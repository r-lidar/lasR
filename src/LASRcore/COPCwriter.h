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

  // Per-voxel resident point: hash drives unbiased replacement on collision,
  // bytes are held in RAM until close() flushes them to spill keyed by their
  // octant. Holding bytes (rather than a spill offset) is what lets a losing
  // resident be evicted and re-routed deeper without leaving an orphan in
  // spill. Memory cost is bounded by max_points_per_octant per active octant
  // (caveat 1's cap), so total resident bytes are bounded by the product of
  // active-octant count and the cap.
  struct ResidentEntry
  {
    U64 hash;
    std::vector<U8> bytes;
  };

  // Per-octant voxel occupancy. A point claims the first ancestor (root-down)
  // whose voxel cell at this octant is either unoccupied (and the octant is
  // under cap) or occupied by a resident with a smaller hash. Evicted
  // residents are re-routed starting at depth d+1. Points that exhaust every
  // voxel up to routing_max_depth are force-accepted via spill at the
  // deepest key — for user-set max_depth this is the expected terminal
  // case; for auto mode it only happens at HARD_DEPTH_LIMIT and warns.
  std::unordered_map<EPTkey, std::unordered_map<I32, ResidentEntry>, EPTKeyHasher> occupancy;

  // Octants whose residents have already been flushed to spill due to RAM
  // pressure. A flushed octant no longer accepts new claims at routing
  // time — points descend past it. The flushed residents are baked into
  // spill exactly as if they had been emitted from the in-RAM table
  // normally; they just lose their replaceability (the hash-replace
  // mechanism is disabled for that octant after flush).
  std::unordered_set<EPTkey, EPTKeyHasher> flushed_octants;

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
  // max-depth leaf if no ancestor accepts. Returns false on spill error.
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
