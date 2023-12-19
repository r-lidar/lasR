#include "triangulate.h"

#include "Progress.hpp"
#include "Raster.h"
#include "Shape.h"
#include "openmp.h"
#include "NA.h"

#include "lastransform.hpp"

#include <chrono>

LASRtriangulate::LASRtriangulate(double xmin, double ymin, double xmax, double ymax, double trim, std::string use_attribute)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->trim = trim*trim;
  this->npoints = 0;
  this->las = 0;

  this->use_attribute = use_attribute;

  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRtriangulate::process(LASpoint*& p)
{
  if (!lasfilter.filter(p))
  {
    vb.insert_point(p->get_X(), p->get_Y());
    index_map.push_back(npoints);
  }
  npoints++;

  return true;
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

  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    if (lastransform) lastransform->transform(p);
    process(p);
  }

  this->las = las;
  vb.construct(&vd);

  progress->done();

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Construction of voronoi diagram took %.2f sec for %llu vertices\n", second, vd.num_vertices());
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
  progress->set_total(vd.num_vertices());
  progress->set_prefix("Interpolation");

  auto start_time = std::chrono::high_resolution_clock::now();

  if (raster == 0)
    res.resize(las->npoints);
  else
    res.resize(raster->get_ncells());

  std::fill(res.begin(), res.end(), NA_F64);

  // 1. loop through the triangles, search the point inside triangle, interpolate
  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < vd.num_vertices() ; ++i)
  {
    std::vector<PointXYZ> nodes;
    const voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin() + i;
    const voronoi_diagram<double>::vertex_type& vertex = *it;
    const voronoi_diagram<double>::edge_type* edge = vertex.incident_edge();

    do
    {
      unsigned int id = index_map[edge->cell()->source_index()];
      PointLAS p;
      las->get_point(id, p, nullptr, lastransform);
      nodes.push_back(p);

      if (nodes.size() == 3)
      {
        TriangleXYZ triangle(nodes[0], nodes[1], nodes[2]);

        // Interpolate in this triangle if the longest edge fulfill requirements
        if (trim == 0 || triangle.square_max_edge_size() < trim)
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

        nodes.erase(nodes.begin() + 1);
      }

      edge = edge->rot_next();

    } while (edge != vertex.incident_edge());

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
  progress->set_total(vd.num_vertices());

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < vd.num_vertices() ; ++i)
  {
    std::vector<PointXYZ> nodes;
    const voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin() + i;
    const voronoi_diagram<double>::vertex_type& vertex = *it;
    const voronoi_diagram<double>::edge_type* edge = vertex.incident_edge();

    do
    {
      unsigned int id = index_map[edge->cell()->source_index()];
      double xyz[3];
      las->get_xyz(id, xyz);
      nodes.push_back({xyz[0], xyz[1], xyz[2]});

      if (nodes.size() == 3)
      {
        TriangleXYZ triangle(nodes[0], nodes[1], nodes[2]);

        if (trim == 0 || triangle.square_max_edge_size() < trim)
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

        nodes.erase(nodes.begin() + 1);
      }

      edge = edge->rot_next();

    } while (edge != vertex.incident_edge());

    progress->update(i);
    progress->show();
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

  progress->set_total(vd.num_vertices());
  progress->set_prefix("Write triangulation");

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<TriangleXYZ> triangles;

  for (unsigned int i = 0 ; i < vd.num_vertices() ; ++i)
  {
    std::vector<PointXYZ> nodes;
    const voronoi_diagram<double>::const_vertex_iterator it = vd.vertices().begin() + i;
    const voronoi_diagram<double>::vertex_type& vertex = *it;
    const voronoi_diagram<double>::edge_type* edge = vertex.incident_edge();

    do
    {
      unsigned int id = index_map[edge->cell()->source_index()];
      PointLAS p;
      las->get_point(id, p, nullptr, lastransform);
      nodes.push_back(p);

      if (nodes.size() == 3)
      {
        TriangleXYZ triangle(nodes[0], nodes[1], nodes[2]);

        if (trim == 0 || triangle.square_max_edge_size() < trim)
        {
          triangle.make_clock_wise();
          triangles.push_back(triangle);
        }

        nodes.erase(nodes.begin() + 1);
      }

      edge = edge->rot_next();

    } while (edge != vertex.incident_edge());

    progress->update(i);
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
  vd.clear();
  vb.clear();
  index_map.clear();
  npoints = 0;
}
