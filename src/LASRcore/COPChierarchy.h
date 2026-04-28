#ifndef COPC_HIERARCHY_H
#define COPC_HIERARCHY_H

#include <unordered_map>
#include <vector>

#include "lascopc.hpp"
#include "lasdefinitions.hpp"

// Owns the octree math for a COPC output: max-depth leaf key assignment,
// the min-points-per-chunk collapse rule, the ordered list of final octants
// ready for emit, and the chunk-offset table used to build the EPT
// hierarchy eVLR. Does no I/O; operates on counts and keys supplied by
// the caller (typically COPCspill and COPCwriter).
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
  // box and scale/offset determine the octree. max_depth <= 10. grid_size is
  // the root grid density (copc_density: 128/256/512).
  COPChierarchy(const LASheader& copc_header, I32 max_depth, I32 grid_size);

  // Compute the max-depth leaf key for a point. Called per-point during intake.
  EPTkey compute_leaf_key(const LASpoint* p) const;

  // Compute the octant key at an arbitrary depth (0..max_depth) for a point.
  // Used by the top-down voxel-routing intake path.
  EPTkey compute_key_at(const LASpoint* p, I32 depth) const;

  // Voxel cell index within `key`'s grid_size³ grid for the point. Returns
  // a non-negative cell id usable as a key into a per-octant occupancy set.
  I32 compute_voxel_cell(const LASpoint* p, const EPTkey& key) const;

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
  // min_points_per_chunk: if a leaf has fewer than this, its points are
  // absorbed into the nearest existing ancestor. Matches the current
  // LASwriterCOPC behaviour.
  void finalize(const std::unordered_map<EPTkey, U64, EPTKeyHasher>& leaf_counts,
                I32 min_points_per_chunk);

  const std::vector<FinalOctant>& emit_order() const { return emit; }

  // Called by COPCwriter after each octant's LAZ chunk has been written.
  void record_chunk(const EPTkey& key, U64 offset, I32 byte_size, I32 point_count);

  // After all chunks recorded, returns the hierarchy entries ready to copy
  // into the eVLR payload. Includes zero-count placeholder entries for
  // absorbed non-leaf octants so readers can traverse correctly.
  const std::vector<LASvlr_copc_entry>& build_evlr_entries() const { return entries; }

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
