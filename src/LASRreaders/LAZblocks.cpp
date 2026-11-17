#include "LAZblocks.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "lasmessage.hpp"
#include "lasreader.hpp"
#include "lasreader_las.hpp"
#include "lasreadermerged.hpp"
#include "lasreadpoint.hpp"
#include "lasfilter.hpp"
#include "lasindex.hpp"
#include "lascopc.hpp"

namespace
{
  using Intervals = std::vector<std::pair<I64, I64>>;

  // A block holds about this number of points
  constexpr I64 target_points = 50000;

  // LASlib throws its errors (see byebye()), a destructor must not
  struct Closer { void operator()(LASreader* r) const { try { r->close(); } catch (...) {} delete r; } };
  using ReaderPtr = std::unique_ptr<LASreader, Closer>;

  // The files were already opened by the main reader, which printed their messages. The threads must
  // not print anyway. An error is still thrown.
  struct Mute { Mute() { las_mute_thread(true); } ~Mute() { las_mute_thread(false); } };

  // The fields of a LASpoint that the decoders write: X to the wave packet, which are contiguous, and
  // the extra bytes. Copying the fields, not the LAS record (LASpoint::copy_to()), keeps the legacy
  // fields that the decoders derive from LAS 1.4 points.
  std::size_t fields_size(const LASpoint& p)
  {
    return reinterpret_cast<const U8*>(&p.wavepacket + 1) - reinterpret_cast<const U8*>(&p.X);
  }

  // The spatial query of the main reader, as LASreader tests it
  bool in_query(const LASreader& main, const LASpoint& p)
  {
    switch (main.get_inside())
    {
    case 2:  return p.inside_circle(main.get_c_center_x(), main.get_c_center_y(), main.get_c_radius() * main.get_c_radius());
    case 3:  return p.inside_rectangle(main.get_r_min_x(), main.get_r_min_y(), main.get_r_max_x(), main.get_r_max_y());
    default: return true;
    }
  }

  // The first point of each chunk of a file, then its number of points: the chunk table of LASzip for
  // a LAZ file, any cut for a LAS file. Empty when the table is missing or incomplete (e.g. a truncated
  // file): LASzip must then read the file sequentially.
  std::vector<I64> chunk_starts(LASreaderLAS& reader)
  {
    std::vector<I64> starts;
    const I64 n = reader.npoints;
    I64 size = target_points;

    if (reader.header.laszip)
    {
      // The table is loaded by the first seek
      const LASreadPoint* decoder = reader.get_read_point();
      if (decoder == nullptr || !reader.seek(0)) return starts;

      // A complete table has one more start than chunks: the end of the last chunk
      const U32 nchunks = decoder->get_number_chunks();
      if (nchunks == 0 || nchunks == U32_MAX || decoder->get_tabled_chunks() != nchunks + 1) return starts;

      // Variable chunk sizes (COPC included)
      if (const U64* totals = decoder->get_chunk_totals())
      {
        if ((I64)totals[nchunks] == n) starts.assign(totals, totals + nchunks + 1);
        return starts;
      }

      size = decoder->get_chunk_size();
      if ((I64)nchunks != (n + size - 1) / size) return starts;
    }

    for (I64 p = 0 ; p < n ; p += size) starts.push_back(p);
    starts.push_back(n);
    return starts;
  }

  // The intervals of points [first, end) that the sequential reader visits with the spatial index of
  // a file, in the same order. false when it does not use an index: it reads the whole file.
  bool index_intervals(const LASreader& reader, bool depth_query, Intervals& out)
  {
    const bool spatial_query = reader.get_inside() != 0;

    COPCindex* copc = reader.get_copcindex();
    if (copc && (spatial_query || depth_query))
    {
      for (const Range& r : copc->get_points_intervals()) out.emplace_back((I64)r.start, (I64)r.end + 1);
      return true;
    }

    LASindex* lax = reader.get_index();
    if (lax && spatial_query)
    {
      if (lax->get_intervals())
        while (lax->has_intervals()) out.emplace_back((I64)lax->start, (I64)lax->end + 1);
      return true;
    }

    return false;
  }
}

LazBlocks::LazBlocks(LASreadOpener& opener, LASreader& main, int threads, std::size_t record_size, const Converter& convert) : main(main)
{
#ifdef __EMSCRIPTEN__
  throw std::runtime_error("threads are not available on this platform");
#endif

  // The workers read the files with a plain LASreaderLAS. This excludes what changes how or which
  // points LASlib reads. A transform may depend on the order of the points.
  if (!dynamic_cast<LASreaderLAS*>(&main) && !dynamic_cast<LASreaderMerged*>(&main))
    throw std::runtime_error("the options '-buffered', '-stored' and '-pipe_on' are not supported");

  if (main.get_transform())
    throw std::runtime_error("LASlib transforms are not supported");

  if (main.get_inside() == 1 || opener.is_offset_adjust() || opener.is_file_formula_filtered() || opener.z_from_attribute || opener.are_files_flightlines() || opener.applying_file_source_ID())
    throw std::runtime_error("a LASlib option is not supported");

  const LASheader& mh = main.header;
  const I32 depth = main.get_copc_depth();
  const F32 resolution = main.get_copc_resolution();
  const bool depth_query = resolution > 0 || (depth != -1 && depth != I32_MAX);

  Mute mute;

  // 1. Each file is opened like the sequential reader opens it, with the same query: LASlib loads its
  //    spatial index (LAX or COPC) and tells which points the sequential reader would visit.
  std::vector<std::vector<I64>> starts;
  std::vector<Intervals> intervals;
  I64 total = 0;

  for (U32 i = 0 ; i < opener.get_file_name_number() ; i++)
  {
    LASreadOpener probe_opener; // must outlive its reader
    probe_opener.set_copc_stream_ordered_by_chunk();
    probe_opener.add_file_name(opener.get_file_name(i));
    if (main.get_inside() == 2) probe_opener.set_inside_circle(main.get_c_center_x(), main.get_c_center_y(), main.get_c_radius());
    if (main.get_inside() == 3) probe_opener.set_inside_rectangle(main.get_r_min_x(), main.get_r_min_y(), main.get_r_max_x(), main.get_r_max_y());
    if (resolution > 0)   probe_opener.set_resolution(resolution);
    else if (depth_query) probe_opener.set_max_depth(depth);

    ReaderPtr probe(probe_opener.open());
    LASreaderLAS* las = dynamic_cast<LASreaderLAS*>(probe.get());
    if (las == nullptr)
      throw std::runtime_error("only LAS and LAZ files can be decoded in parallel");

    const LASheader& h = las->header;
    if (h.point_data_format != mh.point_data_format || h.point_data_record_length != mh.point_data_record_length || h.number_attributes != mh.number_attributes)
      throw std::runtime_error("the files have different point formats");

    File file;
    file.path     = opener.get_file_name(i);
    file.rescale  = h.x_scale_factor != mh.x_scale_factor || h.y_scale_factor != mh.y_scale_factor || h.z_scale_factor != mh.z_scale_factor;
    file.reoffset = h.x_offset != mh.x_offset || h.y_offset != mh.y_offset || h.z_offset != mh.z_offset;
    file.indexed  = false;

    starts.emplace_back();
    intervals.emplace_back();

    // The sequential reader reads no point of a file that the query does not intersect
    if (las->npoints > 0 && !las->is_outside_query())
    {
      starts.back() = chunk_starts(*las);
      if (starts.back().empty())
        throw std::runtime_error("the chunk table of a file is missing or the file is incomplete");

      file.indexed = index_intervals(*las, depth_query, intervals.back());
      if (!file.indexed) intervals.back().emplace_back(0, las->npoints);

      for (const auto& interval : intervals.back())
      {
        if (interval.first < 0 || interval.second > las->npoints)
          throw std::runtime_error("the spatial index of a file does not match the file");
        total += interval.second - interval.first;
      }
    }

    files.push_back(file);
  }

  // 2. The intervals are cut at the chunk boundaries and grouped in blocks. A block starts on a new
  //    chunk once the previous one holds 'target' points: a chunk is never split between two blocks
  //    (two workers would decode its start). A small cloud gets smaller blocks, a few per thread.
  const I64 target = std::max<I64>(1, std::min<I64>(target_points, total / (4 * threads)));

  for (std::size_t f = 0 ; f < files.size() ; f++)
  {
    const std::vector<I64>& st = starts[f];
    Block block{f};
    std::size_t chunk = SIZE_MAX;

    for (const auto& interval : intervals[f])
    {
      I64 p = interval.first;
      while (p < interval.second)
      {
        const std::size_t c = (std::size_t)(std::upper_bound(st.begin(), st.end(), p) - st.begin()) - 1;
        const I64 q = std::min(interval.second, st[c + 1]);

        if (c != chunk && block.count >= target)
        {
          blocks.push_back(std::move(block));
          block = Block{f};
        }

        if (!block.pieces.empty() && block.pieces.back().first + block.pieces.back().second == p)
          block.pieces.back().second += q - p; // contiguous: one piece
        else
          block.pieces.emplace_back(p, q - p);

        block.count += q - p;
        chunk = c;
        p = q;
      }
    }

    if (block.count > 0) blocks.push_back(std::move(block));
  }

  if (blocks.size() < 2)
    throw std::runtime_error("the files are too small");

  // 3. The threads. The window leaves each one a block to decode plus one waiting.
  if (main.get_filter()) raw_size = fields_size(main.point) + main.point.extra_bytes_number;
  stride = raw_size + record_size;

  const std::size_t n = std::min<std::size_t>(threads, blocks.size());
  window = 2 * n;

  try
  {
    for (std::size_t i = 0 ; i < n ; i++) workers.emplace_back(&LazBlocks::work, this, convert);
  }
  catch (...)
  {
    stop();
    throw std::runtime_error("threads cannot be created");
  }
}

LazBlocks::~LazBlocks()
{
  stop();
}

void LazBlocks::stop()
{
  {
    std::lock_guard<std::mutex> lock(mutex);
    stopping = true;
  }
  wakeup.notify_all();
  for (auto& worker : workers) worker.join();
  workers.clear();
}

// A worker takes a block number, decodes the block and starts again until there is no block left.
// It waits while the workers are too far ahead of the consumer. An error is stored in the block, with
// the points read before it: the consumer throws it after these points.
void LazBlocks::work(Converter convert)
{
  Mute mute;
  LASreadOpener opener; // must outlive the reader
  std::unique_ptr<LASreaderLAS, Closer> reader;
  std::size_t opened = SIZE_MAX;

  for (;;)
  {
    std::size_t i;
    {
      std::unique_lock<std::mutex> lock(mutex);
      wakeup.wait(lock, [this] { return stopping || next_job == blocks.size() || next_job < current + window; });
      if (stopping || next_job == blocks.size()) return;
      i = next_job++;
    }

    Block& block = blocks[i];

    try
    {
      // The reader of the file, built like LASreaderMerged builds its readers
      if (block.file != opened)
      {
        const File& file = files[block.file];
        const LASheader& h = main.header;
        opened = SIZE_MAX;

        if (file.rescale && file.reoffset)
          reader.reset(new LASreaderLASrescalereoffset(&opener, h.x_scale_factor, h.y_scale_factor, h.z_scale_factor, h.x_offset, h.y_offset, h.z_offset));
        else if (file.rescale)
          reader.reset(new LASreaderLASrescale(&opener, h.x_scale_factor, h.y_scale_factor, h.z_scale_factor));
        else if (file.reoffset)
          reader.reset(new LASreaderLASreoffset(&opener, h.x_offset, h.y_offset, h.z_offset));
        else
          reader.reset(new LASreaderLAS(&opener));

        if (!reader->open(file.path.c_str()))
          throw std::runtime_error("cannot open '" + file.path + "'");

        opened = block.file;
      }

      decode(block, *reader, convert);
    }
    catch (const std::exception& e)
    {
      block.error = e.what();
      opened = SIZE_MAX; // the reader may be in any state
    }
    catch (...)
    {
      block.error = "unknown error while decoding '" + files[block.file].path + "'";
      opened = SIZE_MAX;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      block.done = true;
    }
    wakeup.notify_all();
  }
}

// No lock: a worker only touches its own reader and its own block.
void LazBlocks::decode(Block& block, LASreaderLAS& reader, Converter& convert) const
{
  const LASpoint& p = reader.point;
  const std::string& path = files[block.file].path;

  block.data.resize(block.count * stride);

  for (const auto& piece : block.pieces)
  {
    // The first piece starts a chunk: nothing is decoded and thrown away
    if (!reader.seek(piece.first))
      throw std::runtime_error("cannot seek in '" + path + "'");

    // The error messages of LASlib count the points read: as if the file had been read up to here
    reader.p_cnt = piece.first;

    for (I64 k = 0 ; k < piece.second ; k++)
    {
      if (!reader.read_point())
        throw std::runtime_error("'" + path + "' has fewer points than its header says");

      if (!in_query(main, p)) continue;

      unsigned char* out = &block.data[block.kept * stride];

      if (raw_size > 0)
      {
        const std::size_t n = fields_size(p);
        std::memcpy(out, &p.X, n);
        if (raw_size > n) std::memcpy(out + n, p.extra_bytes, raw_size - n);
      }

      convert(p, reader.header, out + raw_size);
      block.kept++;
    }
  }

  // Read one more point, as the sequential reader does when it reads a whole file: LASzip checks that
  // a chunk is intact only when it starts the next one, or at the end of the file. Without this read
  // a corrupt chunk would be decoded silently. With a spatial index the sequential reader seeks instead.
  if (!files[block.file].indexed) reader.read_point();
}

const unsigned char* LazBlocks::next()
{
  LASfilter* filter = main.get_filter();

  while (current < blocks.size())
  {
    Block& block = blocks[current];

    if (cursor == 0)
    {
      std::unique_lock<std::mutex> lock(mutex);
      wakeup.wait(lock, [&block] { return block.done; });
    }

    while (cursor < block.kept)
    {
      const unsigned char* record = &block.data[cursor++ * stride];
      main.p_cnt++;

      // The LASlib filter is applied here, in the order of the points, to the decoded point
      if (filter)
      {
        LASpoint& p = main.point;
        const std::size_t n = fields_size(p);
        std::memcpy(&p.X, record, n);
        if (raw_size > n) std::memcpy(p.extra_bytes, record + n, raw_size - n);
        if (filter->filter(&p)) continue;
      }

      return record + raw_size;
    }

    if (!block.error.empty())
      throw std::runtime_error(block.error);

    // Done with the block: it is freed, the workers may go one block further. The points out of
    // the query were read too.
    main.p_cnt += block.count - (I64)block.kept;
    std::vector<unsigned char>().swap(block.data);
    cursor = 0;
    {
      std::lock_guard<std::mutex> lock(mutex);
      current++;
    }
    wakeup.notify_all();
  }

  return nullptr;
}
