#include "pdt.h"

#include "Shape.h"
#include "openmp.h"
#include "NA.h"

#include "lastransform.hpp"

LASRpdt::LASRpdt(double xmin, double ymin, double xmax, double ymax)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->npoints = 0;
  this->las = nullptr;

  vector.set_geometry_type(wkbMultiPolygon25D);
}

bool LASRpdt::process(LAS*& las)
{
  progress->reset();
  progress->set_prefix("Delaunay triangulation");
  progress->set_total(las->npoints);
  progress->show();

  const Point2Dd BT_P1(1e+10, 0);
  const Point2Dd BT_P2(0, 1e+10);
  const Point2Dd BT_P3(-1e+10, -1e+10);
  d.setBoundingTrianglePoints(BT_P1, BT_P2, BT_P3);

  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    if (!lasfilter.filter(p))
    {
      d.addPoint(Point2Dd((p->get_x()-xmin)*1000, (p->get_y()-ymin)*1000));
      index_map.push_back(las->current_point);
      npoints++;
    }

    (*progress)++;
    progress->show();
  }

  if (npoints < 3)
  {
    //last_error = "impossible to construct a Delaunay triangulation with " + std::to_string(coords.size()) + " points";
    return true;
  }

  this->las = las;

  progress->done();

  return true;
}

bool LASRpdt::write()
{
  if (ofile.empty()) return true;

  progress->set_total(d.triangles.size());
  progress->set_prefix("Write triangulation");
  progress->show();

  print("Triangle size %lu\n", d.triangles.size());

  auto start_time = std::chrono::high_resolution_clock::now();

  std::vector<TriangleXYZ> triangles;

  for (unsigned int i = 0 ; i < d.triangles.size(); i++)
  {
    if (d.triangles[i]->getIsDeleted() == true) continue;

    PointXYZ A(d.triangles[i]->getA()->x()/1000+xmin, d.triangles[i]->getA()->y()/1000+ymin);
    PointXYZ B(d.triangles[i]->getB()->x()/1000+xmin, d.triangles[i]->getB()->y()/1000+ymin);
    PointXYZ C(d.triangles[i]->getC()->x()/1000+xmin, d.triangles[i]->getC()->y()/1000+ymin);
    TriangleXYZ triangle(A, B, C);
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
  d.cleanDelaunayTriangulation();
}
