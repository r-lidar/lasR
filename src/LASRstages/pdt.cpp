#include "pdt.h"
#include "Profiler.h"
#include "Raster.h"
#include "NA.h"

#include "hporro/delaunay.h"

#include <algorithm>
#include <cmath>

#include "nanoflann/nanoflann.h"
#include <vector>
#include <cmath>

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

  print("PDT parameters\n");
  print("max_iteration_distance = %.2lf\n",max_iteration_distance);
  print("max_iteration_angle = %.2lf\n", max_iteration_angle);
  print("seed_resolution_search = %.2lf\n", seed_resolution_search);
  print("min_triangle_size = %.2lf\n", min_size);
  print("max_iter = %d\n\n", max_iter);
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
  for (const auto& item : items)index.push_back(item.second);

  if (verbose) print("%.2f secs\n", prof.elapsed());

  // =====================
  // Find some seed points
  // =====================

  prof = Profiler();

  if (verbose) print(" Searching seeds: ");

  Grid grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, seed_resolution_search);
  std::vector<PointLAS> seeds;
  seeds.resize(grid.get_ncells());
  for (auto& seed : seeds) seed.z =  std::numeric_limits<double>::max();

  int counter = 0;
  while (las->read_point())
  {
    counter++;

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

  if (counter == 0)
  {
    last_error = "0 point to process";
    return false;
  }

  // We created 1 seed per grid cell. Some cell maybe empty. In these cells: z = INF
  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const PointXYZ& p) { return p.z == std::numeric_limits<double>::max(); });
  seeds.erase(new_end, seeds.end());

  // In the triangulation the 4 first default points at infinity need a Z value. We will use mean z of seeds
  z_default = std::accumulate(seeds.begin(), seeds.end(), 0.0, [](double sum, const PointXYZ& p) { return sum + p.z; }) / seeds.size();
  z_default -= 100;

  if (verbose) print("%.2f secs\n", prof.elapsed());

  // ==========================
  // Add virtual edge seeds
  // ==========================

  if (buffer_size > 0)
  {
    add_virtual_buffer(seeds, las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, buffer_size);
  }

  // ============================
  // Triangulate edges first
  // (for offset to payload)
  // ============================

  Grid gd(las->header->min_x-x_offset-buffer_size,
          las->header->min_y-y_offset-buffer_size,
          las->header->max_x-x_offset+buffer_size,
          las->header->max_y-y_offset+buffer_size, 1);
  d = new Triangulation(gd);
  int t = -1;

  if (verbose) print(" Buffer: ");

  prof = Profiler();

  if (verbose) print("adding %lu virtual seed ", vbuf.size());

  for (auto& p : vbuf)
  {
    // Offset to fit in the bounding box of the triangulation which is +/- 1e8
    // (Also get extra accuracy)
    Vec2 pt(p.x - x_offset, p.y - y_offset);
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

  int count = 0;
  int iteration = 0;
  unsigned int discared = 0;

  if (verbose) print("adding %lu seeds to the ground", seeds.size());

  for (auto& seed : seeds)
  {
    // Offset to fit in the bounding box of the triangulation which is +/- 1e8
    // (Also get extra accuracy)
    Vec2 pt(seed.x - x_offset, seed.y - y_offset);

    t = d->findContainerTriangleFast(pt);

    if (d->delaunayInsertion(pt, t))
    {
      index_map.push_back(seed.FID);
      inserted[seed.FID] = true;
      count++;
    }
  }

  if (verbose) print(" took %.2f secs\n", count, prof.elapsed());

  // =============================
  // Progressive TIN densification
  // =============================

  if (verbose) print(" Progressive TIN densification:\n");

  prof = Profiler();
  std::chrono::duration<double> total_search_time(0);

  int kk = 0;
  // We loop while we are adding new triangles
  do
  {
    if (max_iter == 0) break;

    progress->reset();
    progress->set_prefix("PDT");
    progress->set_total(las->npoints);
    progress->show();

    Profiler prof2;

    iteration++;
    count = 0;
    t = -1;

    // Process all the points in order
    for (int id : index)
    {

      las->seek(id);
      //unsigned int id = las->current_point;
      double x = las->point.get_x();
      double y = las->point.get_y();
      double z = las->point.get_z();

      (*progress)++;
      progress->show();

      // This point has already been inserted in the triangulation. Skip next computations
      if (inserted[id]) continue;

      if (pointfilter.filter(&las->point)) continue;

      kk++;

      PointXYZ P(x, y, z);
      Vec2 pt(x - x_offset, y - y_offset);

      // Search the triangle where to insert. Benchmarking search time for later speed improvement
      t = d->findContainerTriangleFast(pt);

      if (t < 0) continue;

      // Retrieve the vertices of the triangle
      TriangleXYZ triangle = get_triangle(t);

      // Skip too small triangles. Small triangle are frozen and cannot be subdivided.
      if (triangle.square_max_edge_size() < min_triangle_size) continue;

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

    float perc = (double)count / (double)d->vcount;

    if (iteration == 1 && perc < 0.90)
    {
      last_error = "Internal bug. The first iteration is expected to insert more than 90% of the point. This is a known bug. Please report";
      return false;
    }

    if (verbose) print("  Iteration %d: adding %d points (+%.1f%%) to the ground took %.2f secs\n", iteration, count, perc * 100, prof2.elapsed());

    if (perc < 1.0f / 1000.0f) break; // We actually stop the loop when < 0.1% additions

  } while (count > 0 && iteration < max_iter);


  if (verbose)
  {
    print("  Densification took %.2f secs (tri search %.2f secs)\n", prof.elapsed(), total_search_time.count());
    print(" Number of point discared based on DTM: %lu (%.1f%%)\n", discared, (float)((double)discared/(double)kk*100));
    print(" PDT took %.2f secs\n", tot.elapsed());
  }

  progress->done();

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

      if (v_idx < 4) continue;                    // 1. Skip Super Vertices (0, 1, 2, 3)
      if (v_idx < (4 + vbuf.size())) continue;    // 2. Skip Virtual Buffer points
      int map_idx = v_idx - 4 -  vbuf.size();     // 3. Calculate the correct index for index_map
      if (map_idx >= index_map.size()) continue;  // Safety check (optional)

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
  else if (id < (vbuf.size() + 4))
  {
    const auto& q = vbuf[id - 4];
    p.x = q.x;
    p.y = q.y;
    p.z = q.z;
  }
  else
  {
    Point tmp;
    tmp.set_schema(&las->header->schema);
    las->get_point(index_map[id - 4 - vbuf.size()], &tmp);
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

#include <random>

void LASRpdt::add_virtual_buffer(const std::vector<PointLAS>& xyz, double xmin, double ymin, double xmax, double ymax, double spacing)
{
  vbuf.clear();

  std::mt19937 generator(std::random_device{}());
  double noise_magnitude = 1;
  std::uniform_real_distribution<double> distribution(-noise_magnitude / 2.0, noise_magnitude / 2.0);

  xmax += 30;
  ymax += 30;
  xmin -= 30;
  ymin -= 30;

  double dx = xmax - xmin;
  double dy = ymax - ymin;

  int nx = std::round(dx / spacing);
  int ny = std::round(dy / spacing);

  double sx = dx / nx;
  double sy = dy / ny;

  vbuf.reserve((nx + 1) * (ny + 1));

  for (int i = 0; i <= nx; i++)
  {
    double x = xmin + i * sx;
    for (int j = 0; j <= ny; j++)
    {
      double x_noise = distribution(generator);
      double y_noise = distribution(generator);

      double y = ymin + j * sy;
      PointLAS p;
      p.x = x + x_noise;
      p.y = y + y_noise;
      p.z = 0.0;
      p.FID = 0;
      vbuf.push_back(std::move(p));
    }
  }

  using namespace nanoflann;
  PLAdaptor adaptor(xyz);
  typedef KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<double, PLAdaptor>,PLAdaptor,2> kd_tree_t;

  kd_tree_t index(2, adaptor, KDTreeSingleIndexAdaptorParams(10));
  index.buildIndex();

  for (auto& p : vbuf)
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

