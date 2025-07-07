#include "sort.h"

#include "GridPartition.h"

bool LASRsort::set_parameters(const nlohmann::json& stage)
{
  spatial = stage.value("spatial", true);
  return true;
}

bool LASRsort::process(PointCloud*& las)
{
  auto start_time = std::chrono::high_resolution_clock::now();

  // Sort by channel > gpstime > return number sort
  //if (!las->sort()) return false;

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  float second = (float)duration.count()/1000.0f;

  //print("  First sort took %.2f sec.\n", second);*/

  if (!spatial) return true;

  start_time = std::chrono::high_resolution_clock::now();

  double res = 50 * crs.get_linear_units();

  // Spatial sort
  GridPartition grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, res);
  while (las->read_point()) grid.insert(las->point.get_x(), las->point.get_y());
  auto& umap = grid.map;
  std::map<int, std::vector<Interval>> sorted_map(umap.begin(), umap.end());   // Use a map to automatically sort the keys

  std::vector<int> order;
  order.reserve(las->npoints);

  for (const auto& pair : sorted_map)
  {
    for (auto interval : pair.second)
    {
      int cell = pair.first;
      int start = interval.start;
      int end = interval.end;

      for(int i = start ; i <= end; i++) order.push_back(i);
    }
  }

  if (order.size() != las->header->number_of_point_records)
  {
    last_error = "Internal error. Invalid order size";
    return false;
  }

  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  second = (float)duration.count()/1000.0f;

  //print("  Indexing took %.2f sec.\n", second);

  start_time = std::chrono::high_resolution_clock::now();

  if (!las->sort(order)) return false;

  end_time = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  second = (float)duration.count()/1000.0f;

  //print("  Second sort took %.2f sec.\n", second);

  return true;
}