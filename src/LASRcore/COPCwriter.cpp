#include "COPCwriter.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "laswriter_las.hpp"
#include "lasdefinitions.hpp"
#include "lascopc.hpp"
#include "print.h"

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

  // Reject only when the entire bbox is degenerate. A flat axis (e.g.
  // a synthetic flat-ground cloud, or a single-elevation airborne strip
  // with min_z == max_z) is valid as long as at least one axis has
  // extent — the COPC root cube takes its halfsize from the largest
  // axis. Degenerate axes are inflated downstream (in prepare_copc_header)
  // to keep EPToctree::get_key's grid_resolution computation safe.
  const bool x_flat = source_header->max_x <= source_header->min_x;
  const bool y_flat = source_header->max_y <= source_header->min_y;
  const bool z_flat = source_header->max_z <= source_header->min_z;
  if (x_flat && y_flat && z_flat)
  {
    char msg[512];
    std::snprintf(msg, sizeof(msg),
      "COPC writer requires a non-degenerate bounding box on at least one "
      "axis; got x=(%g, %g) y=(%g, %g) z=(%g, %g). The input header (or "
      "upstream pipeline) must populate min/max bounds before writing.",
      source_header->min_x, source_header->max_x,
      source_header->min_y, source_header->max_y,
      source_header->min_z, source_header->max_z);
    fail(msg);
    return false;
  }

  if (!prepare_copc_header(source_header)) return false;

  // Inflate any flat axes by a small fraction of the largest extent so
  // EPToctree's per-axis grid_resolution (= extent / grid_size) is never
  // zero. The COPC info VLR's halfsize comes from the largest axis only,
  // so this inflation is invisible to readers.
  {
    const F64 ext_x = copc_header->max_x - copc_header->min_x;
    const F64 ext_y = copc_header->max_y - copc_header->min_y;
    const F64 ext_z = copc_header->max_z - copc_header->min_z;
    const F64 max_ext = std::max(ext_x, std::max(ext_y, ext_z));
    const F64 pad = max_ext * 1e-6;  // tiny but non-zero
    if (ext_x <= 0) { copc_header->max_x = copc_header->min_x + pad; }
    if (ext_y <= 0) { copc_header->max_y = copc_header->min_y + pad; }
    if (ext_z <= 0) { copc_header->max_z = copc_header->min_z + pad; }
  }

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
  data_min_x =  1e300; data_max_x = -1e300;
  data_min_y =  1e300; data_max_y = -1e300;
  data_min_z =  1e300; data_max_z = -1e300;
  have_any_point = false;

  return true;
}

bool COPCwriter::write_point(const LASpoint* p)
{
  if (poisoned || !spill || !hierarchy || !writer_las) return false;

  // Same-format fast path: when the source is already in the target PDRF
  // (modern LAS 1.4 input → PDRF 6/7/8) and the record length matches, use
  // p directly for inventory, key, and bytes. Saves one LASpoint::operator=
  // (a field-by-field copy) per point. The slow path goes through `*point = *p`
  // which is required to upgrade legacy PDRF 0–5 to extended 6+ semantics.
  const LASpoint* effective = p;
  const bool fast_path = (p->extended_point_type == point->extended_point_type)
                      && (p->total_point_size   == point->total_point_size);
  if (!fast_path)
  {
    *point = *p;
    effective = point;
  }

  const F64 x = effective->get_x();
  const F64 y = effective->get_y();
  const F64 z = effective->get_z();
  const F64 t = effective->get_gps_time();
  if (!have_any_point || t < gpstime_minimum) gpstime_minimum = t;
  if (!have_any_point || t > gpstime_maximum) gpstime_maximum = t;
  if (!have_any_point || x < data_min_x) data_min_x = x;
  if (!have_any_point || x > data_max_x) data_max_x = x;
  if (!have_any_point || y < data_min_y) data_min_y = y;
  if (!have_any_point || y > data_max_y) data_max_y = y;
  if (!have_any_point || z < data_min_z) data_min_z = z;
  if (!have_any_point || z > data_max_z) data_max_z = z;
  have_any_point = true;

  // Track per-point stats into LASwriterLAS's inventory. The points won't
  // actually reach LASwriterLAS::write_point() until finalize_and_write(), but
  // the inventory is used by update_header(use_inventory=TRUE) to populate the
  // output header's bbox and point counts — so we must update it here, per
  // intake point, not at emit time (which would double-count everything).
  writer_las->update_inventory(effective);

  // Compute the max-depth leaf key.
  EPTkey key = hierarchy->compute_leaf_key(effective);

  // Serialize to our reusable scratch and hand to spill.
  effective->copy_to(write_scratch.data());
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

    // Sort via indirection over fixed-size records. Build a pointer vector
    // aliased into sort_buf, stable_sort the pointers, then drive
    // LASwriterLAS::write_point directly off the sorted pointers — no
    // second full-size byte buffer.
    // stable_sort: identical (gps_time, channel, return) keys keep their
    // intake order so two runs over the same input produce byte-equivalent
    // chunks. Costs O(N) extra scratch (the pointer vector) and ~20% more
    // sort time vs std::sort.
    ptrs.resize(o.point_count);
    for (U64 i = 0; i < o.point_count; i++) ptrs[i] = sort_buf.data() + i * point_size;
    std::stable_sort(ptrs.begin(), ptrs.end(), PointLess{point_size});

    // Write the chunk. Offset is captured before the first point of the chunk;
    // chunk() commits and we measure size by the delta. Decode through the
    // sorted pointer vector — keeps close-time peak memory at one octant's
    // worth (sort_buf + ptrs), independent of the post-collapse octant size.
    const I64 chunk_offset = writer_las->tell();
    for (U64 i = 0; i < o.point_count; i++)
    {
      point->copy_from(ptrs[i]);
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

  // 6) Validate the declared (octree) bbox against the actual data bbox.
  //    The octree was built at open() from the declared bbox; if a
  //    pipeline stage moved any point outside that bbox, EPToctree::get_key
  //    silently clamped it to the boundary cell — producing a structurally
  //    bad COPC where boundary chunks are over-loaded. Detect this case
  //    and fail loudly: a clamped output is worse than no output.
  //    Also warn (without failing) when the declared bbox is much looser
  //    than the data — the output is correct but the COPC info VLR's
  //    spacing is too coarse for efficient LOD reads.
  if (have_any_point)
  {
    const F64 eps = 1e-9;
    const bool clamped =
        (data_min_x < copc_header->min_x - eps) ||
        (data_max_x > copc_header->max_x + eps) ||
        (data_min_y < copc_header->min_y - eps) ||
        (data_max_y > copc_header->max_y + eps) ||
        (data_min_z < copc_header->min_z - eps) ||
        (data_max_z > copc_header->max_z + eps);
    if (clamped)
    {
      char buf[768];
      std::snprintf(buf, sizeof(buf),
        "COPC writer: at least one point fell outside the octree bbox the "
        "writer was opened with. EPToctree::get_key clamped those points to "
        "the boundary cells, so the resulting COPC has a corrupted spatial "
        "index. This usually means a pipeline stage transformed points "
        "after the writer was opened. "
        "data x=(%g, %g) y=(%g, %g) z=(%g, %g); "
        "octree x=(%g, %g) y=(%g, %g) z=(%g, %g).",
        data_min_x, data_max_x, data_min_y, data_max_y, data_min_z, data_max_z,
        copc_header->min_x, copc_header->max_x,
        copc_header->min_y, copc_header->max_y,
        copc_header->min_z, copc_header->max_z);
      fail(buf);
      return false;
    }

    const F64 declared_vol = (copc_header->max_x - copc_header->min_x) *
                             (copc_header->max_y - copc_header->min_y) *
                             (copc_header->max_z - copc_header->min_z);
    const F64 data_vol     = (data_max_x - data_min_x) *
                             (data_max_y - data_min_y) *
                             (data_max_z - data_min_z);
    if (declared_vol > 0 && data_vol > 0 && data_vol < 0.5 * declared_vol)
    {
      warning("COPC writer: declared bbox is %.1fx larger than data bbox; "
              "the octree is sized to the declared bbox so chunks may be "
              "concentrated in a sub-region of the octree volume. "
              "data x=(%g, %g) y=(%g, %g) z=(%g, %g); "
              "declared x=(%g, %g) y=(%g, %g) z=(%g, %g).\n",
              declared_vol / data_vol,
              data_min_x, data_max_x, data_min_y, data_max_y, data_min_z, data_max_z,
              copc_header->min_x, copc_header->max_x,
              copc_header->min_y, copc_header->max_y,
              copc_header->min_z, copc_header->max_z);
    }
  }

  return true;
}

I64 COPCwriter::close()
{
  if (closed) return poisoned ? -1 : 0;
  closed = true;

  bool finalize_ok = true;
  I64 total = 0;

  if (!poisoned && writer_las && hierarchy && spill)
  {
    finalize_ok = finalize_and_write();
    // Always attempt LASwriterLAS::close so we don't leak its file handle,
    // but ignore its byte count when finalize failed — the file is partial.
    total = writer_las->close(TRUE);
  }
  else if (writer_las)
  {
    finalize_ok = false;  // we never completed finalize
    total = writer_las->close(TRUE);
  }

  delete writer_las; writer_las = nullptr;
  if (spill) { spill->cleanup(); }

  // If finalize or earlier write failed, the on-disk file is corrupt.
  // Unlink it and signal the caller via a -1 return so they can throw.
  if (poisoned || !finalize_ok)
  {
    if (!output_path.empty())
    {
      std::remove(output_path.c_str());
    }
    return -1;
  }
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
