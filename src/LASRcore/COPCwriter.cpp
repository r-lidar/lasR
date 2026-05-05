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
  std::uint64_t env_max_sort_memory()
  {
    const char* v = std::getenv("LASR_COPC_MAX_SORT_MEMORY");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (std::uint64_t)n;
  }
  std::uint32_t env_max_entries_per_page()
  {
    const char* v = std::getenv("LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0 || n > 0xFFFFFFFFu) return 0;
    return (std::uint32_t)n;
  }
  int env_protected_lod_depth()
  {
    const char* v = std::getenv("LASR_COPC_PROTECTED_LOD_DEPTH");
    if (!v || !*v) return -1;
    char* end = nullptr;
    long n = std::strtol(v, &end, 10);
    if (!end || *end != '\0') return -1;
    if (n < -1) return -1;
    if (n > 16) return 16;
    return (int)n;
  }
  int env_xy_lod_depth()
  {
    const char* v = std::getenv("LASR_COPC_XY_LOD_DEPTH");
    if (!v || !*v) return -2;
    char* end = nullptr;
    long n = std::strtol(v, &end, 10);
    if (!end || *end != '\0') return -2;
    if (n < -1) return -2;
    if (n > 16) return 16;
    return (int)n;
  }
  int env_xy_lod_grid_multiplier()
  {
    const char* v = std::getenv("LASR_COPC_XY_LOD_GRID_MULTIPLIER");
    if (!v || !*v) return 0;
    char* end = nullptr;
    long n = std::strtol(v, &end, 10);
    if (!end || *end != '\0' || n <= 0) return 0;
    if (n > 16) return 16;
    return (int)n;
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

  // Endianness-safe LE encode/decode helpers. LAS is always little-endian
  // by spec; LASlib uses put64bitsLE / get64bitsLE helpers internally.
  // Our post-close patch fwrites multi-byte integers, so on big-endian
  // builds (rare for R packages but possible) a native fread/fwrite
  // would silently corrupt the file. These byte-by-byte helpers work on
  // any host endianness — the few-microsecond cost is irrelevant for a
  // handful of patches in close().
  inline void put_u64_le(std::uint64_t v, U8* dst)
  {
    for (int i = 0; i < 8; ++i) dst[i] = (U8)((v >> (8*i)) & 0xFFu);
  }
  inline void put_u32_le(std::uint32_t v, U8* dst)
  {
    for (int i = 0; i < 4; ++i) dst[i] = (U8)((v >> (8*i)) & 0xFFu);
  }
  inline std::uint64_t get_u64_le(const U8* src)
  {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((std::uint64_t)src[i] & 0xFFu) << (8*i);
    return v;
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
  // LASlib's add_evlr (lasdefinitions.hpp:686-722) only writes strlen bytes
  // of user_id and uses realloc (not calloc) for subsequent slots, so the
  // padding bytes after "copc" are undefined when this isn't the very
  // first eVLR add. Zero them explicitly so finalize_and_write's bounded
  // user_id_equals scan reliably matches our placeholder regardless of
  // where LASlib positions it.
  if (copc_header->number_of_extended_variable_length_records > 0)
  {
    LASevlr& last = copc_header->evlrs[copc_header->number_of_extended_variable_length_records - 1];
    std::memset(last.user_id, 0, sizeof(last.user_id));
    std::memcpy(last.user_id, "copc", 4);
  }

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

  // Routing cap, four-way (max_extra_depth opens an auto-mode tuning):
  //   - user-set:                  hard cap at max_depth, no bumping.
  //   - auto, max_depth == 0:      single-chunk fast path.
  //   - auto, max_depth > 0,
  //     max_extra_depth >= 0:      bump up to N levels past the heuristic.
  //                                max_extra_depth=0 disables bumping
  //                                entirely (LAStools-like compact mode).
  //                                max_extra_depth=1 is the surface
  //                                default (R/Python/api.h) — single-
  //                                level bumping balances LAStools-class
  //                                file size against keeping chunks
  //                                bounded by max_points_per_octant.
  //   - auto, max_depth > 0,
  //     max_extra_depth < 0:       opt-in unbounded bumping up to
  //                                HARD_DEPTH_LIMIT (finest LOD;
  //                                largest files; was the prior default
  //                                before sofi-class inputs revealed
  //                                the file-size cost).
  if (copc_depth_user_set)
    routing_max_depth = max_depth;
  else if (max_depth == 0)
    routing_max_depth = 0;
  else if (copc_max_extra_depth >= 0)
    routing_max_depth = std::min<I32>(max_depth + copc_max_extra_depth, HARD_DEPTH_LIMIT);
  else
    routing_max_depth = HARD_DEPTH_LIMIT;

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
  // explicit budgets don't track: the outer-level occupancy /
  // octant_bytes / flushed_octants maps, allocator fragmentation,
  // LASZip encoder state across the emitted chunks, the close-time
  // sort buffer for the largest emitted chunk, and read-side buffers.
  // (The per-octant cell_to_idx is now a flat vector-backed table whose
  // arrays are accounted via capacity deltas alongside bytes/cells/hashes.)
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

  // Sort-buffer cap. Independent of resident_budget: the sort buffer is a
  // close-time, single-chunk allocation, and operators may want to allow
  // more for routing without inflating finalize peak memory (or vice
  // versa). Env override only — not folded into MEMORY_BUDGET because
  // it's not a steady-state allocation and the cap is a backstop, not a
  // budget partition.
  if (std::uint64_t e = env_max_sort_memory(); e > 0) max_sort_memory = e;
  if (std::uint32_t e = env_max_entries_per_page(); e > 0) max_entries_per_page = e;
  if (int e = env_protected_lod_depth(); e >= 0) protected_lod_depth = e;
  // Auto-tune xy_lod_depth to (routing_max_depth - 1) — i.e. one level
  // above the leaf — so XY-LOD reopen-on-flush covers every intermediate
  // (non-leaf) depth regardless of how deep the tree ends up. Without
  // this, merged writes whose routing_max_depth exceeds the static
  // default+1 (= 5) leave intermediate octants flushed-and-not-reopened,
  // producing visible horizontal banding at d > xy_lod_depth (e.g. d=5
  // of a 4-tile merge with routing_max_depth=6). routing_max_depth has
  // already been resolved above (auto = max_depth + max_extra_depth, or
  // user-set value, or HARD_DEPTH_LIMIT). The single-tile case
  // (routing_max_depth=5 → xy_lod_depth=4) matches the previous static
  // default, so behavior there is unchanged. Env override still wins.
  xy_lod_depth = (std::max)(0, routing_max_depth - 1);
  if (int e = env_xy_lod_depth(); e >= -1) xy_lod_depth = e;
  if (int e = env_xy_lod_grid_multiplier(); e > 0) xy_lod_grid_multiplier = e;

  // Optional one-shot diagnostic: when LASR_COPC_DEBUG_BUDGETS is set,
  // print the resolved (resident, ram, wb, sort-cap) so tests and operators
  // can verify that LASR_COPC_MEMORY_BUDGET was actually parsed and
  // split. Cheap and avoids exposing budget internals through the
  // public R API.
  if (const char* dbg = std::getenv("LASR_COPC_DEBUG_BUDGETS"); dbg && *dbg)
  {
    warning("COPC budgets resolved: resident=%llu spill_ram=%llu spill_wb=%llu sort_cap=%llu protected_lod_depth=%d xy_lod_depth=%d xy_lod_grid_multiplier=%d (mem=%llu)\n",
            (unsigned long long)resident_budget,
            (unsigned long long)spill_ram,
            (unsigned long long)spill_wb,
            (unsigned long long)max_sort_memory,
            (int)protected_lod_depth,
            (int)xy_lod_depth,
            (int)xy_lod_grid_multiplier,
            (unsigned long long)mem_budget);
  }

  hierarchy = new COPChierarchy(*copc_header, max_depth, copc_density);
  spill = new COPCspill(output_path, copc_header->point_data_record_length,
                        spill_ram,
                        COPCspill::DEFAULT_WRITE_BUF_SIZE,
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

  // Serialize the point bytes once. write_scratch was sized to point_size
  // at open() and is reused here to avoid one heap allocation per
  // write_point() call (~360M on huge inputs). The routing loop mutates
  // these bytes in place via std::swap_ranges on hash collision; on
  // function return the buffer is logically dead until the next intake
  // point overwrites it.
  const U32 point_size = copc_header->point_data_record_length;
  effective->copy_to(write_scratch.data());
  const U64 hash = point_hash(write_scratch.data(), point_size);

  // Top-down voxel routing with hash-based eviction (caveats 1 + 2).
  return route_or_spill(write_scratch.data(), hash, /*start_depth=*/0);
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
// FlatI32U32Map implementation — open-addressed I32→U32 hash table with
// linear probing. ~12 B amortised per entry vs ~50 B for unordered_map.
U32 COPCwriter::FlatI32U32Map::mix(I32 x)
{
  U32 h = static_cast<U32>(x);
  h ^= h >> 16;
  h *= 0x85ebca6bu;
  h ^= h >> 13;
  h *= 0xc2b2ae35u;
  h ^= h >> 16;
  return h;
}

U32 COPCwriter::FlatI32U32Map::find(I32 cell) const
{
  if (keys.empty()) return UINT32_MAX;
  const U32 mask = static_cast<U32>(keys.size() - 1);
  U32 slot = mix(cell) & mask;
  for (;;)
  {
    const I32 k = keys[slot];
    if (k == cell) return values[slot];
    if (k == -1)   return UINT32_MAX;
    slot = (slot + 1u) & mask;
  }
}

void COPCwriter::FlatI32U32Map::insert(I32 cell, U32 idx)
{
  // Initial table or load > 0.7 → grow (double, or 16 if empty). The
  // load check is on (count + 1) so we never let a single insert push
  // past the threshold.
  if (keys.empty() || (count + 1) * 10 > keys.size() * 7)
  {
    const std::size_t new_cap = keys.empty() ? 16u : keys.size() * 2u;
    std::vector<I32> new_keys(new_cap, -1);
    std::vector<U32> new_values(new_cap, 0u);
    const U32 new_mask = static_cast<U32>(new_cap - 1);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
      const I32 k = keys[i];
      if (k == -1) continue;
      U32 slot = mix(k) & new_mask;
      while (new_keys[slot] != -1) slot = (slot + 1u) & new_mask;
      new_keys[slot]   = k;
      new_values[slot] = values[i];
    }
    keys.swap(new_keys);
    values.swap(new_values);
  }
  const U32 mask = static_cast<U32>(keys.size() - 1);
  U32 slot = mix(cell) & mask;
  while (keys[slot] != -1) slot = (slot + 1u) & mask;
  keys[slot]   = cell;
  values[slot] = idx;
  ++count;
}

bool COPCwriter::FlatI32U32Map::erase(I32 cell)
{
  if (keys.empty()) return false;
  const U32 mask = static_cast<U32>(keys.size() - 1);
  U32 slot = mix(cell) & mask;
  for (;;)
  {
    const I32 k = keys[slot];
    if (k == -1) return false;
    if (k == cell) break;
    slot = (slot + 1u) & mask;
  }

  keys[slot] = -1;
  values[slot] = 0u;
  --count;

  // Linear-probing deletion must reinsert the following cluster; otherwise
  // find() could stop at the new empty slot before reaching keys displaced
  // past it.
  U32 next = (slot + 1u) & mask;
  while (keys[next] != -1)
  {
    const I32 rekey = keys[next];
    const U32 reval = values[next];
    keys[next] = -1;
    values[next] = 0u;
    --count;
    insert(rekey, reval);
    next = (next + 1u) & mask;
  }

  return true;
}

// Small per-voxel slack added on top of the capacity-delta accounting.
// With the FlatI32U32Map (no per-entry node allocations) the only
// non-array per-voxel cost is the malloc bookkeeping amortised across
// the five vector allocations (bytes / cells / hashes /
// cell_to_idx.keys / cell_to_idx.values), which doesn't scale with N.
// 8 B is conservative slack for the libc++ vector header overhead and
// any rounding-induced inaccuracy not captured by capacity().
static constexpr std::size_t RESIDENT_NODE_OVERHEAD = 8;

bool COPCwriter::route_or_spill(U8* bytes, U64 hash, I32 start_depth)
{
  const I32 cap = max_points_per_octant > 0 ? max_points_per_octant : 100000;
  const std::size_t point_size = (std::size_t)copc_header->point_data_record_length;

  // Decode `bytes` into `point` exactly once per outer-loop iteration.
  // compute_key_at and compute_voxel_cell only read x/y/z from `point`,
  // and those don't change as we walk depths — re-decoding per depth
  // (the previous behaviour) just burned ~max_depth point-decodes per
  // point routing iteration. After a hash-collision swap the bytes do
  // change, so set need_decode=true on that path and re-enter the
  // outer loop, which re-decodes once at the top.
  bool inserted_resident = false;
  bool need_decode = true;
  for (;;)
  {
    if (need_decode)
    {
      point->copy_from(bytes);
      need_decode = false;
    }
    bool placed = false;
    for (I32 d = start_depth; d < routing_max_depth; ++d)
    {
      EPTkey k_d = hierarchy->compute_key_at(point, d);

      // Flushed non-XY octants no longer accept claims — descend. For
      // intermediate XY overview octants, a flush is only a byte spill:
      // keep accepting later batches until the total emitted count reaches
      // the chunk cap. That keeps point RAM bounded but avoids freezing a
      // low-depth chunk to whichever scan band arrived before the first
      // memory-pressure flush.
      const bool was_flushed = flushed_octants.count(k_d) != 0;
      U64 already_flushed = 0;
      if (was_flushed)
      {
        if (d > xy_lod_depth) continue;
        auto fit = flushed_octant_points.find(k_d);
        already_flushed = (fit != flushed_octant_points.end()) ? fit->second : 0;
        if (already_flushed >= (U64)cap) continue;
      }

      I32 cell_d = -1;
      if (d <= xy_lod_depth)
      {
        const I32 base_grid = hierarchy->get_grid_size();
        const I32 multiplier = (std::max)(1, xy_lod_grid_multiplier);
        const I32 xy_grid = (base_grid <= 8192 / multiplier) ? base_grid * multiplier : 8192;
        cell_d = hierarchy->compute_xy_cell(point, k_d, xy_grid);
      }
      else
      {
        cell_d = hierarchy->compute_voxel_cell(point, k_d);
      }
      if (cell_d < 0) continue;

      auto& oct = occupancy[k_d];
      const U32 found_idx = oct.cell_to_idx.find(cell_d);
      if (found_idx == UINT32_MAX)
      {
        // Voxel free. Claim it only if the octant still has cap room;
        // otherwise let shallow XY overview cells compete for an existing
        // slot. This avoids freezing the first cap cells encountered in
        // scan order at d=3-4: a later cell with a stronger deterministic
        // hash can evict a previously selected cell, and the evicted point
        // continues routing downward. The slot count and byte capacity stay
        // unchanged, so this improves intermediate-depth mixing without
        // increasing resident RAM.
        const U64 total_claims = already_flushed + (U64)oct.cells.size();
        if (total_claims >= (U64)cap)
        {
          if (d <= xy_lod_depth && !oct.cells.empty())
          {
            const U32 idx = (U32)(hash % (U64)oct.cells.size());
            if (hash > oct.hashes[idx])
            {
              const I32 old_cell = oct.cells[idx];
              U8* slot = oct.bytes.data() + (std::size_t)idx * point_size;
              std::swap_ranges(bytes, bytes + point_size, slot);
              std::swap(hash, oct.hashes[idx]);
              oct.cell_to_idx.erase(old_cell);
              oct.cells[idx] = cell_d;
              oct.cell_to_idx.insert(cell_d, idx);
              start_depth = d + 1;
              placed = false;
              need_decode = true;
              goto next_iter;
            }
          }
          continue;
        }
        // Snapshot capacities before insertion so we can attribute any
        // vector growth (which doubles capacity) to this voxel's account.
        // Includes the FlatI32U32Map's two arrays.
        const std::size_t cap_before =
            oct.bytes.capacity() +
            oct.cells.capacity()  * sizeof(I32) +
            oct.hashes.capacity() * sizeof(U64) +
            oct.cell_to_idx.keys.capacity()   * sizeof(I32) +
            oct.cell_to_idx.values.capacity() * sizeof(U32);

        const U32 idx = (U32)oct.cells.size();
        oct.cells.push_back(cell_d);
        oct.hashes.push_back(hash);
        oct.bytes.insert(oct.bytes.end(), bytes, bytes + point_size);
        oct.cell_to_idx.insert(cell_d, idx);

        const std::size_t cap_after =
            oct.bytes.capacity() +
            oct.cells.capacity()  * sizeof(I32) +
            oct.hashes.capacity() * sizeof(U64) +
            oct.cell_to_idx.keys.capacity()   * sizeof(I32) +
            oct.cell_to_idx.values.capacity() * sizeof(U32);
        const std::size_t added = (cap_after - cap_before) + RESIDENT_NODE_OVERHEAD;
        resident_bytes += added;
        octant_bytes[k_d] += added;
        inserted_resident = true;
        placed = true;
        break;
      }

      const U32 idx = found_idx;
      if (hash > oct.hashes[idx])
      {
        // New point wins this voxel. Swap the in-arena slot with the
        // caller-provided `bytes` buffer using std::swap_ranges — no
        // allocation per replacement, no malloc churn. The evicted
        // bytes end up in `bytes` and continue routing from depth+1.
        // Hash is a plain swap.
        U8* slot = oct.bytes.data() + (std::size_t)idx * point_size;
        std::swap_ranges(bytes, bytes + point_size, slot);
        std::swap(hash, oct.hashes[idx]);
        start_depth = d + 1;
        placed = false;
        // bytes were just swapped — `point` no longer matches them.
        // Re-decode at the top of the next outer iteration.
        need_decode = true;
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
      // `point` is already in sync with `bytes` here: the inner loop
      // exited via fall-through (no break, no goto), and the outer-loop
      // top decoded `bytes` into `point` at the start of this iteration.
      // Hash-collision swaps would have set need_decode and jumped to
      // next_iter, so this branch is reached only when bytes haven't
      // mutated since the decode.
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
      if (!spill->append(deepest, bytes))
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
  flushed_octant_points[key] += (U64)n;
  occupancy.erase(it);
  flushed_octants.insert(key);
  resident_bytes = (resident_bytes >= freed) ? (resident_bytes - freed) : 0;
  return true;
}

bool COPCwriter::enforce_resident_budget()
{
  // Pick the heaviest unprotected hot octants from the incrementally-
  // maintained octant_bytes table (O(N_octants), no inner voxel sum) and
  // flush until resident_bytes drops below the low-water mark. Without any
  // hysteresis the previous version triggered per-point on huge inputs and
  // dominated CPU (97% in this function on the sofi run before this fix).
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
  // Low_water at 40% leaves headroom for metadata, fragmentation, and
  // transient peaks during finalize.
  // Protected LODs are intentionally not flush candidates: freezing them
  // early makes later points descend past those overview octants, so
  // low-depth reads can show scan-order bands and blocky holes. Treat
  // protected bytes as the irreducible floor and apply hysteresis to the
  // remaining budget slice.
  if (octant_bytes.empty()) return true;
  std::uint64_t protected_bytes = 0;
  for (const auto& kv : octant_bytes)
    if (kv.first.d <= protected_lod_depth) protected_bytes += kv.second;

  const std::uint64_t unprotected_budget =
      (resident_budget > protected_bytes) ? (resident_budget - protected_bytes) : 0;
  const std::uint64_t low_water =
      protected_bytes + (std::uint64_t)((double)unprotected_budget * 0.4);
  if (resident_bytes <= low_water) return true;

  std::vector<std::pair<std::uint64_t, EPTkey>> sorted;
  sorted.reserve(octant_bytes.size());
  for (const auto& kv : octant_bytes)
  {
    if (kv.first.d <= protected_lod_depth) continue;
    sorted.emplace_back(kv.second, kv.first);
  }
  if (sorted.empty()) return true;
  // Heaviest first; (depth, x, y, z) tie-break preserves determinism.
  std::sort(sorted.begin(), sorted.end(),
            [&key_less](const auto& a, const auto& b) {
              if (a.first != b.first) return a.first > b.first;
              return key_less(a.second, b.second);
            });

  for (const auto& kv : sorted)
  {
    if (resident_bytes <= low_water) break;
    // Defensive: skip if the octant was already drained earlier this pass.
    if (occupancy.find(kv.second) == occupancy.end()) continue;
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

  // Phase boundary: residents are now fully flushed to spill, the
  // bytes/cells/hashes/cell_to_idx arenas inside `occupancy` are no
  // longer needed, and `octant_bytes` was emptied by flush_hot_octant.
  // Swap-clear all three so their bucket arrays are released back to
  // the allocator before we allocate the close-time sort buffer and
  // hierarchy's emit_copy. With ~50k+ active octants on huge inputs
  // this routinely frees hundreds of MB of bucket-array memory and
  // keeps peak finalize footprint small.
  decltype(occupancy)().swap(occupancy);
  decltype(octant_bytes)().swap(octant_bytes);
  // flushed_octants is no longer consulted past this point either —
  // routing has stopped. Release it too.
  decltype(flushed_octants)().swap(flushed_octants);

  // Drain every spilled cell's pending write_buf to the shared spill log
  // in one pass. Without this, the per-leaf flush_write_buf inside
  // read_octant / stream_octant runs interleaved with reads, churning
  // the fd between SEEK_END+fwrite and SEEK_SET+fread for every emit
  // chunk. Doing the drain once here also releases write_buf capacity
  // back to the allocator before the emit phase allocates its sort
  // buffer, lowering peak finalize memory by up to wb_budget bytes.
  if (!spill->flush_all_write_bufs())
  {
    fail(std::string("COPCspill drain failed: ") + spill->last_error());
    return false;
  }

  const U32 point_size = copc_header->point_data_record_length;

  // 1) Collapse the octree based on per-leaf counts.
  auto leaf_counts = spill->cell_counts();
  hierarchy->finalize(leaf_counts, min_points_per_chunk, max_points_per_octant);
  // Release the temporary leaf_counts map — hierarchy has its own
  // internal state now and we don't need this copy past finalize.
  decltype(leaf_counts)().swap(leaf_counts);

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
      // Differentiate the auto branch by where routing actually stopped:
      // hitting HARD_DEPTH_LIMIT means a true pathological cluster, but
      // stopping at the configured compact / max_extra_depth cap means
      // the user can opt back into deeper bumping with a knob change.
      // Without this distinction, compact-mode users would see a
      // "hard depth limit" message that doesn't apply to them.
      const char* mitigation;
      if (copc_depth_user_set)
      {
        mitigation = "Raise max_depth (or leave it auto for adaptive bumping) "
                     "or accept the larger chunk.";
      }
      else if (routing_max_depth == HARD_DEPTH_LIMIT)
      {
        mitigation = "A point cluster exhausted the writer's hard depth limit; "
                     "either lower the cluster density or accept the larger chunk.";
      }
      else
      {
        mitigation = "Routing stopped at the configured cap (max_extra_depth past "
                     "the auto heuristic); raise max_extra_depth (-1 = unbounded) "
                     "or accept the larger chunk.";
      }
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
    const I64 chunk_offset = writer_las->tell();

    // Sort-cap gate. The sort path buffers the whole chunk in `sort_buf`
    // plus a parallel pointer vector for stable_sort — fine for
    // <=max_points_per_octant chunks (typical 3 MB at 100k × 30 B), but
    // a force-accepted leaf at HARD_DEPTH_LIMIT (or a user-set
    // max_points_per_chunk) can produce a chunk whose buffer alone
    // exceeds available RAM. The skip-sort path streams from spill
    // point-by-point — same per-point CPU, no whole-chunk allocation.
    // The COPC spec doesn't mandate within-chunk ordering; the only
    // cost is slightly worse LAZ compression on the affected chunk.
    if (bytes > max_sort_memory)
    {
      if (!skip_sort_warned)
      {
        warning("COPC writer: chunk at depth=%d x=%d y=%d z=%d holds "
                "%llu MB of point data, exceeding the sort-buffer cap "
                "(%llu MB). Emitting in spill-append order without "
                "sorting; LAZ compression will be slightly worse for "
                "this chunk. Subsequent occurrences are silenced. "
                "Raise LASR_COPC_MAX_SORT_MEMORY to opt back into "
                "sorting at the cost of higher peak RAM.\n",
                (int)o.key.d, (int)o.key.x, (int)o.key.y, (int)o.key.z,
                (unsigned long long)(bytes >> 20),
                (unsigned long long)(max_sort_memory >> 20));
        skip_sort_warned = true;
      }
      auto cb = [this](const U8* p) -> bool {
        point->copy_from(p);
        return writer_las->write_point(point) != 0;
      };
      if (!spill->stream_octant(o.leaves, o.point_count, cb))
      {
        fail(std::string("COPCspill stream failed: ") + spill->last_error());
        return false;
      }
    }
    else
    {
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

      // Write the chunk through the sorted pointer vector — keeps close-time
      // peak memory at one octant's worth (sort_buf + ptrs), independent of
      // the post-collapse octant size.
      for (U64 i = 0; i < o.point_count; i++)
      {
        point->copy_from(ptrs[i]);
        if (!writer_las->write_point(point))
        {
          fail("LASwriterLAS::write_point failed");
          return false;
        }
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

  // 3) Install the hierarchy bytes into the COPC eVLR placeholder.
  // Build the paginated layout — for hierarchies that fit in one page
  // (typical: <4096 entries by default), this produces a single page
  // with no child references (semantically identical to the legacy flat
  // layout). For larger hierarchies, subtrees that overflow the page
  // are spawned as child pages referenced by entries with
  // point_count = -1 — cloud readers can fetch one page at a time via
  // byte-range requests instead of pulling the whole eVLR up front.
  //
  // Two-phase write: child-ref offsets are 0 placeholders here. The
  // actual eVLR data start file position is only known after
  // writer_las->close() runs writer->done() (which flushes LAZ encoder
  // state). close() patches the file-on-disk after writer_las has
  // closed — see the post-close patch block in close().
  auto pag = hierarchy->build_paginated_evlr_data(max_entries_per_page);
  const std::size_t n_entries_total = pag.data.size() / sizeof(LASvlr_copc_entry);
  // LASlib owns evlrs[i].data after assignment and delete[]'s it via
  // ~LASheader (lasdefinitions.hpp:684). new[] is mandatory per that
  // contract — pag.data is a std::vector<U8>, so copy out.
  LASvlr_copc_entry* ev_data = new LASvlr_copc_entry[n_entries_total];
  std::memcpy(ev_data, pag.data.data(), pag.data.size());
  // Stash patch state for close().
  needs_root_size_patch = (pag.root_page_size != pag.data.size());
  root_page_size_for_patch = pag.root_page_size;
  child_refs_for_patch = std::move(pag.child_refs);
  // Look the placeholder up by (user_id, record_id) instead of evlrs[0]:
  // prepare_copc_header happens to add it first today, but a future patch
  // that inserts another eVLR earlier (e.g. CRS WKT eVLR for LAS 1.4)
  // would silently corrupt the COPC hierarchy via positional access.
  // LASvlr/LASevlr::user_id is a fixed 16-byte field. LASlib's add_vlr
  // memsets the slot before write so the field is well-padded; add_evlr
  // does not, and uses realloc (not calloc) for subsequent slots, so
  // padding bytes after the user_id string may be undefined when our
  // placeholder isn't the very first add. Use a bounded compare for
  // both lookups so a preceding non-terminated record can't make the
  // scan run past the field or accidentally match.
  auto user_id_equals = [](const char* field16, const char* test) -> bool {
    const std::size_t n = std::strlen(test);
    if (n > 16) return false;
    if (std::memcmp(field16, test, n) != 0) return false;
    // For a match, every byte after `test` within the 16-byte field
    // must be NUL (so "copc\0..." matches "copc" but "copcX..." doesn't).
    for (std::size_t k = n; k < 16; ++k) if (field16[k] != '\0') return false;
    return true;
  };
  LASevlr* hier_evlr = nullptr;
  for (U32 i = 0; i < copc_header->number_of_extended_variable_length_records; i++)
  {
    if (user_id_equals(copc_header->evlrs[i].user_id, "copc") &&
        copc_header->evlrs[i].record_id == 1000)
    {
      hier_evlr = &copc_header->evlrs[i];
      break;
    }
  }
  if (!hier_evlr)
  {
    delete[] ev_data;
    fail("COPC writer: hierarchy eVLR placeholder (user_id=copc, record_id=1000) not found");
    return false;
  }
  hier_evlr->record_length_after_header = (I64)pag.data.size();
  hier_evlr->data = (U8*)ev_data;

  // 4) Fill COPC info VLR fields we know now. LASwriterLAS::update_header
  //    will compute and patch root_hier_offset / root_hier_size based on the
  //    eVLR position (see laswriter_las.cpp:1250-1383), so leave those zero.
  // Same scan pattern as the eVLR above. add_vlr does memset its slot, so
  // a strcmp-based lookup (e.g. via LASheader::get_vlr) would also work
  // today; using the bounded helper keeps both lookups consistent and
  // future-proof against LASlib changes that might drop the memset.
  LASvlr* info_vlr = nullptr;
  for (U32 i = 0; i < copc_header->number_of_variable_length_records; i++)
  {
    if (user_id_equals(copc_header->vlrs[i].user_id, "copc") &&
        copc_header->vlrs[i].record_id == 1)
    {
      info_vlr = &copc_header->vlrs[i];
      break;
    }
  }
  if (!info_vlr || !info_vlr->data)
  {
    fail("COPC writer: info VLR placeholder (user_id=copc, record_id=1) not found");
    return false;
  }
  LASvlr_copc_info* info = (LASvlr_copc_info*)info_vlr->data;
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

  // Pagination v2 post-close patch. Two things to fix on disk:
  //   (a) Child-page-reference entries' (offset, byte_size) fields,
  //       which we left as 0 placeholders at finalize time because
  //       writer_las->tell() didn't yet account for LAZ done()-flush
  //       bytes. Now that the writer has fully closed, the on-disk
  //       file's COPC info VLR has the correct root_hier_offset (=
  //       absolute file offset of the eVLR data start), patched there
  //       by LASlib's update_header (laswriter_las.cpp:1378-1385).
  //   (b) root_hier_size: LASlib unconditionally sets it to the total
  //       eVLR length, which is wrong when there are child pages —
  //       readers expect only the root page's size.
  // Both patches operate on tiny well-defined byte positions; if they
  // fail, the file is corrupted (paginated layout pointing at garbage)
  // — fail the close so the engine surfaces the error and the writer's
  // output unlink kicks in.
  if (!poisoned && finalize_ok && copc_header &&
      (needs_root_size_patch || !child_refs_for_patch.empty()))
  {
    // The COPC info VLR is the first VLR after the LAS 1.4 header, and
    // root_hier_offset / root_hier_size live at fixed offsets within
    // that VLR's data. LASlib hardcodes the same constants when it
    // patches root_hier_offset/size at write time
    // (laswriter_las.cpp:1381 → `stream->seek(375 + 54 + 40)`); we
    // hardcode the same numbers here so the two sides cannot drift.
    // (For our writer copc_header->header_size is always 375 — we
    // upgrade to LAS 1.4 in prepare_copc_header — so using the field
    // would be equivalent today, but matching LASlib's literal is the
    // load-bearing invariant.)
    const I64 root_hier_offset_pos = 375 + 54 + 40;
    const I64 root_hier_size_pos   = 375 + 54 + 48;

    FILE* fp = std::fopen(output_path.c_str(), "r+b");
    if (!fp)
    {
      finalize_ok = false;
      fail("paginated hierarchy: cannot reopen output for post-close patch");
    }
    else
    {
      auto seek64 = [&](I64 pos) -> bool {
#ifdef _WIN32
        return _fseeki64(fp, (long long)pos, SEEK_SET) == 0;
#else
        return fseeko(fp, (off_t)pos, SEEK_SET) == 0;
#endif
      };
      bool ok = true;
      U64 evlr_data_start = 0;
      // (1) Read root_hier_offset off disk to learn where the eVLR data
      // starts in the file. We don't trust our cached writer_las->tell()
      // value because of the done()-flush gap. Read into a fixed-size
      // byte buffer and decode LE explicitly so the patch is correct
      // on big-endian hosts (LAS is always little-endian by spec).
      U8 le_buf[12];
      if (ok && !seek64(root_hier_offset_pos)) { ok = false; }
      if (ok && std::fread(le_buf, 1, 8, fp) != 8) { ok = false; }
      if (ok) evlr_data_start = get_u64_le(le_buf);
      // Sanity: the eVLR data must start past the LAS 1.4 header.
      if (ok && evlr_data_start < 375u) { ok = false; }
      // (2) Patch each child page reference in place. Each ChildPageRef
      // identifies the entry's byte position within the eVLR payload;
      // its absolute file position = evlr_data_start + byte_offset_in_payload.
      // Within an entry (32 bytes), the layout is:
      //   bytes 0-15: key (depth, x, y, z) — 4*I32
      //   bytes 16-23: offset (U64)
      //   bytes 24-27: byte_size (I32)
      //   bytes 28-31: point_count (I32)
      // We patch only offset and byte_size; point_count was already
      // written as -1 by the initial eVLR write.
      for (const auto& cref : child_refs_for_patch)
      {
        if (!ok) break;
        const I64 entry_pos = (I64)evlr_data_start + (I64)cref.byte_offset_in_payload;
        const std::uint64_t abs_target_offset =
            evlr_data_start + cref.target_page_relative_offset;
        put_u64_le(abs_target_offset, le_buf);
        put_u32_le((std::uint32_t)cref.target_page_byte_size, le_buf + 8);
        if (!seek64(entry_pos + 16))                 { ok = false; break; }
        if (std::fwrite(le_buf, 1, 12, fp) != 12)    { ok = false; break; }
      }
      // (3) Patch root_hier_size if the layout is paginated.
      if (ok && needs_root_size_patch)
      {
        put_u64_le(root_page_size_for_patch, le_buf);
        if (!seek64(root_hier_size_pos))             { ok = false; }
        else if (std::fwrite(le_buf, 1, 8, fp) != 8) { ok = false; }
      }
      // fclose may surface a delayed write error from a previous fwrite —
      // treat its return code as part of the patch outcome so a flush
      // failure can't silently leave the pagination metadata corrupt.
      if (std::fclose(fp) != 0) ok = false;
      if (!ok)
      {
        finalize_ok = false;
        fail("paginated hierarchy: post-close on-disk patch failed");
      }
    }
  }

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
