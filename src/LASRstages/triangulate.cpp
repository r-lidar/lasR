#include "triangulate.h"
#include "Raster.h"
#include "Shape.h"
#include "openmp.h"
#include "NA.h"

#include "lastransform.hpp"

#include "delaunator/delaunator.hpp"

LASRtriangulate::LASRtriangulate()
{
  npoints = 0;
  las = nullptr;
  d = nullptr;
  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRtriangulate::set_parameters(const nlohmann::json& stage)
{
  double max_edge = stage.at("max_edge");
  use_attribute = stage.at("use_attribute");

  keep_large = max_edge < 0;
  trim = max_edge*max_edge;

  return true;
}

bool LASRtriangulate::process(LAS*& las)
{
  this->las = las;

  progress->reset();
  progress->set_prefix("Delaunay triangulation");
  progress->set_total(0);
  progress->show();

  std::vector<double> coords;

  Point* p;
  while (las->read_point())
  {
    p = &las->p;
    if (pointfilter.filter(p)) continue;
    coords.push_back(p->get_x());
    coords.push_back(p->get_y());
    index_map.push_back(las->current_point);
    npoints++;
  }

  // Does not fail because contour can work with d == nullptr
  // However 'interpolate' will handle the case and fail
  if (coords.size() < 3) return true;

  d = new delaunator::Delaunator(coords);

  progress->done();

  return true;
}

bool LASRtriangulate::interpolate(std::vector<double>& res, const Raster* raster)
{
  AttributeReader accessor(use_attribute);

  int n = (raster == nullptr) ? las->npoints : raster->get_ncells();
  res.resize(n);
  std::fill(res.begin(), res.end(), NA_F64);

  if (d == nullptr) return true;
  if (res.size() == 0) return true; // Fix #40

  progress->reset();
  progress->set_total(d->triangles.size()/3);
  progress->set_prefix("Interpolation");
  progress->set_ncpu(ncpu);
  progress->show();

  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  // 1. loop through the triangles, search the point inside triangle, interpolate
  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->triangles.size() ; i+=3)
  {
    if (progress->interrupted()) continue;

    int id;
    Point A,B,C;
    A.set_schema(&las->newheader->schema);
    B.set_schema(&las->newheader->schema);
    C.set_schema(&las->newheader->schema);

    id = index_map[d->triangles[i]];
    las->get_point(id, &A);

    id = index_map[d->triangles[i+1]];
    las->get_point(id, &B);

    id = index_map[d->triangles[i+2]];
    las->get_point(id, &C);

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
        std::vector<Point> points;
        las->query(&triangle, points);

        for (auto& pt : points)
        {
          PointXYZ p(pt.get_x(), pt.get_y());
          triangle.linear_interpolation(p);
          int index = las->get_index(&pt);
          res[index] = p.z;
        }
      }
    }

    if (main_thread)
    {
      // can only be called in outer thread 0 AND is internally thread safe being called only in outer thread 0
      #pragma omp critical
      {
        (*progress)++;
        progress->show();
      }
    }
  }

  progress->done();

  return true;
}

bool LASRtriangulate::contour(std::vector<Edge>& e) const
{
  // d is nullptr if the triangulation was not computed because we do not have enough points.
  // In this case 'contour' should not fail
  if (d == nullptr) return true; // # nocov

  std::unordered_set<Edge> edges;

  progress->reset();
  progress->set_prefix("Delaunay contours");
  progress->set_total(d->triangles.size()/3);
  progress->set_ncpu(ncpu);
  progress->show();

  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->triangles.size() ; i+=3)
  {
    if (progress->interrupted()) continue;

    int id;
    Point a, b, c;
    a.set_schema(&las->newheader->schema);
    b.set_schema(&las->newheader->schema);
    c.set_schema(&las->newheader->schema);

    id = index_map[d->triangles[i]];
    las->get_point(id, &a);

    id = index_map[d->triangles[i+1]];
    las->get_point(id, &b);

    id = index_map[d->triangles[i+2]];
    las->get_point(id, &c);

    TriangleXYZ triangle(a, b, c);

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

    if (main_thread)
    {
      #pragma omp critical
      {
        // can only be called in outer thread 0 AND is internally thread safe being called only in outer thread 0
        (*progress)++;
        progress->show();
      }
    }
  }

  for (const auto& elmt : edges) e.push_back(elmt);

  return true;
}

bool LASRtriangulate::write()
{
  if (ofile.empty()) return true;

  AttributeReader accessor(use_attribute);

  progress->set_total(d->triangles.size()/3);
  progress->set_prefix("Write triangulation");
  progress->show();

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<TriangleXYZ> triangles;

  for (unsigned int i = 0 ; i < d->triangles.size(); i+=3)
  {
    int id;
    Point A,B,C;
    A.set_schema(&las->newheader->schema);
    B.set_schema(&las->newheader->schema);
    C.set_schema(&las->newheader->schema);

    id = index_map[d->triangles[i]];
    las->get_point(id, &A);

    id = index_map[d->triangles[i+1]];
    las->get_point(id, &B);

    id = index_map[d->triangles[i+2]];
    las->get_point(id, &C);

    TriangleXYZ triangle(A, B, C);

    bool keep_triangle = (keep_large) ? triangle.square_max_edge_size() > trim : triangle.square_max_edge_size() < trim;
    if (trim == 0 || keep_triangle)
    {
      triangle.make_clock_wise();
      triangles.push_back(triangle);
    }

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

void LASRtriangulate::clear(bool last)
{
  coords.clear();
  index_map.clear();
  delete d;
  d = nullptr;
  npoints = 0;
}
