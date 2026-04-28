#include "COPChierarchy.h"

#include <algorithm>
#include <cstring>
#include <unordered_set>

COPChierarchy::COPChierarchy(const LASheader& copc_header, I32 max_depth, I32 grid_size)
  : octree(copc_header), max_depth(max_depth)
{
  octree.set_gridsize(grid_size);
}

EPTkey COPChierarchy::compute_leaf_key(const LASpoint* p) const
{
  return octree.get_key(p, max_depth);
}

EPTkey COPChierarchy::compute_key_at(const LASpoint* p, I32 depth) const
{
  return octree.get_key(p, depth);
}

I32 COPChierarchy::compute_voxel_cell(const LASpoint* p, const EPTkey& key) const
{
  return octree.get_cell(p, key);
}

// Find the nearest existing ancestor of `key` in `octant_counts`. Returns
// EPTkey::root() if no proper ancestor exists (root is always considered
// to exist for routing purposes; the caller emits a zero-count root if
// none of the input keys is root).
static EPTkey find_existing_ancestor(const EPTkey& key,
                                     const std::unordered_map<EPTkey, U64, EPTKeyHasher>& octant_counts)
{
  EPTkey k = key;
  while (k.d > 0)
  {
    k = k.get_parent();
    if (octant_counts.count(k)) return k;
  }
  return EPTkey::root();
}

void COPChierarchy::finalize(const std::unordered_map<EPTkey, U64, EPTKeyHasher>& leaf_counts,
                             I32 min_points_per_chunk)
{
  emit.clear();
  entries.clear();

  // With top-down voxel routing in COPCwriter::write_point, each octant key
  // already holds exactly the points belonging to its level (one
  // representative per voxel cell of grid_size³). Our job here is just to:
  //   1) collapse octants whose count is below min_points_per_chunk by
  //      moving their points up to the nearest existing ancestor (matches
  //      LASlib's behaviour and avoids tiny LAZ chunks),
  //   2) emit zero-count placeholders for any missing ancestor of a real
  //      octant so readers can traverse the hierarchy,
  //   3) sort deterministically for stable byte output.
  std::unordered_map<EPTkey, U64, EPTKeyHasher> octant_counts = leaf_counts;
  std::unordered_map<EPTkey, std::vector<EPTkey>, EPTKeyHasher> octant_sources;
  octant_sources.reserve(octant_counts.size());
  for (const auto& kv : octant_counts) octant_sources[kv.first] = {kv.first};

  // Empty input: emit zero-count root so the file stays valid.
  if (octant_counts.empty())
  {
    FinalOctant root;
    root.key = EPTkey::root();
    root.point_count = 0;
    emit.push_back(std::move(root));
    return;
  }

  // Collapse small chunks: process deepest-first so absorption cascades
  // upward correctly (a small leaf merges into its parent; if the parent
  // is also small, the next pass merges it into the grandparent, etc.).
  const U64 min_chunk = static_cast<U64>(std::max(1, min_points_per_chunk));
  std::vector<EPTkey> ordered_keys;
  ordered_keys.reserve(octant_counts.size());
  for (const auto& kv : octant_counts) ordered_keys.push_back(kv.first);
  std::sort(ordered_keys.begin(), ordered_keys.end(),
            [](const EPTkey& a, const EPTkey& b) { return a.d > b.d; });

  for (const EPTkey& k : ordered_keys)
  {
    auto it = octant_counts.find(k);
    if (it == octant_counts.end()) continue;
    if (it->second >= min_chunk) continue;
    if (k.d == 0) continue; // root: keep as-is even if small

    EPTkey ancestor = find_existing_ancestor(k, octant_counts);
    auto anc_it = octant_counts.find(ancestor);
    if (anc_it == octant_counts.end())
    {
      // No existing ancestor (only root above and root has no entry yet):
      // create a root entry so we can absorb into it.
      anc_it = octant_counts.emplace(ancestor, 0ULL).first;
      octant_sources[ancestor] = {};
    }
    anc_it->second += it->second;
    auto& dst = octant_sources[ancestor];
    auto& src = octant_sources[k];
    dst.insert(dst.end(), std::make_move_iterator(src.begin()),
                          std::make_move_iterator(src.end()));
    octant_counts.erase(it);
    octant_sources.erase(k);
  }

  // Build the emit list from surviving octants.
  std::vector<FinalOctant> collected;
  collected.reserve(octant_counts.size());
  for (const auto& kv : octant_counts)
  {
    FinalOctant o;
    o.key = kv.first;
    o.point_count = kv.second;
    o.leaves = std::move(octant_sources[kv.first]);
    collected.push_back(std::move(o));
  }

  // Add zero-count placeholders for missing ancestors. Snapshot the real
  // count before walking — appending to `collected` while range-iterating
  // it would invalidate iterators on reallocation.
  std::unordered_set<EPTkey, EPTKeyHasher> emitted_keys;
  emitted_keys.reserve(collected.size());
  for (const auto& o : collected) emitted_keys.insert(o.key);

  std::vector<FinalOctant> placeholders;
  const size_t real_count = collected.size();
  for (size_t i = 0; i < real_count; ++i)
  {
    EPTkey k = collected[i].key;
    while (k.d > 0)
    {
      EPTkey parent = k.get_parent();
      if (emitted_keys.insert(parent).second)
      {
        FinalOctant z;
        z.key = parent;
        z.point_count = 0;
        placeholders.push_back(std::move(z));
      }
      k = parent;
    }
  }
  collected.insert(collected.end(),
                   std::make_move_iterator(placeholders.begin()),
                   std::make_move_iterator(placeholders.end()));

  // Sort deterministically for emit (depth asc, then x/y/z asc).
  std::sort(collected.begin(), collected.end(),
            [](const FinalOctant& a, const FinalOctant& b) {
              if (a.key.d != b.key.d) return a.key.d < b.key.d;
              if (a.key.x != b.key.x) return a.key.x < b.key.x;
              if (a.key.y != b.key.y) return a.key.y < b.key.y;
              return a.key.z < b.key.z;
            });

  emit = std::move(collected);
}

void COPChierarchy::record_chunk(const EPTkey& key, U64 offset, I32 byte_size, I32 point_count)
{
  LASvlr_copc_entry e;
  e.key.depth = key.d;
  e.key.x = key.x;
  e.key.y = key.y;
  e.key.z = key.z;
  e.offset = offset;
  e.byte_size = byte_size;
  e.point_count = point_count;
  entries.push_back(e);
}

void COPChierarchy::fill_copc_info(LASvlr_copc_info* info,
                                   F64 gpstime_minimum,
                                   F64 gpstime_maximum,
                                   U64 root_hier_offset,
                                   U64 root_hier_size) const
{
  std::memset(info, 0, sizeof(LASvlr_copc_info));
  info->center_x = octree.get_center_x();
  info->center_y = octree.get_center_y();
  info->center_z = octree.get_center_z();
  info->halfsize = octree.get_halfsize();
  info->spacing = get_spacing();
  info->gpstime_minimum = gpstime_minimum;
  info->gpstime_maximum = gpstime_maximum;
  info->root_hier_offset = root_hier_offset;
  info->root_hier_size = root_hier_size;
}
