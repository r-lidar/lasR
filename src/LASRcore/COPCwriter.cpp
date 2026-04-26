#include "COPCwriter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "laswriter_las.hpp"
#include "lasdefinitions.hpp"
#include "lascopc.hpp"

namespace
{
  // Extract the three fields (GPS time, scanner channel, return number) used
  // as the within-chunk sort key. Layout matches PDRF 6/7/8 point records.
  // Same semantics as the helpers in laswriter_copc.cpp:50-52.
  inline F64 point_gps_time(const U8* p)      { return *reinterpret_cast<const F64*>(&p[22]); }
  inline U8  point_scanner_channel(const U8* p) { return (U8)((p[15] >> 4) & 0x03); }
  inline U8  point_return_number(const U8* p)   { return (U8)(p[14] & 0x0F); }

  // Strict-weak-ordering comparator used by std::sort. Replaces the current
  // qsort comparator in laswriter_copc.cpp. Sorts chunk points to improve LAZ
  // compression (same key order: GPS time, scanner channel, return number).
  struct PointLess
  {
    U32 size;
    bool operator()(const U8* a, const U8* b) const
    {
      F64 ta = point_gps_time(a), tb = point_gps_time(b);
      if (ta != tb) return ta < tb;
      U8 ca = point_scanner_channel(a), cb = point_scanner_channel(b);
      if (ca != cb) return ca < cb;
      return point_return_number(a) < point_return_number(b);
    }
  };

  // Number of "base" bytes for each legacy PDRF (format < 6). Anything beyond
  // this in record_length is extra-bytes storage.
  int base_length_for_pdrf(U8 pdrf)
  {
    switch (pdrf)
    {
      case 0:  return 20;
      case 1:  return 28;
      case 2:  return 26;
      case 3:  return 34;
      case 4:  return 57;
      case 5:  return 63;
      case 6:  return 30;
      case 7:  return 36;
      case 8:  return 38;
      case 9:  return 59;
      case 10: return 67;
      default: return -1;
    }
  }
}

COPCwriter::COPCwriter() = default;

COPCwriter::~COPCwriter()
{
  if (writer_las)
  {
    // Close (idempotent-ish) and delete.
    delete writer_las;
    writer_las = nullptr;
  }
  delete point;
  delete copc_header;
  delete hierarchy;
  delete spill;
}

bool COPCwriter::prepare_copc_header(const LASheader* source_header)
{
  copc_header = new LASheader;
  *copc_header = *source_header;
  copc_header->unlink();

  // Promote PDRF: legacy formats get upgraded to 6/7/8.
  U8 target_pdrf = 6;
  U8 src_pdrf = source_header->point_data_format;
  if (src_pdrf == 2 || src_pdrf == 3 || src_pdrf == 5 || src_pdrf == 7) target_pdrf = 7;
  if (src_pdrf == 8) target_pdrf = 8;
  copc_header->point_data_format = target_pdrf;

  // Header-size bump for <1.4 inputs. Matches current writer.
  if (source_header->version_minor < 3)
  {
    copc_header->header_size += (8 + 140);
    copc_header->offset_to_point_data += (8 + 140);
    copc_header->start_of_waveform_data_packet_record = 0;
  }
  else if (source_header->version_minor == 3)
  {
    copc_header->header_size += 140;
    copc_header->offset_to_point_data += 140;
  }

  // Version bump to 1.4: move legacy point counts to the extended fields.
  if (source_header->version_minor < 4)
  {
    copc_header->version_minor = 4;
    copc_header->extended_number_of_point_records = copc_header->number_of_point_records;
    copc_header->number_of_point_records = 0;
    for (U32 i = 0; i < 5; i++)
    {
      copc_header->extended_number_of_points_by_return[i] = copc_header->number_of_points_by_return[i];
      copc_header->number_of_points_by_return[i] = 0;
    }
  }

  // Recompute point_data_record_length for the target PDRF, preserving any
  // extra-bytes tail from the source.
  if (src_pdrf < 6 || src_pdrf > 8)
  {
    int src_base = base_length_for_pdrf(src_pdrf);
    if (src_base < 0)
    {
      fail("unsupported source point data format");
      return false;
    }
    I32 num_extra = (I32)source_header->point_data_record_length - src_base;
    if (num_extra < 0)
    {
      fail("source point record shorter than required for its PDRF");
      return false;
    }
    copc_header->clean_laszip();
    int tgt_base = base_length_for_pdrf(target_pdrf);
    copc_header->point_data_record_length = (U16)(tgt_base + num_extra);
  }

  // LASpoint for per-call format conversion into the target PDRF.
  point = new LASpoint;
  point->init(copc_header, copc_header->point_data_format, copc_header->point_data_record_length);

  // Placeholders — COPC info VLR and EPT hierarchy eVLR. Populated at close().
  LASvlr_copc_info* info = new LASvlr_copc_info[1];
  std::memset(info, 0, sizeof(LASvlr_copc_info));
  copc_header->add_vlr("copc", 1, sizeof(LASvlr_copc_info), (U8*)info, FALSE, "copc info");
  copc_header->add_evlr("copc", 1000, 0, 0, FALSE, "EPT hierarchy");

  // Deep-copy the source VLRs/eVLRs into our transformed header, so the caller
  // can destroy the source header independently.
  for (U32 i = 0; i < source_header->number_of_variable_length_records; i++)
  {
    const LASvlr& vlr = source_header->vlrs[i];
    U8* data = new U8[vlr.record_length_after_header];
    std::memcpy(data, vlr.data, vlr.record_length_after_header);
    copc_header->add_vlr(vlr.user_id, vlr.record_id, vlr.record_length_after_header,
                          data, FALSE, vlr.description);
  }
  for (U32 i = 0; i < source_header->number_of_extended_variable_length_records; i++)
  {
    const LASevlr& evlr = source_header->evlrs[i];
    U8* data = new U8[evlr.record_length_after_header];
    std::memcpy(data, evlr.data, evlr.record_length_after_header);
    copc_header->add_evlr(evlr.user_id, evlr.record_id, evlr.record_length_after_header,
                           data, FALSE, evlr.description);
  }

  return true;
}

bool COPCwriter::open(const char* file_name, const LASheader* source_header, I32 io_buffer_size)
{
  if (poisoned) return false;
  output_path = file_name;

  if (!prepare_copc_header(source_header)) return false;

  writer_las = new LASwriterLAS;
  if (!writer_las->open(file_name, copc_header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, 2, 0, io_buffer_size))
  {
    fail(std::string("cannot open LASwriterLAS for ") + file_name);
    delete writer_las;
    writer_las = nullptr;
    return false;
  }

  write_scratch.resize(copc_header->point_data_record_length);

  // Hierarchy: EPToctree is built from the (now COPC-shaped) header; max depth
  // is either user-specified or derived from the point count / octant budget.
  I32 max_depth = (copc_depth < 0)
                    ? EPToctree::compute_max_depth(*copc_header, (U64)max_points_per_octant)
                    : copc_depth;
  if (max_depth > 10) max_depth = 10;
  if (max_depth < 0)  max_depth = 0;

  hierarchy = new COPChierarchy(*copc_header, max_depth, copc_density);
  spill = new COPCspill(output_path, copc_header->point_data_record_length);

  gpstime_minimum =  1e300;
  gpstime_maximum = -1e300;
  have_any_point = false;

  return true;
}

bool COPCwriter::write_point(const LASpoint* p)
{
  if (poisoned || !spill || !hierarchy || !writer_las) return false;

  // Format-convert through our internal LASpoint (handles PDRF upgrade).
  *point = *p;

  F64 t = point->get_gps_time();
  if (!have_any_point || t < gpstime_minimum) gpstime_minimum = t;
  if (!have_any_point || t > gpstime_maximum) gpstime_maximum = t;
  have_any_point = true;

  // Track per-point stats into LASwriterLAS's inventory. The points won't
  // actually reach LASwriterLAS::write_point() until finalize_and_write(), but
  // the inventory is used by update_header(use_inventory=TRUE) to populate the
  // output header's bbox and point counts — so we must update it here, per
  // intake point, not at emit time (which would double-count everything).
  writer_las->update_inventory(point);

  // Compute the max-depth leaf key from the already-converted point.
  EPTkey key = hierarchy->compute_leaf_key(point);

  // Serialize the converted point into our reusable scratch, then hand to spill.
  point->copy_to(write_scratch.data());
  if (!spill->append(key, write_scratch.data()))
  {
    fail(std::string("COPCspill error: ") + spill->last_error());
    return false;
  }
  return true;
}

bool COPCwriter::finalize_and_write()
{
  if (poisoned) return false;
  if (!writer_las || !hierarchy || !spill) { fail("writer not open"); return false; }

  const U32 point_size = copc_header->point_data_record_length;

  // 1) Collapse the octree based on per-leaf counts.
  auto leaf_counts = spill->cell_counts();
  hierarchy->finalize(leaf_counts, min_points_per_chunk);

  // 2) Iterate final octants in deterministic order. Emit real chunks for
  //    point_count > 0; record zero-size entries for placeholder ancestors.
  //    Copy the emit order up front because record_chunk mutates entries.
  std::vector<COPChierarchy::FinalOctant> emit_copy = hierarchy->emit_order();

  std::vector<U8> sort_buf;
  std::vector<U8*> ptrs;
  std::vector<U8>  scratch;
  scratch.resize(point_size);

  for (const auto& o : emit_copy)
  {
    if (o.point_count == 0)
    {
      hierarchy->record_chunk(o.key, 0, 0, 0);
      continue;
    }

    const U64 bytes = (U64)o.point_count * (U64)point_size;
    sort_buf.resize(bytes);

    if (!spill->read_octant(o.leaves, sort_buf.data(), bytes))
    {
      fail(std::string("COPCspill read failed: ") + spill->last_error());
      return false;
    }

    // Sort via indirection over fixed-size records. Build a pointer vector,
    // sort pointers by key, then reshuffle the bytes into place in the same
    // sort_buf (one-pass, no extra allocations beyond ptrs).
    ptrs.resize(o.point_count);
    for (U64 i = 0; i < o.point_count; i++) ptrs[i] = sort_buf.data() + i * point_size;
    std::sort(ptrs.begin(), ptrs.end(), PointLess{point_size});

    // In-place permutation: pull records out in sorted order into a second
    // scratch buffer (same size) and swap buffers. Simpler and correct vs
    // trying to permute in place across variable-size swaps.
    std::vector<U8> sorted_buf(bytes);
    for (U64 i = 0; i < o.point_count; i++)
      std::memcpy(sorted_buf.data() + i * point_size, ptrs[i], point_size);
    sort_buf.swap(sorted_buf);

    // Write the chunk. Offset is captured before the first point of the chunk;
    // chunk() commits and we measure size by the delta.
    const I64 chunk_offset = writer_las->tell();
    for (U64 i = 0; i < o.point_count; i++)
    {
      point->copy_from(sort_buf.data() + i * point_size);
      if (!writer_las->write_point(point))
      {
        fail("LASwriterLAS::write_point failed");
        return false;
      }
    }
    if (!writer_las->chunk())
    {
      fail("LASwriterLAS::chunk failed");
      return false;
    }
    const I32 chunk_size = (I32)(writer_las->tell() - chunk_offset);
    hierarchy->record_chunk(o.key, (U64)chunk_offset, chunk_size, (I32)o.point_count);

    // Release spill resources for this octant.
    spill->drop_octant(o.leaves);
  }

  // 3) Install the hierarchy entries into the COPC eVLR placeholder.
  const auto& entries = hierarchy->build_evlr_entries();
  LASvlr_copc_entry* ev_data = new LASvlr_copc_entry[entries.size()];
  for (size_t i = 0; i < entries.size(); i++) ev_data[i] = entries[i];
  copc_header->evlrs[0].record_length_after_header = (I64)(entries.size() * sizeof(LASvlr_copc_entry));
  copc_header->evlrs[0].data = (U8*)ev_data;

  // 4) Fill COPC info VLR fields we know now. LASwriterLAS::update_header
  //    will compute and patch root_hier_offset / root_hier_size based on the
  //    eVLR position (see laswriter_las.cpp:1250-1383), so leave those zero.
  LASvlr_copc_info* info = (LASvlr_copc_info*)copc_header->vlrs[0].data;
  if (!have_any_point)
  {
    gpstime_minimum = 0.0;
    gpstime_maximum = 0.0;
  }
  hierarchy->fill_copc_info(info, gpstime_minimum, gpstime_maximum, 0, 0);

  // 5) Drive the header/VLR/eVLR flush + patch via LASlib.
  if (!writer_las->update_header(copc_header, TRUE, TRUE))
  {
    fail("LASwriterLAS::update_header failed");
    return false;
  }
  return true;
}

I64 COPCwriter::close()
{
  if (closed) return 0;
  closed = true;

  I64 total = 0;

  if (!poisoned && writer_las && hierarchy && spill)
  {
    if (!finalize_and_write())
    {
      // finalize failed — fall through to close()/cleanup so we don't leak.
    }
    total = writer_las->close(TRUE);
  }
  else if (writer_las)
  {
    total = writer_las->close(TRUE);
  }

  delete writer_las; writer_las = nullptr;
  if (spill) { spill->cleanup(); }
  return total;
}

I64 COPCwriter::tell()
{
  return writer_las ? writer_las->tell() : 0;
}

void COPCwriter::fail(const std::string& msg)
{
  poisoned = true;
  if (error_msg.empty()) error_msg = msg;
}
