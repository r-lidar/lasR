#include "triangulate.h"

#include "Progress.hpp"
#include "Raster.h"
#include "Shape.h"
#include "openmp.h"
#include "NA.h"

#include "lastransform.hpp"

#include "delaunator/delaunator.hpp"

#include <chrono>

LASRtriangulate::LASRtriangulate(double xmin, double ymin, double xmax, double ymax, double trim, std::string use_attribute)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->keep_large = trim < 0;
  this->trim = trim*trim;
  this->npoints = 0;
  this->las = 0;
  this->d = nullptr;

  this->use_attribute = use_attribute;

  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRtriangulate::process(LAS*& las)
{
  auto start_time = std::chrono::high_resolution_clock::now();

  LAStransform* lastransform = nullptr;
  if (use_attribute != "Z")
  {
    lastransform = las->make_z_transformer(use_attribute);
    if (lastransform == nullptr)
    {
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "no extrabyte attribute '%s' found", use_attribute.c_str());
      last_error = std::string(buffer);
      return false;
    }
  }

  progress->reset();
  progress->set_prefix("Delaunay triangulation");
  progress->set_total(0);
  progress->show();

  std::vector<double> coords;

  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    if (lastransform) lastransform->transform(p);
    if (!lasfilter.filter(p))
    {
      coords.push_back(p->get_x());
      coords.push_back(p->get_y());
      index_map.push_back(las->current_point);
      npoints++;
    }
  }

  this->las = las;
  d = new delaunator::Delaunator(coords);

  progress->done();

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Construction of triangulation took %.2f sec for %lu triangles\n", second, d->triangles.size()/3);
    // # nocov end
  }

  return true;
}

bool LASRtriangulate::interpolate(std::vector<double>& res, const Raster* raster)
{
  if (las == 0) return false;

  LAStransform* lastransform = nullptr;
  if (use_attribute != "Z")
  {
    lastransform = las->make_z_transformer(use_attribute);
    if (lastransform == nullptr)
    {
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "no extrabyte attribute '%s' found", use_attribute.c_str());
      last_error = std::string(buffer);
      return false;
    }
  }

  progress->reset();
  progress->set_total(d->triangles.size()/3);
  progress->set_prefix("Interpolation");

  auto start_time = std::chrono::high_resolution_clock::now();

  if (raster == 0)
    res.resize(las->npoints);
  else
    res.resize(raster->get_ncells());

  std::fill(res.begin(), res.end(), NA_F64);

  // 1. loop through the triangles, search the point inside triangle, interpolate
  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->triangles.size() ; i+=3)
  {
    int id;
    PointLAS A,B,C;

    id = index_map[d->triangles[i]];
    las->get_point(id, A, nullptr, lastransform);

    id = index_map[d->triangles[i+1]];
    las->get_point(id, B, nullptr, lastransform);

    id = index_map[d->triangles[i+2]];
    las->get_point(id, C, nullptr, lastransform);

    TriangleXYZ triangle(A, B, C);

    // Interpolate in this triangle if the longest edge fulfill requirements
    bool keep_triangle = (keep_large) ? triangle.square_max_edge_size() > trim : triangle.square_max_edge_size() < trim;
    if (trim == 0 || keep_triangle)
    {
      if (raster)
      {
        // Generate the raster points in this triangle
        double xres = raster->get_xres();
        double yres = raster->get_yres();
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

            int cell = raster->cell_from_xy(x,y);
            if (cell != -1) res[cell] = p.z;
          }
        }
      }
      else
      {
        std::vector<PointLAS> points;
        las->query(&triangle, points, nullptr, lastransform);

        for (auto& p : points)
        {
          triangle.linear_interpolation(p);
          res[p.FID] = p.z;
        }
      }
    }

    #pragma omp critical
    {
      (*progress)++;
    }
    if (omp_get_thread_num() == 0)
    {
      progress->show();
    }
  }

  progress->done();

  if (lastransform) delete lastransform;

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Interpolation of Delaunay triangulation took %.2g sec\n", second);
    // # nocov end
  }

  return true;
}

bool LASRtriangulate::contour(std::vector<Edge>& e) const
{
  std::unordered_set<Edge> edges;

  auto start_time = std::chrono::high_resolution_clock::now();

  progress->reset();
  progress->set_prefix("Delaunay contours");
  progress->set_total(d->triangles.size()/3);

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->triangles.size() ; i+=3)
  {
    int id;
    double xyz[3];

    id = index_map[d->triangles[i]];
    las->get_xyz(id, xyz);
    PointXYZ A({xyz[0], xyz[1], xyz[2]});

    id = index_map[d->triangles[i+1]];
    las->get_xyz(id, xyz);
    PointXYZ B({xyz[0], xyz[1], xyz[2]});

    id = index_map[d->triangles[i+2]];
    las->get_xyz(id, xyz);
    PointXYZ C({xyz[0], xyz[1], xyz[2]});

    TriangleXYZ triangle(A, B, C);

    bool keep_triangle = (keep_large) ? triangle.square_max_edge_size() > trim : triangle.square_max_edge_size() < trim;
    if (trim == 0 || keep_triangle)
    {
      triangle.make_clock_wise();

      Edge AB = {triangle.A, triangle.B};
      Edge BC = {triangle.B, triangle.C};
      Edge CA = {triangle.C, triangle.A};

      #pragma omp critical
      {
        if (edges.count(AB) > 0) edges.erase(AB); else edges.insert(AB);
        if (edges.count(BC) > 0) edges.erase(BC); else edges.insert(BC);
        if (edges.count(CA) > 0) edges.erase(CA); else edges.insert(CA);
      }
    }

    #pragma omp critical
    {
      (*progress)++;
    }
    if (omp_get_thread_num() == 0)
    {
      progress->show();
    }
  }

  for (const auto& elmt : edges) e.push_back(elmt);

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Delaunay contour took %.2f sec\n", second);
    // # nocov end
  }

  return true;
}

bool LASRtriangulate::write()
{
  if (ofile.empty()) return true;

  LAStransform* lastransform = nullptr;
  if (use_attribute != "Z")
  {
    lastransform = las->make_z_transformer(use_attribute);
    if (lastransform == nullptr)
    {
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "no extrabyte attribute '%s' found", use_attribute.c_str());
      last_error = std::string(buffer);
      return false;
    }
  }

  progress->set_total(d->triangles.size()/3);
  progress->set_prefix("Write triangulation");

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<TriangleXYZ> triangles;

  for (unsigned int i = 0 ; i < d->triangles.size(); i+=3)
  {
    int id;
    PointLAS A,B,C;

    id = index_map[d->triangles[i]];
    las->get_point(id, A, nullptr, lastransform);

    id = index_map[d->triangles[i+1]];
    las->get_point(id, B, nullptr, lastransform);

    id = index_map[d->triangles[i+2]];
    las->get_point(id, C, nullptr, lastransform);

    TriangleXYZ triangle(A, B, C);

    bool keep_triangle = (keep_large) ? triangle.square_max_edge_size() > trim : triangle.square_max_edge_size() < trim;
    if (trim == 0 || keep_triangle)
    {
      triangle.make_clock_wise();
      triangles.push_back(triangle);
    }

    (*progress)++;
    progress->show();
  }

  progress->done();

  if (lastransform) delete lastransform;

  vector.write_triangulation(triangles);

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

void LASRtriangulate::clear(bool last)
{
  coords.clear();
  index_map.clear();
  delete d;
  d = nullptr;
  npoints = 0;
}
