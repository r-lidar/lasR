#include "pdt.h"
#include "hporro/delaunay.h"
#include "Profiler.h"
#include "Raster.h"

#include <algorithm>

LASRpdt::LASRpdt(double distance, double angle, double res, double min_size, int classification)
{
  this->las = nullptr;
  this->d = nullptr;

  this->seed_resolution_search = res;
  this->max_iteration_angle = angle;
  this->max_terrain_angle = 75;
  this->max_iteration_distance = distance;
  this->min_triangle_size = min_size*min_size;
  this->classification = classification;

  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRpdt::process(LAS*& las)
{
  progress->reset();
  progress->set_prefix("Progressive TIN densification");
  progress->set_total(las->npoints);
  progress->show();

  this->las = las;

  double xmin = las->header->min_x;
  double ymin = las->header->min_y;

  Raster dtm(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, 1);

  // =====================
  // Find some seed points
  // =====================

  Profiler prof;
  prof.tic();

  // Keep the lowest points per 50 m grid cells

  Grid grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, seed_resolution_search);
  std::vector<PointLAS> seeds;
  seeds.resize(grid.get_ncells());
  for (auto& seed : seeds) seed.z = F64_MAX;

  while (las->read_point())
  {
    if (lasfilter.filter(&las->point)) continue;

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

  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const PointXYZ& p) { return p.z == F64_MAX; });
  seeds.erase(new_end, seeds.end());

  std::vector<Vec2> pts;
  pts.reserve(seeds.size());
  index_map.clear();
  for (const auto& seed : seeds)
  {
    pts.push_back({(seed.x-xmin)*1000, (seed.y-ymin)*1000});
    index_map.push_back(seed.FID);
  }

  // ============================
  // Triangulate the seed points
  // ============================

  d = new Triangulation(pts);

  prof.toc();
  print("Triangulating seeds took %.2f secs\n", prof.elapsed());

  // =============================
  // Progressive TIN densification
  // =============================

  print("Progressive TIN densification\n");

  prof.tic();

  std::chrono::duration<double> total_search_time(0);

  int count;
  int iteration = 0;
  do
  {

    Profiler prof2;
    prof2.tic();

    iteration++;
    count = 0;

    // Placeholder for special bbox points
    double z_placeholder = las->header->min_z;

    LASpoint* p;
    int tri_index = -1;
    while (las->read_point())
    {
      p = &las->point;

      (*progress)++;
      progress->show();

      if (lasfilter.filter(&las->point)) continue;

      // Coordinates of the current point
      PointXYZ P(p->get_x(),p->get_y(),p->get_z());
      Vec2 pt((P.x-xmin)*1000, (P.y-ymin)*1000);

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
      auto query_coordinates = [&](int id, PointLAS& p)
      {
        if (id < 4)
        {
          p.x = d->vertices[id].pos[0];
          p.y = d->vertices[id].pos[1];
          p.z = z_placeholder;
        }
        else
        {
          las->get_point(index_map[id-4], p, nullptr, nullptr);
        }
      };

      PointLAS A,B,C;
      query_coordinates(idA, A);
      query_coordinates(idB, B);
      query_coordinates(idC, C);

      // This is the triangle in which we are supposed to insert the point
      TriangleXYZ triangle(A,B,C);
      //triangle.make_clock_wise();

      // If the triangle is < 10 cm, stop subdividing. This is computationally demanding for no
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
        if (d->delaunayInsertion(Vec2((P.x-xmin)*1000, (P.y-ymin)*1000), tri_index))
        {
          index_map.push_back(las->current_point);
          count++;
        }
      }
    }

    prof2.toc();

    float perc = (double)count/(double)d->vcount;
    print("Iteration %d: adding %d points (+%.4f\%) to the ground took %.2f secs\n", iteration, count, perc, prof2.elapsed());

    if (perc < 1.0f/1000.0f) break;

  } while(count > 0);

  prof.toc();
  print("Densification took %.2f secs\n", prof.elapsed());
  print("Triangle search took: %.2f secs\n", total_search_time.count());

  progress->done();

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  while (las->read_point())
  {
    if (las->point.get_classification() == classification)
    {
      las->point.set_classification(1);
      las->update_point();
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
    las->point.set_classification(classification);
    las->update_point();

    las->seek(index_map[idB]);
    las->point.set_classification(classification);
    las->update_point();

    las->seek(index_map[idC]);
    las->point.set_classification(classification);
    las->update_point();
  }

  return true;
}

bool LASRpdt::write()
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
    las->get_point(index_map[idA], A, nullptr, nullptr);
    las->get_point(index_map[idB], B, nullptr, nullptr);
    las->get_point(index_map[idC], C, nullptr, nullptr);

    Vec2 a = d->vertices[d->triangles[i].v[0]].pos;
    Vec2 b = d->vertices[d->triangles[i].v[1]].pos;
    Vec2 c = d->vertices[d->triangles[i].v[2]].pos;

    //print("A (%.1lf, %.1lf, %.1lf) vs  (%.1lf, %.1lf)\n", A.x, A.y, A.z, a[0]/1000+xmin, a[1]/1000+ymin);
    //print("B (%.1lf, %.1lf, %.1lf) vs  (%.1lf, %.1lf)\n", B.x, B.y, B.z, b[0]/1000+xmin, b[1]/1000+ymin);
    //print("C (%.1lf, %.1lf, %.1lf) vs  (%.1lf, %.1lf)\n", C.x, C.y, C.z, c[0]/1000+xmin, c[1]/1000+ymin);


    //if (A.x != a[0]/1000+xmin || A.y != a[1]/1000+ymin ||  B.x != b[0]/1000+xmin || B.y != b[1]/1000+ymin ||  C.x != c[0]/1000+xmin || C.y != c[1]/1000+ymin)
    //  print(">>>>>>> %lu\n", i);

    TriangleXYZ triangle(A, B, C);

    /*Vec2 a = d->vertices[d->triangles[i].v[0]].pos;
     PointXYZ A(a[0]/1000+xmin, a[1]/1000+ymin);

     Vec2 b = d->vertices[d->triangles[i].v[1]].pos;
     PointXYZ B(b[0]/1000+xmin, b[1]/1000+ymin);

     Vec2 c = d->vertices[d->triangles[i].v[2]].pos;
     PointXYZ C(c[0]/1000+xmin, c[1]/1000+ymin);

     TriangleXYZ triangle(A, B, C);*/
    triangle.make_clock_wise();
    triangles.push_back(triangle);

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  progress->done();

#pragma omp critical (write_triangulation)
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
}

void LASRpdt::clear(bool last)
{
  delete d;
  d = nullptr;
  index_map.clear();
}

// See LASRtriangulate::interpolate
void LASRpdt::compute_dtm(const Triangulation* d, Raster* r) const
{
  // Generate the raster points in this triangle
  double xres = r->get_xres();
  double yres = r->get_yres();

  for (unsigned int i = 0 ; i < d->tcount; i++)
  {
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    PointLAS A,B,C;
    las->get_point(index_map[idA], A, nullptr, nullptr);
    las->get_point(index_map[idB], B, nullptr, nullptr);
    las->get_point(index_map[idC], C, nullptr, nullptr);
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
}
