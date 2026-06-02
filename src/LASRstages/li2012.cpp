#include "li2012.h"
#include "PointSchema.h"
#include "print.h"

#include <algorithm>
#include <cstdint>
#include <limits>

// File-local NA sentinel. INT32_MIN == R's NA_INTEGER, so R callers see NA.
static constexpr int32_t TREEID_NA = std::numeric_limits<int32_t>::min();

LASRli2012::LASRli2012()
  : unicity_table(std::make_shared<
        std::unordered_map<ApexKey, int, ApexKeyHash>>()),
    global_counter(std::make_shared<std::atomic<int>>(0)),
    table_mutex(std::make_shared<std::mutex>())
{
}

bool LASRli2012::set_parameters(const nlohmann::json& stage)
{
  dt1 = stage.value("dt1", 1.5);
  dt2 = stage.value("dt2", 2.0);
  R = stage.value("R", 2.0);
  Zu = stage.value("Zu", 15.0);
  hmin = stage.value("hmin", 2.0);
  speed_up = stage.value("speed_up", 10.0);
  store_in_attribute = stage.value("store_in_attribute",
                                   std::string("treeID"));

  if (dt1 <= 0)      { last_error = "dt1 must be positive";       return false; }
  if (dt2 <= 0)      { last_error = "dt2 must be positive";       return false; }
  if (R < 0)         { last_error = "R must be non-negative";     return false; }
  if (Zu <= 0)       { last_error = "Zu must be positive";        return false; }
  if (hmin <= 0)     { last_error = "hmin must be positive";      return false; }
  if (speed_up <= 0) { last_error = "speed_up must be positive";  return false; }
  if (store_in_attribute.empty())
  { last_error = "store_in_attribute must be a non-empty string"; return false; }

  return true;
}

bool LASRli2012::process(PointCloud*& las)
{
  if (!las) { last_error = "Uninitialized PointCloud"; return false; }
  if (las->npoints == 0) return true;  // nothing to do

  // Validate store_in_attribute exists with the right type and scale.
  // Only INT32 with scale=1 and offset=0 round-trips integer tree IDs
  // and the NA sentinel safely.
  int idx = las->header->schema.get_attribute_index(store_in_attribute);
  if (idx < 0)
  {
    last_error = store_in_attribute +
      " is not present in the point cloud. "
      "Use add_extrabytes() before li2012().";
    return false;
  }
  const Attribute& attr = las->header->schema.attributes[idx];
  if (attr.type != INT32 || attr.scale_factor != 1.0 ||
      attr.value_offset != 0.0)
  {
    last_error = store_in_attribute +
      " must be of type 'int' with scale=1 and offset=0.";
    return false;
  }

  AttributeAccessor set_treeid(store_in_attribute);

  // --- Initial fill: every point starts as TREEID_NA. ---
  for (size_t i = 0; i < las->npoints; ++i)
  {
    las->seek(i);
    set_treeid(&las->point, (double)TREEID_NA);
  }

  // --- hmin pre-check: max-Z over filter-passing points only. ---
  double maxZ = -std::numeric_limits<double>::infinity();
  {
    Point pp;
    pp.set_schema(&las->header->schema);
    for (size_t i = 0; i < las->npoints; ++i)
    {
      if (!las->get_point(i, &pp, &pointfilter)) continue;
      double z = pp.get_z();
      if (z > maxZ) maxZ = z;
    }
  }
  if (maxZ < hmin)
  {
    warning("'hmin' is higher than the highest point. No tree segmented.\n");
    return true;  // every point already TREEID_NA from the fill above
  }

  // --- Local-maximum precompute. Matches lidR filter_local_maxima(R, 0, true):
  //     half-window = R/2, circular window, on Z. R==0 => every point is a
  //     candidate apex (short-circuit). ---
  std::vector<char> is_lm(las->npoints, 0);

  if (R == 0)
  {
    std::fill(is_lm.begin(), is_lm.end(), 1);
  }
  else
  {
    if (verbose) print("Building spatial index for local-max pass\n");
    las->build_partition();

    const double hws = R / 2.0;
    Point lm_pp;
    lm_pp.set_schema(&las->header->schema);

    for (size_t i = 0; i < las->npoints; ++i)
    {
      if (!las->get_point(i, &lm_pp, &pointfilter)) continue;
      double z_i = lm_pp.get_z();

      Circle window(lm_pp.get_x(), lm_pp.get_y(), hws);
      std::vector<Point> neighbours;
      las->query(&window, neighbours, &pointfilter);

      bool higher_found = false;
      bool tied_with_existing_lm = false;
      for (auto& nb : neighbours)
      {
        int j = las->get_index(&nb);
        if (j == (int)i) continue;
        double z_j = nb.get_z();
        if (z_j > z_i) { higher_found = true; break; }
        // lidR tie-break: if a tied neighbour is already tagged LM, current
        // point loses (first-tagged wins).
        if (z_j == z_i && is_lm[j]) { tied_with_existing_lm = true; break; }
      }
      if (!higher_found && !tied_with_existing_lm) is_lm[i] = 1;
    }
  }

  // --- Working set U: filter-passing indices, sorted by Z descending.
  //     std::sort (NOT stable_sort) to match lidR's tie behaviour. ---
  std::vector<int> order;
  order.reserve(las->npoints);
  std::vector<double> Z_cache(las->npoints,
      std::numeric_limits<double>::quiet_NaN());
  {
    Point sort_pp;
    sort_pp.set_schema(&las->header->schema);
    for (size_t i = 0; i < las->npoints; ++i)
    {
      if (!las->get_point(i, &sort_pp, &pointfilter)) continue;
      Z_cache[i] = sort_pp.get_z();
      order.push_back((int)i);
    }
  }
  std::sort(order.begin(), order.end(),
            [&Z_cache](int a, int b) { return Z_cache[a] > Z_cache[b]; });

  // inU[k] flags whether order[k] is still in the working set. We never delete
  // from order; flipping inU is the equivalent of lidR's U.swap(temp).
  std::vector<char> inU(order.size(), 1);

  return true;
}

int LASRli2012::register_apex(const ApexKey& key)
{
  std::lock_guard<std::mutex> lock(*table_mutex);
  auto it = unicity_table->find(key);
  if (it != unicity_table->end()) return it->second;
  int tree_id = ++(*global_counter);
  (*unicity_table)[key] = tree_id;
  return tree_id;
}

void LASRli2012::clear(bool last)
{
  // No per-chunk state to reset. The shared dedup table must persist
  // across chunks under concurrent-files; we deliberately do NOT
  // reset unicity_table or global_counter on clear(false).
  // On clear(last=true) the shared_ptrs die with the stage anyway.
  (void)last;
}
