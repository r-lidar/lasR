#include "pdt.h"
#include "Profiler.h"
#include "Raster.h"
#include "NA.h"

#include "hporro/delaunay.h"

#include <algorithm>
#include <cmath>

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
  this->las = las;
  this->x_offset = las->header->min_x;
  this->y_offset = las->header->min_y;
  this->z_offset = las->header->min_z;
  size_t n = las->npoints;

  // Used to discard candidate points quickly and save computationally demanding tests
  Raster dtm(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, 1);

  // Used to retain if a point is already part of the triangulation
  std::vector<bool> inserted(n, false);

  Profiler prof;
  prof.tic();

  // ========================================
  // Order in which point should be processed
  // =========================================

  if (verbose) print("Sorting points:\n");

  // Records Z to sort index by Z in order to loop from lowest to highest
  std::vector<float> height; height.reserve(n);
  std::vector<unsigned int> index(n);
  std::iota(index.begin(), index.end(), 0);
  while (las->read_point()) height.push_back(las->point.get_z());
  std::sort(index.begin(), index.end(), [&height](unsigned int i1, unsigned int i2) { return height[i1] < height[i2]; });
  height.clear();
  height.shrink_to_fit();

  // =====================
  // Find some seed points
  // =====================

  // ----------------------------------
  // Keeps the lowest points of
  // each cell of a grid (usually 50 m)
  // ----------------------------------

  if (verbose) print("Search seeds:\n");

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

  // Sort descending to insert higher seeds first in order to reduce the chance to insert a low outlier first
  std::sort(seeds.begin(), seeds.end(), [](const PointXYZ& a, const PointXYZ& b) { return a.z > b.z; });

  // In the triangulation the 4 first default points at infinity need a Z value. We will use mean z of seeds
  z_default = std::accumulate(seeds.begin(), seeds.end(), 0.0, [](double sum, const PointXYZ& p) { return sum + p.z; }) / seeds.size();

  // ============================
  // Triangulate the seed points
  // ============================

  // -------------------------------------------------------
  // Instead of triangulating all the seeds as described in the original paper,
  // we are applying the progressive densification at this first step in order
  // to get rid of low outliers.
  // ---------------------------------------------------------

  Grid gd(las->header->min_x-x_offset, las->header->min_y-y_offset, las->header->max_x-x_offset, las->header->max_y-y_offset, 1);
  d = new Triangulation(gd);

  int count = 0;
  int iteration = 0;
  int t = -1;
  unsigned int discared = 0;

  for (auto& seed : seeds)
  {
    // Offset to fit in the bounding box of the triangulation which is +/- 1e8
    // Also get extra accuracy
    Vec2 pt(seed.x - x_offset, seed.y - y_offset);

    // Find in which triangle to insert the point
    t = d->findContainerTriangleFast(pt);

    if (t < 0) continue;

    // Retrieve the vertices of the triangle
    TriangleXYZ triangle = get_triangle(t);

    // Skip too small triangles. Small triangle are freezed and cannot be subdivided.
    if (triangle.square_max_edge_size() < min_triangle_size) continue;

    double dist_d, angle;
    if (!axelsson_metrics(seed, triangle, dist_d, angle)) continue;

    // Check if the angles and distance meet the threshold criteria
    if (angle < max_iteration_angle)
    {
      if (d->delaunayInsertion(pt, t))
      {
        index_map.push_back(seed.FID);
        inserted[seed.FID] = true;
        count++;
      }
    }
  }

  prof.toc();
  if (verbose) print("  Iteration 0: adding %d seeds to the ground took %.2f secs\n", count, prof.elapsed());

  interpolate(&dtm);

  // =============================
  // Progressive TIN densification
  // =============================

  if (verbose) print("Progressive TIN densification:\n");

  prof.tic();
  std::chrono::duration<double> total_search_time(0);

  // We loop while we are adding new triangles
  do
  {
    if (max_iter == 0) break;

    progress->reset();
    progress->set_prefix("PDT");
    progress->set_total(las->npoints);
    progress->show();

    Profiler prof2;
    prof2.tic();

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

      PointXYZ P(x, y, z);
      Vec2 pt(x - x_offset, y - y_offset);

      // We have a rough dtm. Check the DTM elevation. If we are significantly
      // higher than the DTM we can save computation time by skipping the search
      // in the triangulation. Anyway this point won't be added.
      double zdtm = dtm.get_value(P.x, P.y);
      if (zdtm != dtm.get_nodata() && std::abs(zdtm - P.z) > 3 * max_iteration_distance)
      {
        discared++;
        continue;
      }

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

    prof2.toc();
    float perc = (double)count / (double)d->vcount;
    if (verbose) print("  Iteration %d: adding %d points (+%.1f%%) to the ground took %.2f secs\n", iteration, count, perc * 100, prof2.elapsed());

    if (perc < 1.0f / 1000.0f) break; // We actually stop the loop when < 0.1% additions

    interpolate(&dtm);

  } while (count > 0 && iteration < max_iter);

  prof.toc();
  if (verbose) print("Densification took %.2f secs (tri search %.2f secs)\n", prof.elapsed(), total_search_time.count());

  print("Number of point discared based on DTM: %lu (%.1f%%)\n", discared, (float)((double)discared/(double)n*100));

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
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    las->seek(index_map[idA]);
    get_and_set_classification(&las->point, classification);

    las->seek(index_map[idB]);
    get_and_set_classification(&las->point, classification);

    las->seek(index_map[idC]);
    get_and_set_classification(&las->point, classification);
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
  else
  {
    Point tmp;
    tmp.set_schema(&las->header->schema);
    las->get_point(index_map[id - 4], &tmp);
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
  double dist_P0 = P.distance(triangle.A);
  double dist_P1 = P.distance(triangle.B);
  double dist_P2 = P.distance(triangle.C);

  double alpha = std::abs(asin(dist_d / dist_P0) * 180.0 / M_PI);
  double beta  = std::abs(asin(dist_d / dist_P1) * 180.0 / M_PI);
  double gamma = std::abs(asin(dist_d / dist_P2) * 180.0 / M_PI);

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

