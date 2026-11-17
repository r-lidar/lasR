#ifndef COPC_HIERARCHY_H
#define COPC_HIERARCHY_H

#include <unordered_map>
#include <vector>

#include "lascopc.hpp"
#include "lasdefinitions.hpp"

// Owns the octree math for a COPC output: octant-key + voxel-cell
// computation at any depth (the writer routes per-point top-down across
// depths), the min-points-per-chunk collapse rule, the ordered list of
// final octants ready for emit, and the chunk-offset table used to build
// the EPT hierarchy eVLR. Does no I/O; operates on counts and keys
// supplied by the caller (typically COPCspill and COPCwriter).
class COPChierarchy
{
public:
  struct FinalOctant
  {
    EPTkey key;
    std::vector<EPTkey> leaves; // source leaf keys that contribute points to this octant (1 if survived, N if absorbed children)
    U64 point_count;
  };

  // copc_header is the COPC-transformed header (LAS 1.4, PDRF 6/7/8). Bounding
  // box and scale/offset determine the octree. max_depth is the auto-heuristic
  // value (LASlib parity, capped at 10) when copc_depth is auto, or the
  // user-provided value (capped at HARD_DEPTH_LIMIT in COPCwriter). grid_size
  // is the root grid density (copc_density: 128/256/512).
  COPChierarchy(const LASheader& copc_header, I32 max_depth, I32 grid_size);

  // Compute the leaf key at the constructor's max_depth for a point. Used
  // by callers that still need the heuristic leaf depth; the writer's
  // routing path uses compute_key_at(p, d) directly to support adaptive
  // depth bumping past max_depth.
  EPTkey compute_leaf_key(const LASpoint* p) const;

  // Compute the octant key at an arbitrary depth for a point. The writer
  // calls this for every depth its routing loop visits, which can extend
  // past the constructor's max_depth in auto mode (up to HARD_DEPTH_LIMIT
  // in COPCwriter). EPToctree::get_key handles any depth that fits in I32
  // grid coordinates (~depth 30 before overflow); the writer's hard cap
  // is 16, well within that.
  EPTkey compute_key_at(const LASpoint* p, I32 depth) const;

  // Voxel cell index within `key`'s grid_size³ grid for the point. Returns
  // a non-negative cell id usable as a key into a per-octant occupancy set.
  I32 compute_voxel_cell(const LASpoint* p, const EPTkey& key) const;

  // XY-only cell index within `key`'s xy_grid_size² grid. Used by the
  // streaming COPC writer for low-depth overview chunks where map-view
  // visual balance matters more than vertical occupancy. Returns a
  // non-negative cell id usable in the same occupancy table as voxel cells.
  I32 compute_xy_cell(const LASpoint* p, const EPTkey& key, I32 xy_grid_size) const;

  I32 get_max_depth() const { return max_depth; }
  I32 get_grid_size() const { return octree.get_gridsize(); }

  // Derived octree geometry for the COPC info VLR.
  F64 get_center_x() const { return octree.get_center_x(); }
  F64 get_center_y() const { return octree.get_center_y(); }
  F64 get_center_z() const { return octree.get_center_z(); }
  F64 get_halfsize() const { return octree.get_halfsize(); }
  F64 get_spacing() const { return (octree.get_halfsize() * 2.0) / octree.get_gridsize(); }

  // Apply the collapse rule to a {leaf_key -> point_count} map. After this
  // returns, emit_order() is populated with final octants in deterministic
  // order (depth-first, depth_order within each level).
  //
  // min_points_per_chunk: if a chunk has fewer than this, its points are
  // absorbed into the nearest existing ancestor — but only if that
  // absorption keeps the ancestor's total at or below max_points_per_chunk
  // (otherwise the small chunk is left alone, preserving the cap invariant).
  // max_points_per_chunk <= 0 disables the cap check (legacy behaviour).
  void finalize(const std::unordered_map<EPTkey, U64, EPTKeyHasher>& leaf_counts,
                I32 min_points_per_chunk,
                I32 max_points_per_chunk = 0);

  const std::vector<FinalOctant>& emit_order() const { return emit; }

  // Called by COPCwriter after each octant's LAZ chunk has been written.
  void record_chunk(const EPTkey& key, U64 offset, I32 byte_size, I32 point_count);

  // After all chunks recorded, returns the hierarchy entries ready to copy
  // into the eVLR payload. Includes zero-count placeholder entries for
  // absorbed non-leaf octants so readers can traverse correctly.
  const std::vector<LASvlr_copc_entry>& build_evlr_entries() const { return entries; }

  // Paginated layout of the COPC hierarchy eVLR. Subtrees are absorbed
  // into the current page only while accumulated page entries +
  // subtree size stay <= `max_entries_per_page`; otherwise the subtree
  // spawns its own child page, referenced from the parent via an entry
  // with `point_count = -1`. Cloud-hosted readers can fetch one page
  // at a time via byte-range requests instead of pulling the whole
  // eVLR up front. For small hierarchies (entries fit in one page),
  // this produces a single root page with no child references —
  // semantically identical to a non-paginated emit modulo entry walk
  // order (root-down DFS vs the prior depth-asc sort).
  //
  // Worst-case page size: `max_entries_per_page + 8`. An octree node
  // has at most 8 children; if the page is at capacity and every child
  // has to spawn its own page, the parent page ends up holding 1 entry
  // (the parent itself) + up to 8 child-refs even when that exceeds
  // `max_entries_per_page`. Bounded; the COPC reader doesn't care
  // about page size.
  //
  // **Two-phase write design.** The data buffer is populated with
  // *relative* offsets (zeros for child-page-reference entries' offset
  // and byte_size fields). The caller writes this payload via LASlib;
  // after writer_las->close() flushes everything, the caller knows the
  // absolute file offset of the eVLR data start (= COPC info VLR's
  // root_hier_offset, which LASlib has patched). It then walks
  // `child_refs` and writes the correct (offset, byte_size) for each
  // child reference in-place on disk. This avoids the v1 bug where we
  // tried to predict the eVLR data start at finalize_and_write time
  // and ended up off by the LAZ done()-flush byte count, producing
  // bad offsets that made the reader recurse on garbage memory.
  struct ChildPageRef
  {
    U64 byte_offset_in_payload;     // where this entry sits in `data`
    U64 target_page_relative_offset;// child page's byte offset within `data`
    U32 target_page_byte_size;      // child page total bytes
  };
  struct PaginatedHierarchy
  {
    std::vector<U8> data;     // serialized eVLR payload (child-ref offsets are 0 placeholders)
    U64 root_page_size = 0;   // bytes of the root page (subset of `data`); drives
                              // COPC info VLR's root_hier_size, which LASlib otherwise
                              // sets to data.size() (only correct for single-page case).
    std::vector<ChildPageRef> child_refs;  // empty => single page; no patching needed.
  };
  // Build the paginated layout. Defaults to 4096 entries per page (≈ 128 KB
  // pages at 32 B/entry — what untwine uses). max_entries_per_page = 0 falls
  // back to the default. Internal safety: caps the number of pages at
  // `max_pages_safety` (default `entries.size() + 64`); abort with empty
  // result if exceeded — guards against algorithmic bugs that would otherwise
  // OOM the process.
  PaginatedHierarchy build_paginated_evlr_data(U32 max_entries_per_page = 4096) const;

  // Fill the COPC info VLR placeholder. root_hier_offset/size are supplied
  // by the caller after the eVLR is written; everything else comes from
  // the hierarchy itself and the per-point gpstime tracking.
  void fill_copc_info(LASvlr_copc_info* info,
                      F64 gpstime_minimum,
                      F64 gpstime_maximum,
                      U64 root_hier_offset,
                      U64 root_hier_size) const;

private:
  EPToctree octree;
  I32 max_depth;
  std::vector<FinalOctant> emit;
  std::vector<LASvlr_copc_entry> entries;
};

#endif
