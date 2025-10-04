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
  max_iteration_distance = stage.value("distance", 1);
  max_iteration_angle = stage.value("angle", 15);
  seed_resolution_search = stage.value("res", 50);
  double min_size = stage.value("min_size", 0.1);
  min_triangle_size = min_size*min_size;
  offset = stage.value("offset", 0.05);
  classification = stage.value("class", 2);

  return true;
}

bool LASRpdt::process(PointCloud*& las)
{
  this->las = las;

  double xmin = las->header->min_x;
  double ymin = las->header->min_y;
  double zmin = las->header->min_z;
  double inf  = std::numeric_limits<double>::max();

  Raster dtm(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, 1);

  // Use to retain if a point is already part of the triangulation
  std::vector<bool> inserted(las->npoints, false);

  Profiler prof;
  prof.tic();

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
  for (auto& seed : seeds) seed.z = inf;

  int counter = 0;
  while (las->read_point())
  {
    counter++;

    if (pointfilter.filter(&las->point)) continue;

    double x = las->point.get_x();
    double y = las->point.get_y();
    double z = las->point.get_z();
    unsigned int id = las->current_point;
    int cell = grid.cell_from_xy(x,y);

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

  // We created 1 seed per grid cell. Some cell maybe empty. In these cells z = INF
  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const PointXYZ& p) { return p.z == std::numeric_limits<double>::max(); });
  seeds.erase(new_end, seeds.end());

  // Sort descending to insert higher seeds first in order to reduce the chance to insert a low outlier first
  std::sort(seeds.begin(), seeds.end(), [](const PointXYZ& a, const PointXYZ& b) { return a.z > b.z; });

  double zmean = 0.0f;
  for (const auto& point : seeds) zmean += point.z;
  zmean /= (double)seeds.size();

  // ============================
  // Triangulate the seed points
  // ============================

  // -------------------------------------------------------
  // Instead of triangulating all the seeds as described in the original paper,
  // we are applying the progressive densification at this first step in order
  // to get rid of low outliers.
  // ---------------------------------------------------------

  // By default the triangulation has 4 points defining a bbox at +/- 1e8
  d = new Triangulation();

  int count = 0;
  int iteration = 0;
  int tri_index = -1;

  for (auto& seed : seeds)
  {
    // Reoffset to fit in the bounding box of the triangulation which is +/- 1e8
    Vec2 pt(seed.x-xmin, seed.y-ymin);

    // Find in which triangle to insert the point
    tri_index = d->findContainerTriangle(pt, tri_index);

    // The point has already been inserted, it is a point of the triangulation
    if (tri_index < 0) continue;

    // Find the index of the vertices
    int idA = d->triangles[tri_index].v[0];
    int idB = d->triangles[tri_index].v[1];
    int idC = d->triangles[tri_index].v[2];

    // The index may map one of the 4 additional points that are not actually in the point cloud and
    // that serve as bounding box. Otherwise retrieve the coordinates in the point cloud.
    auto query_coordinates = [&](int id, PointXYZ& p)
    {
      if (id < 4)
      {
        p.x = d->vertices[id].pos.x + xmin;
        p.y = d->vertices[id].pos.y + ymin;
        p.z = zmean;
      }
      else
      {
        Point tmp;
        tmp.set_schema(&las->header->schema);
        las->get_point(index_map[id-4], &tmp);
        p.x = tmp.get_x();
        p.y = tmp.get_y();
        p.z = tmp.get_z();
      }
    };

    PointXYZ A,B,C;
    query_coordinates(idA, A);
    query_coordinates(idB, B);
    query_coordinates(idC, C);

    // This is the triangle in which we are supposed to insert the point
    TriangleXYZ triangle(A,B,C);

    // If the triangle is < x cm, stop subdividing. This is computationally demanding for no
    // significant improvement
    if (triangle.square_max_edge_size() < min_triangle_size) continue;

    // Compute the normal
    PointXYZ n = triangle.normal();

    // Angle of the triangle in degrees
    double triangle_angle = std::acos(n.z) * (180.0 / M_PI);

    // Calculate the perpendicular projection of seed onto the triangle
    // and the distance d from seed to the Projection (figure 3 in Axelsson's paper)
    PointXYZ v = seed - A;
    double dist_d = v.dot(n);
    PointXYZ P_proj = seed - n * dist_d;
    dist_d = std::abs(dist_d);

    if (!triangle.contains(P_proj)) continue;

    // Calculate the distances from seed to the three vertices of the triangle
    double dist_P0 = seed.distance(triangle.A);
    double dist_P1 = seed.distance(triangle.B);
    double dist_P2 = seed.distance(triangle.C);

    // Convert the angles to radians
    // (figure 3 in Axelsson's paper)
    double alpha = asin(dist_d / dist_P0) * 180.0f/M_PI;
    double beta  = asin(dist_d / dist_P1) * 180.0f/M_PI;
    double gamma = asin(dist_d / dist_P2) * 180.0f/M_PI;
    alpha = std::abs(alpha);
    beta  = std::abs(beta);
    gamma = std::abs(gamma);

    double angle = MAX3(alpha, beta, gamma);

    // Check if the angles and distance meet the threshold criteria
    if (d->delaunayInsertion(pt, tri_index))
    {
      index_map.push_back(seed.FID);
      inserted[seed.FID] = true;
      count++;
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

  do
  {
    progress->reset();
    progress->set_prefix("PDT");
    progress->set_total(las->npoints);
    progress->show();

    Profiler prof2;
    prof2.tic();

    iteration++;
    count = 0;

    Point* p;
    tri_index = -1;
    while (las->read_point())
    {
      p = &las->point;

      (*progress)++;
      progress->show();

      if (pointfilter.filter(&las->point)) continue;

      if (inserted[las->current_point]) continue;

      // Coordinates of the current point
      PointXYZ P(p->get_x(), p->get_y(), p->get_z());
      Vec2 pt(P.x-xmin, P.y-ymin);

      // We have a rough dtm. Check the DTM elevation. If we are significantly
      // higher than the DTM we can save computation time by skipping the search
      // in the triangulation. Anyway this point won't be added.
      double zdtm = dtm.get_value(P.x, P.y);
      if (zdtm != dtm.get_nodata() && std::abs(zdtm - P.z) > 3*max_iteration_distance)
        continue;

      // Find the index of the triangle in which this point lies
      auto start_time = std::chrono::high_resolution_clock::now();
      tri_index = d->findContainerTriangle(pt, tri_index);
      auto end_time = std::chrono::high_resolution_clock::now();
      total_search_time += end_time - start_time;

      // The point has already been inserted, it is a point of the triangulation
      if (tri_index < 0) continue;

      // Find the index of the vertices
      int idA = d->triangles[tri_index].v[0];
      int idB = d->triangles[tri_index].v[1];
      int idC = d->triangles[tri_index].v[2];

      // The index may map one of the 4 additional points that are not actually in the point cloud and
      // that serve as bounding box. Otherwise retrieve the coordinates in the point cloud.
      auto query_coordinates = [&](int id, PointXYZ& p)
      {
        if (id < 4)
        {
          p.x = d->vertices[id].pos.x + xmin;
          p.y = d->vertices[id].pos.y + ymin;
          p.z = zmean;
        }
        else
        {
          Point tmp;
          tmp.set_schema(&las->header->schema);
          las->get_point(index_map[id-4], &tmp);
          p.x = tmp.get_x();
          p.y = tmp.get_y();
          p.z = tmp.get_z();
        }
      };

      PointLAS A,B,C;
      query_coordinates(idA, A);
      query_coordinates(idB, B);
      query_coordinates(idC, C);

      // This is the triangle in which we are supposed to insert the point
      TriangleXYZ triangle(A,B,C);
      //triangle.make_clock_wise();

      // If the triangle is < x cm, stop subdividing. This is computationally demanding for no
      // significant improvement
      if (triangle.square_max_edge_size() < min_triangle_size) continue;

      // Compute the normal
      PointXYZ n = triangle.normal();

      // Angle of the triangle in degrees
      double triangle_angle = std::acos(n.z) * (180.0 / M_PI);

      // Calculate the perpendicular projection of P onto the triangle
      // and th distance d from P to the Projection (figure 3 in Axelsson's paper)
      PointXYZ v = P - A;
      double dist_d = v.dot(n);
      PointXYZ P_proj = P - n * dist_d;
      dist_d = std::abs(dist_d);

      if (!triangle.contains(P_proj)) continue;

      // Calculate the distances from P to the three vertices of the triangle
      double dist_P0 = P.distance(triangle.A);
      double dist_P1 = P.distance(triangle.B);
      double dist_P2 = P.distance(triangle.C);

      // Convert the angles to radians
      // (figure 3 in Axelsson's paper)
      double alpha = asin(dist_d / dist_P0) * 180.0f/M_PI;
      double beta  = asin(dist_d / dist_P1) * 180.0f/M_PI;
      double gamma = asin(dist_d / dist_P2) * 180.0f/M_PI;
      alpha = std::abs(alpha);
      beta = std::abs(beta);
      gamma = std::abs(gamma);

      double angle = MAX3(alpha, beta, gamma);

      // Check if the angles and distance meet the threshold criteria
      if (angle < max_iteration_angle && dist_d < max_iteration_distance)
      {
        if (d->delaunayInsertion(Vec2(P.x-xmin, P.y-ymin), tri_index))
        {
          index_map.push_back(las->current_point);
          inserted[las->current_point] = true;
          count++;
        }
      }

      progress->update(las->current_point);
      progress->show();
    }

    prof2.toc();

    float perc = (double)count/(double)d->vcount;
    if (verbose) print("  Iteration %d: adding %d points (+%.1f\%) to the ground took %.2f secs\n", iteration, count, perc*100, prof2.elapsed());

    if (perc < 1.0f/1000.0f) break;

    interpolate(&dtm);

  } while(count > 0);

  prof.toc();
  if (verbose) print("Densification took %.2f secs (tri search %.2f secs)\n", prof.elapsed(),  total_search_time.count());

  progress->done();

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  AttributeAccessor get_and_set_classification("Classification");

  while (las->read_point())
  {
    if (get_and_set_classification(&las->point) == classification)
    {
      get_and_set_classification(&las->point, 1);
    }
  }

  // =====================================
  // Classify ground points
  // =====================================

  for (unsigned int i = 0 ; i < d->tcount; i++)
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

  if (offset == 0) return true;

  // =============================================
  // Classify ground points with buffer above mesh
  // =============================================

  dtm = Raster(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, 0.25);
  interpolate(&dtm);

  while (las->read_point())
  {
    double zdtm = dtm.get_value(las->point.get_x(), las->point.get_y());
    if (zdtm != dtm.get_nodata() && std::abs(zdtm - las->point.get_z()) <= offset)
    {
      get_and_set_classification(&las->point, classification);
    }
  }

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

