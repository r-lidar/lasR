#include "pdt.h"
#include "hporro/delaunay.h"
#include "Profiler.h"

#include <algorithm>

LASRpdt::LASRpdt()
{
  this->las = nullptr;
  this->d = nullptr;

  this->seed_resolution_search = 50;
  this->max_iteration_angle = 15;
  this->max_terrain_angle = 75;
  this->max_iteration_distance = 1;
  this->min_triangle_size = 0.1;

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

  prof.toc();
  print("Selecting seeds took %.2f s\n", prof.elapsed());

  // ============================
  // Triangulate the seed points
  // ============================

  prof.tic();

  d = new Triangulation(pts);

  prof.toc();
  print("Triangulating seeds took %.2f s\n", prof.elapsed());

  // =============================
  // Progressive TIN densification
  // =============================

  print("\nProgressive TIN densitifcation\n");

  prof.tic();

  std::chrono::duration<double> total_search_time(0);

  int count;

  do
  {
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

      // This is the triangle in which we are suppose to insert the point
      TriangleXYZ triangle(A,B,C);
      triangle.make_clock_wise();

      // If the triangle is < 1 cm, stop subdividing. This is computationally demanding for no
      // significant improvement
      if (triangle.square_max_edge_size() < min_triangle_size) continue;

      // Compute the normal
      PointXYZ n = triangle.normal();

      // Angle of the triangle in degrees
      double triangle_angle = std::acos(n.z) * (180.0 / M_PI);

      // Distance from P to the triangle plane
      // (figure 3 in Axelsson's paper)
      double dist_d = triangle.distance(P);

      // Project P onto the triangle plane
      PointXYZ proj_P(P.x - (dist_d * n.x), P.y - (dist_d * n.y), P.z - (dist_d * n.z));

      // Compute the distances from P to the three vertices of the triangle
      double dist_P0 = P.distance(triangle.A);
      double dist_P1 = P.distance(triangle.B);
      double dist_P2 = P.distance(triangle.C);

      // Convert the angles to radians
      // (figure 3 in Axelsson's paper)
      double alpha = asin(dist_d / dist_P0) * 180.0f/M_PI;
      double beta  = asin(dist_d / dist_P1) * 180.0f/M_PI;
      double gamma = asin(dist_d / dist_P2) * 180.0f/M_PI;

      double angle = MAX3(alpha, beta, gamma);

      // Check if the angles and distance meet the threshold criteria
      if (angle < max_iteration_angle && dist_d < max_iteration_distance)
      {
        d->delaunayInsertion(Vec2((P.x-xmin)*1000, (P.y-ymin)*1000), tri_index);
        index_map.push_back(las->current_point);
        count++;
      }
    }

    print("Added %d points in the triangulation\n", count);

  } while(count > 0);

  prof.toc();
  print("Densification took %.2f s\n", prof.elapsed());

  printf("Triangle search took: %.2f seconds\n", total_search_time.count());

  progress->done();

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
