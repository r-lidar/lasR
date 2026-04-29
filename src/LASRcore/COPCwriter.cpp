#include "COPCwriter.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <utility>
#include <vector>

#include "laswriter_las.hpp"
#include "lasdefinitions.hpp"
#include "lascopc.hpp"
#include "print.h"

namespace
{
  // Parse LASR_COPC_RESIDENT_BUDGET env var (bytes). Returns 0 if unset or
  // invalid. Mirrors COPCspill's LASR_COPC_RAM_BUDGET — they're independent
  // budgets (writer's in-RAM voxel residents vs spill's per-cell write
  // buffers) and tuning one without the other can leave the other as the
  // bottleneck on huge files.
  std::uint64_t env_resident_budget()
  {
    const char* v = std::getenv("LASR_COPC_RESIDENT_BUDGET");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (std::uint64_t)n;
  }

  // Parse LASR_COPC_MEMORY_BUDGET env var (bytes). Returns 0 if unset or
  // invalid. This is the user-facing single knob — the writer splits it
  // internally into resident / pre-spill RAM / write-buffer / reserve so
  // operators don't have to juggle three independent budgets that all
  // need tuning together. The split fractions (35/35/8/22) are picked so
  // the implicit 22% reserve covers metadata, hash-table nodes, allocator
  // fragmentation, LASZip encoder state, and other non-budgeted overhead.
  // Specific per-component env vars still win if set — this is the
  // fallback when only the global knob is provided.
  std::uint64_t env_memory_budget()
  {
    const char* v = std::getenv("LASR_COPC_MEMORY_BUDGET");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (std::uint64_t)n;
  }

  // Replicate COPCspill's env helpers here so the writer can apply the
  // same precedence (specific > global-derived > default) and pass
  // resolved values to the spill ctor. COPCspill itself still re-reads
  // its own env vars and they win — this is just so the global knob
  // works without a build-time coupling to spill internals.
  std::uint64_t env_ram_budget_writer_side()
  {
    const char* v = std::getenv("LASR_COPC_RAM_BUDGET");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (std::uint64_t)n;
  }
  std::uint64_t env_wb_budget_writer_side()
  {
    const char* v = std::getenv("LASR_COPC_SPILL_WRITE_BUDGET");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (std::uint64_t)n;
  }

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

  // FNV-1a 64-bit over the point bytes. Used to break ties when two points
  // contend for the same voxel cell — the higher hash wins. A pure function
  // of the point bytes, so two runs over identical input produce identical
  // routing decisions (byte-stable output, asserted by the round-trip test).
  // Independence from input order removes the bias of "first arriving wins"
  // that scanline-ordered inputs would otherwise introduce.
  inline uint64_t point_hash(const U8* bytes, std::size_t n)
  {
    uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < n; ++i)
    {
      h ^= bytes[i];
      h *= 1099511628211ULL;
    }
    return h;
  }

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

  U8 src_pdrf = source_header->point_data_format;

  // Refuse waveform PDRFs (4, 5, 9, 10). Their point records carry
  // wavepacket descriptor fields that don't exist in COPC's allowed
  // formats (6/7/8); blindly promoting would silently strip them.
  // PDRF 4/5 also assume waveform payload elsewhere (in a separate VLR
  // or external file) which the COPC spec doesn't support. Better to
  // refuse loudly than to write a file that has lost waveform metadata.
  if (src_pdrf == 4 || src_pdrf == 5 || src_pdrf == 9 || src_pdrf == 10)
  {
    char msg[512];
    std::snprintf(msg, sizeof(msg),
      "COPC writer cannot accept waveform point data format %u: COPC requires "
      "PDRF 6, 7, or 8, and promoting from waveform formats would silently "
      "drop the wavepacket descriptor. Strip waveform fields upstream or "
      "convert to a non-waveform PDRF before writing COPC.",
      (unsigned)src_pdrf);
    fail(msg);
    return false;
  }

  // Promote PDRF: legacy formats get upgraded to 6/7/8.
  U8 target_pdrf = 6;
  if (src_pdrf == 2 || src_pdrf == 3 || src_pdrf == 7) target_pdrf = 7;
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
  // The 10-cap matches LASlib's safety on the auto heuristic — explicit
  // user values are honoured up to HARD_DEPTH_LIMIT (we still clamp to the
  // structural ceiling because routing/spill machinery has been validated
  // there). The hierarchy's depth field is mostly cosmetic now since
  // routing decides the actual depth per-point; it still carries the
  // user/auto value for downstream metadata.
  I32 max_depth = copc_depth_user_set ? copc_depth
                : EPToctree::compute_max_depth(*copc_header, (U64)max_points_per_octant);
  if (copc_depth_user_set && max_depth > HARD_DEPTH_LIMIT)
  {
    warning("user-specified max_depth=%d exceeds the writer's hard "
            "depth limit (%d); clamping. The output will use the "
            "deepest supported octree level.\n",
            (int)max_depth, (int)HARD_DEPTH_LIMIT);
    max_depth = HARD_DEPTH_LIMIT;
  }
  if (max_depth > HARD_DEPTH_LIMIT) max_depth = HARD_DEPTH_LIMIT;
  if (max_depth < 0)                max_depth = 0;
  if (!copc_depth_user_set && max_depth > 10) max_depth = 10; // LASlib parity for auto

  // Routing cap, three-way:
  //   - user-set:                  hard cap at max_depth, no bumping. The
  //     user asked for an exact depth, honour it (forced LOD).
  //   - auto, max_depth == 0:      single-chunk fast path. The auto
  //     heuristic decided the whole file fits under cap at root, so emit
  //     one chunk holding everything (matches LAStools' behaviour and
  //     avoids ~17 chunks of artificial hierarchy + per-chunk LAZ
  //     overhead on small inputs). At this size the LOD benefit is weak
  //     anyway — the depth-0 sample is most of the file. Users who want
  //     LOD on small inputs can force it via explicit max_depth > 0.
  //   - auto, max_depth > 0:       progressive LOD with adaptive depth
  //     bumping up to HARD_DEPTH_LIMIT to keep chunks under cap.
  routing_max_depth = copc_depth_user_set
                        ? max_depth
                        : (max_depth == 0 ? 0 : HARD_DEPTH_LIMIT);

  // Memory budgeting, three layers of precedence:
  //   1. LASR_COPC_<COMPONENT>_BUDGET  — wins for that component
  //   2. LASR_COPC_MEMORY_BUDGET / split — convenience knob, splits one
  //      total across resident/ram/wb with an implicit ~22% reserve for
  //      hash-table nodes, fragmentation, LASZip state, R baseline, etc.
  //   3. Built-in defaults (256 MB resident, 256 MB pre-spill, 128 MB wb)
  //
  // Split fractions of MEMORY_BUDGET when used:
  //   - resident: 35%
  //   - pre-spill RAM: 35%
  //   - write buffers: 8%
  //   - reserve: 22% (implicit, not allocated to a component)
  // 78% explicit + 22% reserve. Chosen so the reserve covers what the
  // explicit budgets don't track: per-octant cell_to_idx nodes,
  // unordered_map bucket overhead, allocator fragmentation, and the
  // close-time sort buffer for the largest emitted chunk.
  std::uint64_t mem_budget    = env_memory_budget();
  std::uint64_t resident_env  = env_resident_budget();
  std::uint64_t ram_env       = env_ram_budget_writer_side();
  std::uint64_t wb_env        = env_wb_budget_writer_side();

  if (resident_env > 0)
    resident_budget = resident_env;
  else if (mem_budget > 0)
    resident_budget = (std::uint64_t)((double)mem_budget * 0.35);
  // else keep built-in default

  // Compute spill ctor args (COPCspill::ctor still re-reads its own env
  // vars and they win — this is just so MEMORY_BUDGET works without a
  // build-time coupling). Use COPCspill's own DEFAULT_* constants as the
  // fallback so values stay in sync if those defaults move.
  std::uint64_t spill_ram = (ram_env > 0) ? ram_env
                          : (mem_budget > 0
                              ? (std::uint64_t)((double)mem_budget * 0.35)
                              : COPCspill::DEFAULT_RAM_BUDGET);
  std::uint64_t spill_wb  = (wb_env > 0) ? wb_env
                          : (mem_budget > 0
                              ? (std::uint64_t)((double)mem_budget * 0.08)
                              : COPCspill::DEFAULT_WRITE_BUF_BUDGET);

  // Optional one-shot diagnostic: when LASR_COPC_DEBUG_BUDGETS is set,
  // print the resolved (resident, ram, wb) trio so tests and operators
  // can verify that LASR_COPC_MEMORY_BUDGET was actually parsed and
  // split. Cheap and avoids exposing budget internals through the
  // public R API.
  if (const char* dbg = std::getenv("LASR_COPC_DEBUG_BUDGETS"); dbg && *dbg)
  {
    warning("COPC budgets resolved: resident=%llu spill_ram=%llu spill_wb=%llu (mem=%llu)\n",
            (unsigned long long)resident_budget,
            (unsigned long long)spill_ram,
            (unsigned long long)spill_wb,
            (unsigned long long)mem_budget);
  }

  hierarchy = new COPChierarchy(*copc_header, max_depth, copc_density);
  spill = new COPCspill(output_path, copc_header->point_data_record_length,
                        spill_ram,
                        COPCspill::DEFAULT_WRITE_BUF_SIZE,
                        COPCspill::DEFAULT_FD_CAP,
                        COPCspill::DEFAULT_CHECK_CADENCE,
                        spill_wb);

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

  // Serialize the point bytes once. Both the routing path (in-RAM resident)
  // and the leaf-fallback path (spill->append) consume from this buffer.
  const U32 point_size = copc_header->point_data_record_length;
  std::vector<U8> bytes(point_size);
  effective->copy_to(bytes.data());
  const U64 hash = point_hash(bytes.data(), point_size);

  // Top-down voxel routing with hash-based eviction (caveats 1 + 2).
  return route_or_spill(std::move(bytes), hash, /*start_depth=*/0);
}

// Route a point's bytes into the in-RAM occupancy tables, evicting and
// re-routing any losing residents.
//
// Three refinements over plain "first-hit-wins" voxel claim:
//   1. Hard chunk cap — an octant only accepts new claims while its voxel
//      count is below max_points_per_octant. Once full, points descend to
//      the next depth. Keeps every internal LAZ chunk bounded by the cap.
//   2. Hash-based replacement — on collision the higher hash wins; the
//      losing resident is evicted and re-routed starting at depth+1. The
//      hash is a deterministic function of the point bytes (FNV-1a) so
//      two runs produce byte-identical output, but the result is
//      independent of input order.
//   3. Adaptive depth bump (auto only) — when copc_depth was left auto,
//      routing_max_depth is HARD_DEPTH_LIMIT so the loop can walk past
//      the auto-heuristic max_depth. An explicit user max_depth is
//      treated as a hard cap (routing_max_depth = user value); points
//      that exhaust every voxel up to the cap are force-accepted at the
//      cap, producing a chunk that may exceed max_points_per_octant.
//      Either way the leaf chunk size is surfaced by the
//      end-of-finalize oversize warning.
// Per-voxel overhead used in budget accounting with the packed HotOctant
// layout — the cell_to_idx unordered_map node (int key + U32 value +
// next-ptr + cached hash + amortised bucket-array share). The three
// parallel arrays (bytes/cells/hashes) are accounted via *capacity
// deltas* on each push_back, which captures vector geometric growth
// (capacity typically doubles) rather than just logical size. The
// previous flat-overhead approach undercounted real RAM by up to 2×
// because of that growth.
static constexpr std::size_t RESIDENT_NODE_OVERHEAD = 64;

bool COPCwriter::route_or_spill(std::vector<U8>&& bytes_in, U64 hash, I32 start_depth)
{
  std::vector<U8> bytes = std::move(bytes_in);
  const I32 cap = max_points_per_octant > 0 ? max_points_per_octant : 100000;
  const std::size_t point_size = (std::size_t)copc_header->point_data_record_length;

  bool inserted_resident = false;
  for (;;)
  {
    bool placed = false;
    for (I32 d = start_depth; d < routing_max_depth; ++d)
    {
      // Decode the bytes back into our scratch LASpoint so we can compute
      // octant key / voxel cell. Cheap: same copy_from used elsewhere.
      point->copy_from(bytes.data());
      EPTkey k_d = hierarchy->compute_key_at(point, d);

      // Flushed octants no longer accept claims — descend.
      if (flushed_octants.count(k_d)) continue;

      I32 cell_d = hierarchy->compute_voxel_cell(point, k_d);
      if (cell_d < 0) continue;

      auto& oct = occupancy[k_d];
      auto it = oct.cell_to_idx.find(cell_d);
      if (it == oct.cell_to_idx.end())
      {
        // Voxel free. Claim it only if the octant still has cap room;
        // otherwise descend further (chunk-size cap).
        if ((I32)oct.cells.size() >= cap) continue;
        // Snapshot capacities before insertion so we can attribute any
        // vector growth (which doubles capacity) to this voxel's account.
        const std::size_t cap_before =
            oct.bytes.capacity() +
            oct.cells.capacity()  * sizeof(I32) +
            oct.hashes.capacity() * sizeof(U64);

        const U32 idx = (U32)oct.cells.size();
        oct.cells.push_back(cell_d);
        oct.hashes.push_back(hash);
        oct.bytes.insert(oct.bytes.end(), bytes.begin(), bytes.end());
        oct.cell_to_idx.emplace(cell_d, idx);

        const std::size_t cap_after =
            oct.bytes.capacity() +
            oct.cells.capacity()  * sizeof(I32) +
            oct.hashes.capacity() * sizeof(U64);
        const std::size_t added = (cap_after - cap_before) + RESIDENT_NODE_OVERHEAD;
        resident_bytes += added;
        octant_bytes[k_d] += added;
        inserted_resident = true;
        placed = true;
        break;
      }

      const U32 idx = it->second;
      if (hash > oct.hashes[idx])
      {
        // New point wins this voxel. Swap the in-arena slot with the
        // caller-provided `bytes` buffer using std::swap_ranges — no
        // allocation per replacement, no malloc churn. The evicted
        // bytes end up in `bytes` and continue routing from depth+1.
        // Hash is a plain swap.
        U8* slot = oct.bytes.data() + (std::size_t)idx * point_size;
        std::swap_ranges(bytes.data(), bytes.data() + point_size, slot);
        std::swap(hash, oct.hashes[idx]);
        start_depth = d + 1;
        placed = false;
        goto next_iter;
      }
      // Resident wins; descend.
    }

    if (placed) break;

    // Walked all the way to routing_max_depth without acceptance — force-
    // append at the deepest key. For user-set max_depth this is the
    // *expected* terminal state and chunks may legitimately stay under
    // cap, so no intake-time warning here — the end-of-finalize oversize
    // scan reports actual cap violations. For auto mode, hitting the
    // hard depth limit means a pathological cluster exhausted every
    // voxel up to depth 16, which IS the early-warning case worth a
    // one-shot diagnostic.
    // Scoped so the `deepest` declaration doesn't get bypassed by the
    // `goto next_iter` above (clang refuses such jumps).
    {
      point->copy_from(bytes.data());
      EPTkey deepest = hierarchy->compute_key_at(point, routing_max_depth);
      // Only fire on a genuine hard-limit hit. The auto single-chunk
      // fast path (routing_max_depth == 0) routes every point through
      // this branch by design, so we'd spam a misleading warning if we
      // didn't gate on the structural condition.
      if (!hard_depth_warned && routing_max_depth == HARD_DEPTH_LIMIT)
      {
        warning("COPC writer hit the hard depth limit (%d) — a point "
                "cluster exceeds max_points_per_octant at every voxel "
                "up to that depth. Affected octant: depth=%d x=%d y=%d "
                "z=%d. The chunk will exceed cap; see end-of-finalize "
                "oversize warning for the actual size. Subsequent "
                "occurrences are silenced.\n",
                (int)HARD_DEPTH_LIMIT,
                (int)deepest.d, (int)deepest.x, (int)deepest.y, (int)deepest.z);
        hard_depth_warned = true;
      }
      if (!spill->append(deepest, bytes.data()))
      {
        fail(std::string("COPCspill error: ") + spill->last_error());
        return false;
      }
    }
    break;

    next_iter:;
  }

  // Periodically check the budget — only when we just added net-new bytes.
  // Eviction (resident replacement) keeps resident_bytes constant; the leaf
  // fall-through doesn't grow occupancy at all. So no budget check needed
  // on those paths.
  if (inserted_resident && resident_bytes > resident_budget)
  {
    if (!enforce_resident_budget()) return false;
  }
  return true;
}

bool COPCwriter::flush_hot_octant(const EPTkey& key)
{
  auto it = occupancy.find(key);
  if (it == occupancy.end()) return true; // nothing to do
  auto& oct = it->second;

  // Append residents in deterministic order (sorted by cell id) so spill's
  // per-leaf append sequence is byte-stable across runs. With the packed
  // layout, sort indices into the parallel arrays rather than copying
  // (cell, idx) pairs.
  const std::size_t point_size = (std::size_t)copc_header->point_data_record_length;
  const std::size_t n = oct.cells.size();
  std::vector<U32> sort_idx(n);
  std::iota(sort_idx.begin(), sort_idx.end(), 0u);
  std::sort(sort_idx.begin(), sort_idx.end(),
            [&oct](U32 a, U32 b) { return oct.cells[a] < oct.cells[b]; });

  for (U32 i : sort_idx)
  {
    if (!spill->append(key, oct.bytes.data() + (std::size_t)i * point_size))
    {
      fail(std::string("COPCspill error: ") + spill->last_error());
      return false;
    }
  }
  // Free exactly what was added. octant_bytes[key] was incremented by the
  // capacity-delta-aware accounting at insert time, so it includes any
  // vector growth overshoot — using it here keeps resident_bytes balanced
  // even when the vector grew geometrically beyond the logical N.
  auto bb_it = octant_bytes.find(key);
  const std::size_t freed = (bb_it != octant_bytes.end()) ? bb_it->second : 0;
  if (bb_it != octant_bytes.end()) octant_bytes.erase(bb_it);
  occupancy.erase(it);
  flushed_octants.insert(key);
  resident_bytes = (resident_bytes >= freed) ? (resident_bytes - freed) : 0;
  return true;
}

bool COPCwriter::enforce_resident_budget()
{
  // Pick the heaviest hot octant from the incrementally-maintained
  // octant_bytes table (O(N_octants), no inner voxel sum) and flush.
  // Drive resident_bytes below the LOW WATER mark (50% of budget) before
  // returning, so we don't get called again on the very next insert
  // — without hysteresis the previous version triggered per-point on
  // huge inputs and dominated CPU (97% in this function on the sofi
  // run before this fix).
  //
  // Tie-break on (depth, x, y, z) when multiple octants share the
  // heaviest size: flushing freezes that octant's voxel decisions and
  // any later routing that would have hit it now descends instead, so
  // a non-deterministic pick here would propagate into the output
  // bytes. unordered_map iteration order is implementation-defined.
  auto key_less = [](const EPTkey& a, const EPTkey& b) {
    if (a.d != b.d) return a.d < b.d;
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    return a.z < b.z;
  };
  // Single-pass batch flush. The previous "find heaviest, flush, repeat"
  // loop was O(N_octants^2) per enforce trigger when octants were
  // similarly-sized: each iteration scanned the whole map and only freed
  // ~1/N of the budget, requiring O(N) scans of an O(N) map. On the sofi
  // input (~50k active octants) this dominated CPU at intake despite the
  // incremental octant_bytes tracker.
  //
  // Snapshot+sort once, then drain from heaviest until under low_water.
  // O(N log N) per call; typically called only a handful of times for an
  // entire write because hysteresis prevents re-trigger on each insert.
  // Low_water at 40% (was 50%) leaves an extra 10% as headroom for
  // metadata, fragmentation, and transient peaks during finalize.
  const std::uint64_t low_water = (std::uint64_t)((double)resident_budget * 0.4);
  if (resident_bytes <= low_water || octant_bytes.empty()) return true;

  std::vector<std::pair<std::uint64_t, EPTkey>> sorted;
  sorted.reserve(octant_bytes.size());
  for (const auto& kv : octant_bytes) sorted.emplace_back(kv.second, kv.first);
  // Heaviest first; (depth, x, y, z) tie-break preserves determinism.
  std::sort(sorted.begin(), sorted.end(),
            [&key_less](const auto& a, const auto& b) {
              if (a.first != b.first) return a.first > b.first;
              return key_less(a.second, b.second);
            });

  for (const auto& kv : sorted)
  {
    if (resident_bytes <= low_water) break;
    if (kv.first == 0) break;
    if (!flush_hot_octant(kv.second)) return false;
  }
  return true;
}

bool COPCwriter::finalize_and_write()
{
  if (poisoned) return false;
  if (!writer_las || !hierarchy || !spill) { fail("writer not open"); return false; }

  // Flush all remaining in-RAM voxel residents to spill, keyed by their
  // octant. After this step, COPCspill holds the complete per-octant byte
  // stream and the rest of finalize (collapse / sort / chunk emit) is
  // unchanged. Iterate hot octants in deterministic order so spill's per-leaf
  // append sequence is stable across runs (the within-chunk std::stable_sort
  // below makes this strictly belt-and-braces, but the comparator only sorts
  // on gpstime/channel/return — points with identical keys still need a
  // deterministic incoming order). Reuses flush_hot_octant for the actual
  // append loop so the close-time fold and the intake-time RAM-pressure
  // flush share one ordering convention.
  {
    std::vector<EPTkey> octant_keys;
    octant_keys.reserve(occupancy.size());
    for (const auto& kv : occupancy) octant_keys.push_back(kv.first);
    std::sort(octant_keys.begin(), octant_keys.end(),
              [](const EPTkey& a, const EPTkey& b) {
                if (a.d != b.d) return a.d < b.d;
                if (a.x != b.x) return a.x < b.x;
                if (a.y != b.y) return a.y < b.y;
                return a.z < b.z;
              });
    for (const EPTkey& k : octant_keys)
    {
      if (!flush_hot_octant(k)) return false;
    }
  }

  const U32 point_size = copc_header->point_data_record_length;

  // 1) Collapse the octree based on per-leaf counts.
  auto leaf_counts = spill->cell_counts();
  hierarchy->finalize(leaf_counts, min_points_per_chunk, max_points_per_octant);

  // 2) Iterate final octants in deterministic order. Emit real chunks for
  //    point_count > 0; record zero-size entries for placeholder ancestors.
  //    Copy the emit order up front because record_chunk mutates entries.
  std::vector<COPChierarchy::FinalOctant> emit_copy = hierarchy->emit_order();

  // Up-front oversize scan. Voxel-routing + cap-aware collapse keep every
  // routed chunk under max_points_per_octant; a force-accept at the
  // routing depth cap (user max_depth in user-set mode, HARD_DEPTH_LIMIT
  // in auto mode) is the only way a chunk gets here over cap. Warn BEFORE
  // allocating any sort buffer below — that allocation is the OOM risk
  // surface, and the user needs the diagnostic before the process dies.
  if (max_points_per_octant > 0)
  {
    U64 largest_chunk = 0;
    EPTkey largest_chunk_key;
    for (const auto& o : emit_copy)
    {
      if (o.point_count > largest_chunk)
      {
        largest_chunk = o.point_count;
        largest_chunk_key = o.key;
      }
    }
    if (largest_chunk > (U64)max_points_per_octant)
    {
      const char* mitigation = copc_depth_user_set
        ? "Raise max_depth (or leave it auto for adaptive bumping) "
          "or accept the larger chunk."
        : "A point cluster exhausted the hard depth limit; either "
          "lower the cluster density or accept the larger chunk.";
      warning("largest COPC chunk has %llu points, exceeding the "
              "max_points_per_octant cap of %d (chunk at depth=%d, "
              "x=%d, y=%d, z=%d). Close-time sort will allocate ~%llu MB. "
              "%s\n",
              (unsigned long long)largest_chunk,
              (int)max_points_per_octant,
              (int)largest_chunk_key.d,
              (int)largest_chunk_key.x,
              (int)largest_chunk_key.y,
              (int)largest_chunk_key.z,
              (unsigned long long)((largest_chunk * (U64)point_size) >> 20),
              mitigation);
    }
  }

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

  // 5) Drive the header/VLR/eVLR flush + patch via LASlib. With
  //    use_inventory=TRUE, LASinventory::update_header overwrites the
  //    public-header bbox from the accumulated point inventory. For the
  //    no-points case the inventory is empty and the resulting bbox would
  //    be inverted (max_X starts at INT_MIN, min_X at INT_MAX), giving a
  //    nonsensical declared bbox in the output header even though the
  //    COPC info VLR has the right values. Disable use_inventory when no
  //    points were written so the declared bbox carried in copc_header
  //    survives the flush.
  const BOOL use_inventory = have_any_point ? TRUE : FALSE;
  if (!writer_las->update_header(copc_header, use_inventory, TRUE))
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
