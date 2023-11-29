#include "boundaries.h"
#include "triangulate.h"

#include <unordered_set>
#include <unordered_map>

LASRboundaries::LASRboundaries(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->algorithm = algorithm;

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPolygon);
}

bool LASRboundaries::process(LASheader*& header)
{
  if (algorithm) return true;

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
  if (!algorithm) return true;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(algorithm);

  if (p == 0)
  {
    last_error  = "Internal error. Invalid pointer dynamic cast. Expecting a pointer to LASRtriangulate"; // # nocov
    return false;
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

  return true;
}

void LASRboundaries::clear(bool last)
{
  contour.clear();
}

bool LASRboundaries::write()
{
  return vector.write_polygon(contour);
}

bool LASRboundaries::need_points() const
{
  return algorithm != nullptr;
}