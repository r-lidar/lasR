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

// Recursive top-down subdivision. Group points by which child subtree they fall
// under; for each child, either descend (if subtree has enough points) or absorb
// into the current key. Returns FinalOctants found under `key`. A returned list
// may include the current key itself (if it absorbed any children) plus deeper
// surviving octants.
//
// Note: `leaves_here` is taken by value so recursive frames can mutate without
// disturbing the caller's state.
static void subdivide_recursive(const EPTkey& key,
                                std::unordered_map<EPTkey, U64, EPTKeyHasher> leaves_here,
                                I32 max_depth,
                                U64 min_chunk,
                                std::vector<COPChierarchy::FinalOctant>& out,
                                std::unordered_set<EPTkey, EPTKeyHasher>& ancestors_traversed)
{
  if (leaves_here.empty()) return;

  // Terminal: we're at max_depth — this IS a leaf, emit as single chunk.
  if (key.d >= max_depth)
  {
    U64 total = 0;
    std::vector<EPTkey> leaves_vec;
    leaves_vec.reserve(leaves_here.size());
    for (const auto& kv : leaves_here)
    {
      total += kv.second;
      leaves_vec.push_back(kv.first);
    }
    COPChierarchy::FinalOctant o;
    o.key = key;
    o.leaves = std::move(leaves_vec);
    o.point_count = total;
    out.push_back(std::move(o));
    return;
  }

  // Group leaves by which of the 8 children contains them.
  std::array<std::unordered_map<EPTkey, U64, EPTKeyHasher>, 8> groups;
  std::array<EPTkey, 8> child_keys = key.get_children();

  auto child_index = [&](const EPTkey& leaf) -> int {
    // Find which child subtree `leaf` falls under. Compute the ancestor of
    // `leaf` at depth key.d+1 and match against child_keys.
    I32 delta = leaf.d - (key.d + 1);
    I32 cx = leaf.x >> delta;
    I32 cy = leaf.y >> delta;
    I32 cz = leaf.z >> delta;
    for (int i = 0; i < 8; ++i)
    {
      if (child_keys[i].x == cx && child_keys[i].y == cy && child_keys[i].z == cz)
        return i;
    }
    return -1; // unreachable for well-formed inputs
  };

  for (auto& kv : leaves_here)
  {
    int ci = child_index(kv.first);
    if (ci >= 0) groups[ci].emplace(kv.first, kv.second);
  }

  // For each child: decide whether to recurse or absorb into `key`.
  std::unordered_map<EPTkey, U64, EPTKeyHasher> absorbed_at_this_level;
  std::vector<COPChierarchy::FinalOctant> child_results;
  for (int i = 0; i < 8; ++i)
  {
    if (groups[i].empty()) continue;

    U64 subtree_total = 0;
    for (const auto& kv : groups[i]) subtree_total += kv.second;

    if (subtree_total < min_chunk)
    {
      // Absorb this entire subtree into the current key
      for (auto& kv : groups[i]) absorbed_at_this_level.emplace(kv.first, kv.second);
    }
    else
    {
      // Subtree has enough points — recurse
      subdivide_recursive(child_keys[i], std::move(groups[i]), max_depth, min_chunk,
                          child_results, ancestors_traversed);
    }
  }

  // Emit current key if it absorbed anything, or if it's root (root always
  // emits even if empty — otherwise readers have no entry point).
  bool has_absorbed = !absorbed_at_this_level.empty();
  bool is_root = (key.d == 0);
  if (has_absorbed)
  {
    U64 total = 0;
    std::vector<EPTkey> leaves_vec;
    leaves_vec.reserve(absorbed_at_this_level.size());
    for (auto& kv : absorbed_at_this_level)
    {
      total += kv.second;
      leaves_vec.push_back(kv.first);
    }
    COPChierarchy::FinalOctant o;
    o.key = key;
    o.leaves = std::move(leaves_vec);
    o.point_count = total;
    out.push_back(std::move(o));
  }
  else if (is_root && child_results.empty())
  {
    // Degenerate case: no points at all. Emit empty root so the file is still valid.
    COPChierarchy::FinalOctant o;
    o.key = key;
    o.point_count = 0;
    out.push_back(std::move(o));
  }
  else if (!has_absorbed)
  {
    // Current key has children-that-recursed but absorbed nothing itself.
    // Record it as a zero-count ancestor so readers can traverse.
    ancestors_traversed.insert(key);
  }

  // Append child subtree results
  for (auto& r : child_results) out.push_back(std::move(r));
}

void COPChierarchy::finalize(const std::unordered_map<EPTkey, U64, EPTKeyHasher>& leaf_counts,
                             I32 min_points_per_chunk)
{
  emit.clear();
  entries.clear();

  std::unordered_set<EPTkey, EPTKeyHasher> ancestors;
  std::vector<FinalOctant> collected;

  // Empty input: subdivide_recursive returns immediately on empty leaves,
  // so the in-recursion "emit empty root" fallback is unreachable. Emit
  // it explicitly here so the file is still valid (one zero-count root
  // entry) — readers expect at least the root entry in the hierarchy.
  if (leaf_counts.empty())
  {
    FinalOctant root;
    root.key = EPTkey::root();
    root.point_count = 0;
    collected.push_back(std::move(root));
  }
  else
  {
    // Kick off recursion from root. Copy leaf_counts because the recursion takes by value.
    subdivide_recursive(EPTkey::root(), leaf_counts, max_depth,
                        static_cast<U64>(std::max(1, min_points_per_chunk)),
                        collected, ancestors);
  }

  // Add zero-count placeholders for ancestors not already emitted. Readers
  // need every ancestor of every real octant to be present in the hierarchy.
  // Snapshot the size before the loop and accumulate placeholders into a
  // separate vector — appending to `collected` while range-iterating it
  // would be UB the moment a vector reallocation invalidates the iterator
  // mid-walk.
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
  for (const EPTkey& k : ancestors)
  {
    if (emitted_keys.insert(k).second)
    {
      FinalOctant z;
      z.key = k;
      z.point_count = 0;
      placeholders.push_back(std::move(z));
    }
  }
  collected.insert(collected.end(),
                   std::make_move_iterator(placeholders.begin()),
                   std::make_move_iterator(placeholders.end()));

  // Sort deterministically for emit: depth_order from lascopc.hpp gives a
  // stable total order (depth asc, then x, y, z). Nonzero-count octants are
  // emitted first so placeholders are stable regardless of their eventual
  // position; COPCwriter writes chunks only for point_count > 0.
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
