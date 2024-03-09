#include "boundaries.h"
#include "triangulate.h"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>

LASRboundaries::LASRboundaries(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;

  set_connection(algorithm);

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPolygon);
}

bool LASRboundaries::process(LASheader*& header)
{
  if (!connections.empty()) return true;

  double xmin, ymin, xmax, ymax;

  if (header->vlr_lasoriginal)
  {
    xmin = header->vlr_lasoriginal->min_x;
    ymin = header->vlr_lasoriginal->min_y;
    xmax = header->vlr_lasoriginal->max_x;
    ymax = header->vlr_lasoriginal->max_y;
  }
  else
  {
    xmin = header->min_x;
    ymin = header->min_y;
    xmax = header->max_x;
    ymax = header->max_y;
  }

  PolygonXY bbox;
  bbox.push_back({xmin, ymin});
  bbox.push_back({xmax, ymin});
  bbox.push_back({xmax, ymax});
  bbox.push_back({xmin, ymax});
  bbox.close();
  contour.push_back(bbox);
  return true;
}

bool LASRboundaries::process(LAS*& las)
{
  if (connections.empty())  return true;

  // 'connections' contains a single stage that is supposed to be a triangulation stage.
  // This is the only supported stage as of feb 2024
  auto it = connections.begin();
  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->second);
  if (p == nullptr)
  {
    last_error  = "Internal error. Invalid pointer dynamic cast. Expecting a pointer to LASRtriangulate"; // # nocov
    return false; // # nocov
  }

  std::vector<Edge> edges;
  std::unordered_map<PointXY, PointXY> unordered_contour;

  p->contour(edges);

  for (const auto& v : edges)
  {
    unordered_contour.insert({v.A, v.B});
  }

  edges.clear();
  edges.shrink_to_fit();

  while (!unordered_contour.empty())
  {
    PointXY p;
    PolygonXY ordered_contour;

    auto it = unordered_contour.cbegin();

    do
    {
      ordered_contour.push_back(it->first);
      p = it->second;
      unordered_contour.erase(it);
      it = unordered_contour.find(p);

    } while(it != unordered_contour.end());

    ordered_contour.close();

    contour.push_back(ordered_contour);
  }

  // Sort the vector using is_clockwise such as the outer ring comes first to we spec compliant
  std::sort(contour.begin(), contour.end(), [](const PolygonXY& a, const PolygonXY& b) {
    return a.is_clockwise() < b.is_clockwise();
  });

  return true;
}

void LASRboundaries::clear(bool last)
{
  contour.clear();
}

bool LASRboundaries::write()
{
  bool sucess;

  #pragma omp critical (write_contour)
  {
    sucess = vector.write(contour);
  }

  return sucess;
}

bool LASRboundaries::need_points() const
{
  return !connections.empty();
}