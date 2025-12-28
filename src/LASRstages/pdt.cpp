#include "pdt.h"
#include "Profiler.h"
#include "Raster.h"
#include "NA.h"

#include "nanoflann/nanoflann.h"
#include "hporro/delaunay.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>

struct PLAdaptor
{
  const std::vector<PointLAS>& pts;

  PLAdaptor(const std::vector<PointLAS>& pts_) : pts(pts_) {}

  inline size_t kdtree_get_point_count() const { return pts.size(); }

  inline double kdtree_get_pt(const size_t idx, const size_t dim) const {
    return (dim == 0 ? pts[idx].x : pts[idx].y);
  }

  template <class BBOX>
  bool kdtree_get_bbox(BBOX&) const { return false; }
};

LASRpdt::LASRpdt()
{
  this->las = nullptr;
  this->d = nullptr;

  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRpdt::set_parameters(const nlohmann::json& stage)
{
  this->max_iteration_distance = stage.value("distance", 1);
  this->max_iteration_angle = stage.value("angle", 15);
  this->seed_resolution_search = stage.value("res", 50);
  double min_size = stage.value("min_size", 0.1);
  this->min_triangle_size = min_size*min_size;
  this->offset = stage.value("offset", 0.05);
  this->classification = stage.value("class", 2);
  this->max_iter = stage.value("max_iter", 1000);
  this->buffer_size = stage.value("buffer_size", 30);
  return true;
}

bool LASRpdt::process(PointCloud*& las)
{
  Profiler tot;

  this->las = las;
  this->x_offset = las->header->min_x;
  this->y_offset = las->header->min_y;
  this->z_offset = las->header->min_z;
  size_t n = las->npoints;

  double min_x = las->header->min_x;
  double min_y = las->header->min_y;
  double max_x = las->header->max_x;
  double max_y = las->header->max_y;

  // Used to retain if a point is already part of the triangulation
  std::vector<bool> inserted(n, false);

  Profiler prof;

  // =========================================
  // Order in which points should be processed
  // =========================================

  if (verbose) print(" Sorting points: ");

  // Sort by height (first element of pair)
  std::vector<std::pair<float, unsigned int>> items;
  items.reserve(n);
  unsigned int i = 0;
  while (las->read_point()) items.emplace_back(las->point.get_z(), i++);
  std::sort(items.begin(), items.end(),  [](const auto& a, const auto& b) {  return a.first < b.first; });
  std::vector<unsigned int> index; index.reserve(n);
  for (const auto& item : items) index.push_back(item.second);

  if (verbose) print("%.2f secs\n", prof.elapsed());

  // =====================
  // Find some seed points
  // =====================

  make_seeds();

  if (seeds.size() == 0)
  {
    last_error = "0 point to process";
    return false;
  }

  // ==========================
  // Add virtual edge seeds
  // ==========================

  if (buffer_size > 0)
  {
    make_buffer(seeds, min_x, min_y, max_x, max_y, buffer_size);
  }

  // ============================
  // Triangulate edges first
  // (for offset to payload)
  // ============================

  prof = Profiler();
  int t = -1;
  double bs = buffer_size;
  double xo = x_offset;
  double yo = y_offset;

  Grid gd(min_x-xo-bs, min_y-yo-bs, max_x-xo+bs, max_y-yo+bs, 1);
  d = new Triangulation(gd);

  // For the few first points spatial index search is slow because triangle
  // are too big. We can deactivate spatial indexing this is actually faster
  d->desactivate_spatial_index();

  if (verbose)  print(" Buffer: adding %lu virtual seed ", vbuff.size());

  for (auto& p : vbuff)
  {
    Vec2 pt(p.x - xo, p.y - yo);
    t = d->findContainerTriangleFast(pt);
    if (!d->delaunayInsertion(pt, t))
    {
      last_error = "Internal error: virtual seed point not inserted for unknown reason";
      return false;
    }
  }

  if (verbose) print(" took %.2f secs\n", prof.elapsed());

  // ============================
  // Triangulate the seed points
  // ============================

  if (verbose) print(" Iteration 0: ");

  prof = Profiler();

  int iteration = 0;

  if (verbose) print("adding %lu seeds to the ground", seeds.size());

  for (auto& seed : seeds)
  {
    Vec2 pt(seed.x - xo, seed.y - yo);
    t = d->findContainerTriangleFast(pt);

    if (d->delaunayInsertion(pt, t))
    {
      index_map.push_back(seed.FID);
      inserted[seed.FID] = true;
    }
  }

  if (verbose) print(" took %.2f secs\n", prof.elapsed());


  // Now the edges and seeds are inserted we can leverage spatial indexing.
  d->activate_spatial_index();

  // =============================
  // Progressive TIN densification
  // =============================

  if (verbose) print(" Progressive TIN densification:\n");

  prof = Profiler();

  int count = 0;

  std::vector<bool> active_regions(gd.get_ncells(), true);

  // We loop while we are adding new triangles
  do
  {
    if (max_iter == 0) break;

    // Reset the dirty tracker inside the triangulation
    // so it only records changes happening during this specific iteration
    d->reset_dirty_cells();

    count = 0;

    /*progress->reset();
    progress->set_prefix("PDT");
    progress->set_total(las->npoints);
    progress->show();*/

    Profiler prof2;

    iteration++;
    t = -1;

    // Process all the points in order
    for (int id : index)
    {
      // This point has already been inserted in the triangulation. Skip next computations
      if (inserted[id]) continue;

      las->seek(id);
      //unsigned int id = las->current_point;
      double x = las->point.get_x();
      double y = las->point.get_y();
      double z = las->point.get_z();

      //(*progress)++;
      //progress->show();

      if (pointfilter.filter(&las->point)) continue;

      PointXYZ P(x, y, z);
      Vec2 pt(x - xo, y - yo);

      // -Optimization Step
      // Check if the region containing this point was modified in the previous step.
      // If the cell is valid and was NOT marked active/modified in the previous loop,
      // we can skip this point entirely. The triangulation here hasn't changed.
      int cell_id = gd.cell_from_xy(pt.x, pt.y);
      if (!active_regions[cell_id]) continue;

      // Search the triangle where to insert. Benchmarking search time for later speed improvement
      t = d->findContainerTriangleFast(pt);

      if (t < 0) continue;

      // Retrieve the vertices of the triangle
      TriangleXYZ triangle = get_triangle(t);

      // Skip too small triangles. Small triangle are frozen and cannot be subdivided.
      if (triangle.square_max_edge_size() < min_triangle_size)
      {
        // will be skipped next time since anyway
        // the triangle is not frozen.
        inserted[id] = true;
        continue;
      }

      double dist_d, angle;
      if (!axelsson_metrics(P, triangle, dist_d, angle)) continue;

      if (angle < max_iteration_angle && dist_d < max_iteration_distance)
      {
        if (d->delaunayInsertion(pt, t))
        {
          index_map.push_back(id);
          inserted[id] = true;
          count++;
        }
      }

      progress->update(id);
    }

    // Prepare for next iteration ---
    // The active regions for the NEXT loop are the ones that became "dirty"
    // during THIS loop.
    active_regions = d->get_dirty_cells();

    float perc = (double)count / (double)d->vcount;

    if (iteration == 1 && perc < 0.90)
    {
      warning("Internal debug warning. The first iteration is expected to insert more than 90%% of the point.\n");
    }

    if (verbose) print("  Iteration %d: adding %d points (+%.1f%%) to the ground took %.2f secs\n", iteration, count, perc * 100, prof2.elapsed());

    if (perc < 0.5f / 1000.0f) break; // We actually stop the loop when < 0.05% additions

  } while (count > 0 && iteration < max_iter);


  if (verbose) print(" PDT took %.2f secs\n", tot.elapsed());

  //progress->done();

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  AttributeAccessor get_and_set_classification("Classification");

  while (las->read_point())
  {
    if (get_and_set_classification(&las->point) == classification)
      get_and_set_classification(&las->point, 1);
  }

  // =====================================
  // Classify ground points
  // =====================================

  for (unsigned int i = 0; i < d->tcount; i++)
  {
    // Check all 3 vertices of the triangle
    for (int j = 0; j < 3; j++)
    {
      int v_idx = d->triangles[i].v[j];

      if (v_idx < 4) continue;                     // 1. Skip Super Vertices (0, 1, 2, 3)
      if (v_idx < (4 + vbuff.size())) continue;    // 2. Skip Virtual Buffer points
      int map_idx = v_idx - 4 -  vbuff.size();     // 3. Calculate the correct index for index_map
      if (map_idx >= index_map.size()) continue;   // Safety check (optional)

      las->seek(index_map[map_idx]);
      get_and_set_classification(&las->point, classification);
    }
  }

  return true;
}

// ===========================
// Helper methods
// ===========================

TriangleXYZ LASRpdt::get_triangle(int t)
{
  int idA = d->triangles[t].v[0];
  int idB = d->triangles[t].v[1];
  int idC = d->triangles[t].v[2];

  PointXYZ A, B, C;
  query_coordinates(idA, A);
  query_coordinates(idB, B);
  query_coordinates(idC, C);

  return TriangleXYZ(A, B, C);
}

void LASRpdt::query_coordinates(int id, PointXYZ& p)
{
  if (id < 4)
  {
    p.x = d->vertices[id].pos.x + x_offset;
    p.y = d->vertices[id].pos.y + y_offset;
    p.z = z_default;
  }
  else if (id < (vbuff.size() + 4))
  {
    const auto& q = vbuff[id - 4];
    p.x = q.x;
    p.y = q.y;
    p.z = q.z;
  }
  else
  {
    Point tmp;
    tmp.set_schema(&las->header->schema);
    las->get_point(index_map[id - 4 - vbuff.size()], &tmp);
    p.x = tmp.get_x();
    p.y = tmp.get_y();
    p.z = tmp.get_z();
  }
}

bool LASRpdt::axelsson_metrics(const PointXYZ& P, const TriangleXYZ& triangle, double& dist_d, double& angle)
{
  PointXYZ A = triangle.A;
  PointXYZ n = triangle.normal();

  // Projection and distance
  PointXYZ v = P - A;
  dist_d = std::abs(v.dot(n));
  PointXYZ P_proj = P - n * v.dot(n);
  if (!triangle.contains(P_proj)) return false;

  // Distances to vertices
  double h0 = P_proj.distance(triangle.A);
  double h1 = P_proj.distance(triangle.B);
  double h2 = P_proj.distance(triangle.C);

  double alpha = atan2(dist_d, h0) * 180.0 / M_PI;
  double beta  = atan2(dist_d, h1) * 180.0 / M_PI;
  double gamma = atan2(dist_d, h2) * 180.0 / M_PI;

  angle = MAX3(alpha, beta, gamma);
  return true;
}

/*bool LASRpdt::write()
{
  if (ofile.empty()) return true;

  progress->set_total(d->tcount);
  progress->set_prefix("Write triangulation");
  progress->show();

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<TriangleXYZ> triangles;

  for (unsigned int i = 0 ; i < d->tcount; i++)
  {
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    //print("Vertices %d %d %d\n", idA, idB, idC);

    PointLAS A,B,C;
    las->get_point(index_map[idA], A);
    las->get_point(index_map[idB], B);
    las->get_point(index_map[idC], C);

    Vec2 a = d->vertices[d->triangles[i].v[0]].pos;
    Vec2 b = d->vertices[d->triangles[i].v[1]].pos;
    Vec2 c = d->vertices[d->triangles[i].v[2]].pos;

    TriangleXYZ triangle(A, B, C);
    triangle.make_clock_wise();
    triangles.push_back(triangle);

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  progress->done();

  #pragma omp critical (write_ptd)
  {
    vector.write(triangles);
  }

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Writing Delaunay triangulation took %.2f sec\n", second);
    // # nocov end
  }

  return true;
}*/

void LASRpdt::clear(bool last)
{
  delete d;
  d = nullptr;
  index_map.clear();
}

// See LASRtriangulate::interpolate
void LASRpdt::interpolate(Raster* r) const
{
  Profiler prof;
  prof.tic();

  r->set_value(0, r->get_nodata());

  // Generate the raster points in this triangle
  double xres = r->get_xres();
  double yres = r->get_yres();

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->tcount; i++)
  {
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    PointXYZ A,B,C;
    Point p;
    p.set_schema(&las->header->schema);

    las->get_point(index_map[idA], &p);
    A.x = p.get_x();
    A.y = p.get_y();
    A.z = p.get_z();

    las->get_point(index_map[idB], &p);
    B.x = p.get_x();
    B.y = p.get_y();
    B.z = p.get_z();

    las->get_point(index_map[idC], &p);
    C.x = p.get_x();
    C.y = p.get_y();
    C.z = p.get_z();

    TriangleXYZ triangle(A, B, C);

    double minx = ROUNDANY(triangle.xmin() - 0.5 * xres, xres);
    double miny = ROUNDANY(triangle.ymin() - 0.5 * yres, yres);
    double maxx = ROUNDANY(triangle.xmax() - 0.5 * xres, xres) + xres;
    double maxy = ROUNDANY(triangle.ymax() - 0.5 * yres, yres) + yres;

    for (double x = minx ; x <= maxx ; x += xres)
    {
      for (double y = miny ; y <= maxy ; y += yres)
      {
        if (!triangle.contains({x,y})) continue;

        PointXYZ p(x,y);
        triangle.linear_interpolation(p);

        int cell = r->cell_from_xy(x,y);
        if (cell != -1) r->set_value(cell, p.z);
      }
    }
  }

  prof.toc();
  if (verbose) print("  DTM interpolation took %.2f secs\n", prof.elapsed());
}

void LASRpdt::make_seeds()
{
  auto prof = Profiler();

  if (verbose) print(" Searching seeds: ");

  Grid grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, seed_resolution_search);
  seeds.clear();
  seeds.resize(grid.get_ncells());
  for (auto& seed : seeds) seed.z = std::numeric_limits<double>::max();

  int counter = 0;
  while (las->read_point())
  {
    if (pointfilter.filter(&las->point)) continue;

    double x = las->point.get_x();
    double y = las->point.get_y();
    double z = las->point.get_z();
    unsigned int id = las->current_point;
    int cell = grid.cell_from_xy(x, y);

    PointLAS& p = seeds[cell];
    if (z < p.z)
    {
      p.x = x;
      p.y = y;
      p.z = z;
      p.FID = id;
    }
  }

  // We created 1 seed per grid cell. Some cell maybe empty. In these cells: z = INF
  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const PointXYZ& p) { return p.z == std::numeric_limits<double>::max(); });
  seeds.erase(new_end, seeds.end());

  // In the triangulation the 4 first default points at infinity need a Z value. We will use mean z of seeds
  z_default = std::accumulate(seeds.begin(), seeds.end(), 0.0, [](double sum, const PointXYZ& p) { return sum + p.z; }) / seeds.size();
  z_default -= 100;

  if (verbose) print("%.2f secs\n", prof.elapsed());
}

void LASRpdt::make_buffer(const std::vector<PointLAS>& xyz, double xmin, double ymin, double xmax, double ymax, double buffer)
{
  std::mt19937 generator(std::random_device{}());
  double noise_magnitude = 1;
  std::uniform_real_distribution<double> distribution(-noise_magnitude / 2.0, noise_magnitude / 2.0);

  xmax += (buffer-1);
  ymax += (buffer-1);
  xmin -= (buffer-1);
  ymin -= (buffer-1);

  double dx = xmax - xmin;
  double dy = ymax - ymin;

  int nx = std::round(dx / seed_resolution_search);
  int ny = std::round(dy / seed_resolution_search);

  double sx = dx / nx;
  double sy = dy / ny;

  vbuff.clear();
  vbuff.reserve(2 * (nx + ny) + 4); // perimeter only

  PointLAS p; p.z = 0;
  double x, y, x_noise, y_noise;

  // Bottom and top edges
  for (int i = 0; i <= nx; i++)
  {
    x = xmin + i * sx;
    x_noise = distribution(generator);

    // Bottom (ymin)
    y_noise = distribution(generator);
    p.x = x + x_noise;
    p.y = ymin + y_noise;
    vbuff.push_back(p);

    // Top
    y_noise = distribution(generator);
    p.x = x + x_noise;
    p.y = ymax + y_noise;
    vbuff.push_back(p);
  }

  // Left and right edges (skip corners to avoid duplicates)
  for (int j = 1; j < ny; j++)
  {
    y = ymin + j * sy;
    y_noise = distribution(generator);

    // Left (xmin)
    x_noise = distribution(generator);
    p.x = xmin + x_noise;
    p.y = y + y_noise;
    vbuff.push_back(p);

    // Right (xmax)
    x_noise = distribution(generator);
    p.x = xmax + x_noise;
    p.y = y + y_noise;
    vbuff.push_back(p);
  }

  using namespace nanoflann;
  PLAdaptor adaptor(xyz);
  typedef KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<double, PLAdaptor>,PLAdaptor,2> kd_tree_t;

  kd_tree_t index(2, adaptor, KDTreeSingleIndexAdaptorParams(10));
  index.buildIndex();

  for (auto& p : vbuff)
  {
    double query_pt[2] = { p.x, p.y };

    size_t ret_index;
    double out_dist_sqr;

    KNNResultSet<double> resultSet(1);
    resultSet.init(&ret_index, &out_dist_sqr);

    index.findNeighbors(resultSet, query_pt, nanoflann::SearchParameters());

    p.z = xyz[ret_index].z;
  }
}

// See LASRtriangulate::interpolate
/*void LASRpdt::interpolate(std::vector<double>& x) const
{
  Profiler prof;
  prof.tic();

  x.resize(las->npoints);
  std::fill(x.begin(), x.end(), NA_F64);

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->tcount; i++)
  {
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    PointXYZ A,B,C;
    Point p(&las->header->schema);

    las->get_point(index_map[idA], &p);
    A.x = p.get_x();
    A.y = p.get_y();
    A.z = p.get_z();

    las->get_point(index_map[idB], &p);
    B.x = p.get_x();
    B.y = p.get_y();
    B.z = p.get_z();

    las->get_point(index_map[idC], &p);
    C.x = p.get_x();
    C.y = p.get_y();
    C.z = p.get_z();
    TriangleXYZ triangle(A, B, C);

    std::vector<Point> points;
    las->query(&triangle, points);

    for (auto& p : points)
    {
      triangle.linear_interpolation(p);
      x[p.FID] = p.z;
    }
  }

  prof.toc();
  if (verbose) print("  LAS interpolation took %.2f secs\n", prof.elapsed());
}*/

