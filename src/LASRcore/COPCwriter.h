#ifndef COPC_WRITER_H
#define COPC_WRITER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "COPChierarchy.h"
#include "COPCspill.h"

class LASwriterLAS;
class LASheader;
class LASpoint;

// Facade that writes a Cloud Optimized Point Cloud (COPC) LAZ file. Composes
// (not inherits) LASlib's LASwriterLAS for byte-level LAZ output. Memory
// strategy: adaptive per-cell RAM buffers spill to per-cell temp files only
// under pressure (COPCspill). At close(), the octree is finalised via
// COPChierarchy and one LAZ chunk is emitted per final octant.
//
// API is shaped to drop into LASio::create() in place of LASwriteOpener::open()
// for .copc.laz outputs.
class COPCwriter
{
public:
  COPCwriter();
  ~COPCwriter();

  COPCwriter(const COPCwriter&) = delete;
  COPCwriter& operator=(const COPCwriter&) = delete;

  // Configuration (call before open()).
  // depth >= 0 is treated as a hard cap on routing depth (the auto heuristic
  // can underestimate for clustered data, but if the user asks for a
  // specific depth, honour it). depth < 0 (the default) leaves it on auto:
  // the heuristic picks an initial value, and routing may bump beyond it
  // up to HARD_DEPTH_LIMIT to keep chunks under max_points_per_octant.
  void set_copc_depth(I32 depth) { copc_depth = depth; copc_depth_user_set = (depth >= 0); }
  void set_copc_density(I32 density) { copc_density = density; } // grid side, typically 128/256/512
  void set_min_points_per_chunk(I32 n) { min_points_per_chunk = n; }
  // Auto-mode-only cap on adaptive depth bumping. -1 = no extra cap
  // (HARD_DEPTH_LIMIT applies). 0 = compact: never bump past auto
  // heuristic. >0 = bump up to N levels past the heuristic. Ignored
  // when copc_depth is user-set (an explicit max_depth is already a
  // hard cap and bumping is disabled in that mode).
  void set_copc_max_extra_depth(I32 extra) { copc_max_extra_depth = extra; }
  // Per-chunk cap. Default 100,000 matches LASlib. Larger values
  // produce fewer, larger chunks: less hierarchy + spill metadata,
  // bigger close-time sort buffers, coarser spatial random access.
  void set_max_points_per_octant(I32 n) { if (n > 0) max_points_per_octant = n; }

  // Open the output file. Applies the COPC header transformations (upgrade to
  // LAS 1.4, promote PDRF to 6/7/8, add COPC info VLR placeholder and EPT
  // hierarchy eVLR placeholder). Returns false on failure (e.g. cannot open
  // the output, unsupported PDRF).
  bool open(const char* file_name, const LASheader* source_header, I32 io_buffer_size);

  // Write one point. Format-converts to target PDRF, hashes the bytes, and
  // routes through the top-down voxel-occupancy table (route_or_spill).
  // Returns false if writer is poisoned or I/O fails.
  bool write_point(const LASpoint* p);

  // Finalise: collapse, sort per-octant, LAZ-chunk emit, write hierarchy eVLR,
  // patch header and COPC info VLR. Returns total bytes written (or -1 on error).
  I64 close();

  // Total bytes written so far (for compatibility with LASio's bookkeeping).
  I64 tell();

  const std::string& last_error() const { return error_msg; }
  bool is_poisoned() const { return poisoned; }

private:
  // Copy & transform the caller's header to the target COPC layout (PDRF 6+,
  // LAS 1.4, COPC VLR placeholders). Sets this->copc_header and this->point.
  bool prepare_copc_header(const LASheader* source_header);

  // Drive the finalization sequence (called from close()).
  bool finalize_and_write();

  void fail(const std::string& msg);

private:
  // Transformed header handed to LASwriterLAS. Owned by this writer.
  LASheader* copc_header = nullptr;
  // Point object used for per-call format conversion to the target PDRF.
  LASpoint* point = nullptr;
  // LAZ writer we wrap.
  LASwriterLAS* writer_las = nullptr;

  // Reusable per-call scratch (sized once at open() to target PDRF length).
  // Avoids a heap allocation on every write_point().
  std::vector<U8> write_scratch;

  COPChierarchy* hierarchy = nullptr;
  COPCspill* spill = nullptr;

  // Packed per-octant arena. Replaces the old
  // unordered_map<I32, ResidentEntry> per octant where each voxel paid
  // ~140 B (50 B map node + 32 B ResidentEntry + 48 B vector<U8> heap +
  // 16 B malloc bookkeeping for a 30 B point — ~4.5× payload overhead).
  //
  // Per voxel here:
  //   - point_size bytes of payload, packed contiguously in `bytes`
  //   - 4 B cell id in `cells`
  //   - 8 B hash in `hashes`
  //   - ~12 B amortised in `cell_to_idx` (flat open-addressed table at
  //     ~0.7 load factor: 8 B per slot × 1/0.7 ≈ 11.4 B per voxel)
  // ≈ point_size + 24 B total. ~80% smaller per voxel than the original
  // unordered_map<I32, ResidentEntry> design and ~50% smaller than the
  // first packed cut (which still used unordered_map<I32, U32> for the
  // lookup at ~50 B per voxel).
  //
  // Replacement (hash collision) is in-place: copy new bytes into
  // bytes[idx*point_size], swap hashes[idx], evicted bytes are returned
  // via the caller-passed buffer.
  //
  // FlatI32U32Map is an open-addressed I32→U32 hash table specialised
  // for our usage: insert-or-update only (no per-entry erase — the
  // whole octant is dropped at flush time), so we don't need
  // tombstones; the cell-id key is non-negative so we sentinel empty
  // slots with -1; capacity is a power of 2 so slot index is a cheap
  // bitmask. ~12 B amortised per entry vs ~50 B for unordered_map.
  struct FlatI32U32Map
  {
    std::vector<I32> keys;     // -1 = empty (cell ids are always >= 0)
    std::vector<U32> values;   // index into the parallel arrays
    std::size_t      count = 0;

    // Murmur3 finalizer — well-distributed for sequential cell ids and
    // cheap (a few mixes). Quality matters under linear probing.
    static U32 mix(I32 x);

    // Find the index for `cell`, or return UINT32_MAX if absent.
    U32 find(I32 cell) const;

    // Insert (cell -> idx) assuming cell is not already present. Grows
    // the table when load factor would exceed ~0.7.
    void insert(I32 cell, U32 idx);
  };

  struct HotOctant
  {
    std::vector<U8>  bytes;        // size = N * point_size
    std::vector<I32> cells;        // size = N
    std::vector<U64> hashes;       // size = N
    FlatI32U32Map    cell_to_idx;  // cell -> index in parallel arrays
  };

  // Per-octant voxel occupancy. A point claims the first ancestor (root-down)
  // whose voxel cell at this octant is either unoccupied (and the octant is
  // under cap) or occupied by a resident with a smaller hash. Evicted
  // residents are re-routed starting at depth d+1. Points that exhaust every
  // voxel up to routing_max_depth are force-accepted via spill at the
  // deepest key — for user-set max_depth this is the expected terminal
  // case; for auto mode it only happens at HARD_DEPTH_LIMIT and warns.
  std::unordered_map<EPTkey, HotOctant, EPTKeyHasher> occupancy;

  // Octants whose residents have already been flushed to spill due to RAM
  // pressure. A flushed octant no longer accepts new claims at routing
  // time — points descend past it. The flushed residents are baked into
  // spill exactly as if they had been emitted from the in-RAM table
  // normally; they just lose their replaceability (the hash-replace
  // mechanism is disabled for that octant after flush).
  std::unordered_set<EPTkey, EPTKeyHasher> flushed_octants;

  // Per-octant byte usage, kept in sync with `occupancy` on every insert /
  // replace / evict / flush. Avoids the O(voxels) cost of re-summing each
  // octant's bytes on every enforce_resident_budget call (which on a 364M
  // point input under tight budget triggers per-point and dominates CPU).
  // The aggregate `resident_bytes` is the sum over all entries.
  std::unordered_map<EPTkey, std::uint64_t, EPTKeyHasher> octant_bytes;

  // Aggregate bytes held in `occupancy`. Tracked incrementally on
  // insert / replace / evict so we can keep total resident memory below a
  // configurable budget (resident_budget). When the budget is exceeded the
  // heaviest hot octant is flushed to spill and demoted to flushed_octants.
  std::uint64_t resident_bytes = 0;
  // Default 256 MB to match COPCspill's default budget. The two budgets are
  // independent — spill bytes don't count against this one and vice versa —
  // so total close-time RAM peaks at roughly 2× this number plus the
  // largest single chunk's sort buffer.
  std::uint64_t resident_budget = 256ULL * 1024 * 1024;

  // Route a point's bytes (with precomputed hash) into the in-RAM occupancy
  // tables starting at start_depth, evicting and re-routing any residents
  // that lose the hash compare. Falls through to spill->append at the
  // routing depth cap (user max_depth, or HARD_DEPTH_LIMIT in auto mode)
  // if no ancestor accepts. Returns false on spill error.
  bool route_or_spill(std::vector<U8>&& bytes, U64 hash, I32 start_depth);

  // Push a single hot octant's residents into spill and demote it to
  // flushed_octants. Used both by enforce_resident_budget() (intake-time
  // memory pressure) and by finalize_and_write() (close-time fold).
  // Returns false on spill error.
  bool flush_hot_octant(const EPTkey& key);

  // If resident_bytes > resident_budget, flush the heaviest hot octant(s)
  // until under budget. Called from route_or_spill() after each successful
  // resident insert.
  bool enforce_resident_budget();

  // Configuration
  I32 copc_depth = -1;
  I32 copc_max_extra_depth = -1; // see set_copc_max_extra_depth
  I32 copc_density = 256;
  I32 max_points_per_octant = 100000; // used to estimate auto max_depth
  I32 min_points_per_chunk = 100;
  // Hard upper bound on adaptive depth bumping. Only consulted when
  // copc_depth was left at its auto sentinel (-1) at open() time — a user
  // who explicitly passed max_depth gets that value as a hard routing cap
  // (the auto heuristic can be wrong for clustered data; an explicit
  // request is treated as authoritative). 16 gives sub-mm voxel resolution
  // for any realistic dataset; at the hard limit the writer warns once
  // and force-accepts via spill (chunk may exceed cap; surfaced by the
  // end-of-finalize oversize warning).
  static constexpr I32 HARD_DEPTH_LIMIT = 16;
  // The actual depth the routing loop is allowed to descend to. Set in
  // prepare_copc_header from copc_depth (user) or HARD_DEPTH_LIMIT (auto).
  // Routing visits voxel-occupancy at depths 0..routing_max_depth-1 and
  // force-accepts at routing_max_depth (the "leaf").
  I32 routing_max_depth = HARD_DEPTH_LIMIT;
  // True iff the user passed an explicit max_depth via set_copc_depth.
  // Drives both the routing cap above and the wording of the
  // end-of-finalize oversize warning.
  bool copc_depth_user_set = false;
  bool hard_depth_warned = false;

  // Stats accumulated during write_point
  F64 gpstime_minimum = 0.0;
  F64 gpstime_maximum = 0.0;
  // Actual data bbox seen during intake. Compared against the declared
  // octree bbox at close() to warn when the input header is much looser
  // than the data (skewed octree structure — see "loose bbox" pitfall).
  F64 data_min_x = 0.0, data_max_x = 0.0;
  F64 data_min_y = 0.0, data_max_y = 0.0;
  F64 data_min_z = 0.0, data_max_z = 0.0;
  bool have_any_point = false;

  std::string output_path;
  bool poisoned = false;
  std::string error_msg;
  bool closed = false;
};

#endif
