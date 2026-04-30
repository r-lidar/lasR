#include "COPChierarchy.h"

#include "print.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_map>
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
                             I32 min_points_per_chunk,
                             I32 max_points_per_chunk)
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
  // Tie-break on x/y/z within a depth so that when multiple equal-depth
  // chunks compete for the same ancestor's remaining cap room, the
  // outcome (which one is absorbed vs left standing) is deterministic
  // across runs — without the tie-break, unordered_map iteration order
  // would decide and that's implementation-defined.
  const U64 min_chunk = static_cast<U64>(std::max(1, min_points_per_chunk));
  std::vector<EPTkey> ordered_keys;
  ordered_keys.reserve(octant_counts.size());
  for (const auto& kv : octant_counts) ordered_keys.push_back(kv.first);
  std::sort(ordered_keys.begin(), ordered_keys.end(),
            [](const EPTkey& a, const EPTkey& b) {
              if (a.d != b.d) return a.d > b.d;
              if (a.x != b.x) return a.x < b.x;
              if (a.y != b.y) return a.y < b.y;
              return a.z < b.z;
            });

  const U64 max_chunk = (max_points_per_chunk > 0)
                          ? static_cast<U64>(max_points_per_chunk)
                          : std::numeric_limits<U64>::max();
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
    // Cap-aware: skip absorption if it would push the ancestor over the
    // configured max_points_per_chunk. Leaves the small chunk standing
    // (a slightly suboptimal LAZ chunk size, but bounded), preserves the
    // cap invariant, and keeps the small octant emittable as its own
    // chunk. If the freshly-inserted ancestor entry is unused, drop it
    // so we don't end up with a phantom zero-count root.
    if (anc_it->second + it->second > max_chunk)
    {
      if (anc_it->second == 0 && octant_sources[ancestor].empty())
      {
        octant_counts.erase(anc_it);
        octant_sources.erase(ancestor);
      }
      continue;
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

COPChierarchy::PaginatedHierarchy
COPChierarchy::build_paginated_evlr_data(U32 max_entries_per_page) const
{
  PaginatedHierarchy result;
  if (entries.empty()) return result;
  if (max_entries_per_page == 0) max_entries_per_page = 4096;

  // Safety cap on the number of pages we're willing to emit. The walk
  // is structurally bounded — every entry is visited exactly once, and
  // each spawned page corresponds to exactly one entry whose subtree
  // exceeds max_entries_per_page — so #pages ≤ #entries. Use that as a
  // hard cap with a small slack: if we ever exceed it, abort with a
  // warning. v1 of this routine had a runaway bug; keeping a guard
  // makes any future regression abort fast instead of OOM-ing the
  // host (one prior accident at 112 GB RSS was enough).
  const std::size_t max_pages_safety = entries.size() + 64;

  // 1) Build key -> entry map and parent -> children adjacency. Sorting
  // children deterministically (x, y, z) makes the page partition
  // reproducible run-to-run for byte-stable output.
  std::unordered_map<EPTkey, const LASvlr_copc_entry*, EPTKeyHasher> by_key;
  std::unordered_map<EPTkey, std::vector<EPTkey>, EPTKeyHasher> children;
  by_key.reserve(entries.size());
  for (const auto& e : entries)
  {
    EPTkey k(e.key.depth, e.key.x, e.key.y, e.key.z);
    by_key[k] = &e;
    if (k.d > 0)
    {
      EPTkey parent = k.get_parent();
      children[parent].push_back(k);
    }
  }
  for (auto& kv : children)
  {
    std::sort(kv.second.begin(), kv.second.end(),
              [](const EPTkey& a, const EPTkey& b) {
                if (a.x != b.x) return a.x < b.x;
                if (a.y != b.y) return a.y < b.y;
                return a.z < b.z;
              });
  }

  // 2) Subtree size per key (entries in subtree rooted at key, incl. self).
  // Process deepest-first so children are computed before parents.
  std::unordered_map<EPTkey, U32, EPTKeyHasher> subtree_count;
  subtree_count.reserve(entries.size());
  std::vector<EPTkey> all_keys;
  all_keys.reserve(entries.size());
  for (const auto& e : entries) all_keys.emplace_back(e.key.depth, e.key.x, e.key.y, e.key.z);
  std::sort(all_keys.begin(), all_keys.end(),
            [](const EPTkey& a, const EPTkey& b) { return a.d > b.d; });
  for (const EPTkey& k : all_keys)
  {
    U32 sum = 1;
    auto cit = children.find(k);
    if (cit != children.end())
      for (const EPTkey& c : cit->second) sum += subtree_count[c];
    subtree_count[k] = sum;
  }

  // 3) Walk the tree top-down, partitioning into pages. A child whose
  // subtree exceeds max_entries_per_page is spawned as its own page;
  // the parent page gets a child-page-reference entry (point_count = -1)
  // pointing to it. The initial offset/byte_size in child refs are 0
  // placeholders — the writer fills in absolute file offsets after
  // writer_las->close() flushes, when the eVLR's actual file position
  // is known.
  struct Page { std::vector<LASvlr_copc_entry> ents; U64 byte_offset = 0; };
  std::vector<Page> pages;
  pages.emplace_back();

  // Track positions of child refs *within each page's entry vector* so
  // the post-pages-built phase can compute their byte offsets within
  // the serialized payload. (parent_page_idx, ent_idx_in_page, child_root_key)
  struct PendingRef { std::size_t parent_page; std::size_t ent_index; EPTkey child_root; };
  std::vector<PendingRef> pending_refs;

  // Accumulated per-page tracking: spawn a child page whenever absorbing
  // the next subtree would push the current page past max_entries_per_page.
  // This is a softer rule than "spawn on subtree-size threshold alone" —
  // a parent with N children, each below the threshold, would otherwise
  // absorb all N (page = 1 + N*sub_avg ≈ N*max in the worst case).
  // With accumulated tracking, the page tops up to roughly max entries
  // and the rest spill into spawned child pages.
  //
  // Worst-case overflow: an octree node has at most 8 children. If the
  // page is at capacity (max - 1, say) and all 8 children must spawn
  // (each subtree larger than what fits), the page ends up holding the
  // parent + 8 child-refs ≈ max + 8. Bounded; the COPC reader doesn't
  // care about page size, only that page byte_size matches what the
  // child-ref points to. Document the worst case but don't fight it.
  std::function<bool(const EPTkey&, std::size_t)> visit;
  visit = [&](const EPTkey& key, std::size_t page_idx) -> bool {
    if (pages.size() > max_pages_safety) return false;  // safety abort
    auto by_it = by_key.find(key);
    if (by_it == by_key.end()) return true;
    pages[page_idx].ents.push_back(*by_it->second);
    auto cit = children.find(key);
    if (cit == children.end()) return true;
    for (const EPTkey& c : cit->second)
    {
      const U32 sub = subtree_count[c];
      // Absorb only if the WHOLE subtree fits in the current page's
      // remaining headroom. Otherwise spawn a child page — the
      // recursive visit() will further subdivide if needed.
      if ((U32)pages[page_idx].ents.size() + sub <= max_entries_per_page)
      {
        if (!visit(c, page_idx)) return false;
      }
      else
      {
        const std::size_t new_idx = pages.size();
        pages.emplace_back();
        LASvlr_copc_entry ref;
        ref.key.depth = c.d;
        ref.key.x = c.x;
        ref.key.y = c.y;
        ref.key.z = c.z;
        ref.offset = 0;       // placeholder; patched post-close
        ref.byte_size = 0;    // placeholder; patched post-close
        ref.point_count = -1; // child page marker
        const std::size_t ent_idx = pages[page_idx].ents.size();
        pages[page_idx].ents.push_back(ref);
        pending_refs.push_back({page_idx, ent_idx, c});
        if (!visit(c, new_idx)) return false;
      }
    }
    return true;
  };

  EPTkey root = EPTkey::root();
  bool ok = true;
  if (by_key.count(root))
  {
    ok = visit(root, 0);
  }
  else
  {
    // Defensive: caller should have inserted a root entry. Emit a
    // zero-count placeholder so the file stays navigable.
    LASvlr_copc_entry r;
    r.key.depth = 0; r.key.x = 0; r.key.y = 0; r.key.z = 0;
    r.offset = 0; r.byte_size = 0; r.point_count = 0;
    pages[0].ents.push_back(r);
  }
  if (!ok)
  {
    warning("COPC writer: paginated hierarchy exceeded the safety cap of "
            "%llu pages — likely a bug. Falling back to a single-page emit.\n",
            (unsigned long long)max_pages_safety);
    // Single-page fallback: dump every entry into one page.
    pages.clear();
    pages.emplace_back();
    pending_refs.clear();
    pages[0].ents.assign(entries.begin(), entries.end());
  }

  // 4) Compute per-page byte offsets within the payload (linear layout).
  U64 cursor = 0;
  for (auto& p : pages)
  {
    p.byte_offset = cursor;
    cursor += p.ents.size() * sizeof(LASvlr_copc_entry);
  }
  const U64 total_size = cursor;
  result.root_page_size = pages.empty() ? 0 : pages[0].ents.size() * sizeof(LASvlr_copc_entry);

  // 5) Resolve each PendingRef to its target page's (byte_offset, byte_size).
  // Map each page's *first entry's key* (which is the subtree root by visit's
  // construction) to that page's index for O(1) lookup.
  std::unordered_map<EPTkey, std::size_t, EPTKeyHasher> page_root_to_idx;
  page_root_to_idx.reserve(pages.size());
  for (std::size_t i = 0; i < pages.size(); ++i)
  {
    if (pages[i].ents.empty()) continue;
    const auto& e0 = pages[i].ents.front();
    page_root_to_idx[EPTkey(e0.key.depth, e0.key.x, e0.key.y, e0.key.z)] = i;
  }
  result.child_refs.reserve(pending_refs.size());
  for (const auto& pr : pending_refs)
  {
    auto it = page_root_to_idx.find(pr.child_root);
    if (it == page_root_to_idx.end()) continue;  // shouldn't happen
    const Page& tgt = pages[it->second];
    ChildPageRef cref;
    cref.byte_offset_in_payload =
        pages[pr.parent_page].byte_offset + pr.ent_index * sizeof(LASvlr_copc_entry);
    cref.target_page_relative_offset = tgt.byte_offset;
    cref.target_page_byte_size = (U32)(tgt.ents.size() * sizeof(LASvlr_copc_entry));
    result.child_refs.push_back(cref);
  }

  // 6) Serialize all pages into one contiguous buffer.
  result.data.resize(total_size);
  U8* dst = result.data.data();
  for (const auto& p : pages)
  {
    const std::size_t bytes = p.ents.size() * sizeof(LASvlr_copc_entry);
    std::memcpy(dst, p.ents.data(), bytes);
    dst += bytes;
  }
  return result;
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
