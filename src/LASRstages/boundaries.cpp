#include "boundaries.h"
#include "triangulate.h"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include <iostream>

#include "ogrsf_frmts.h"

bool LASRboundaries::set_parameters(const nlohmann::json& stage)
{
  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPolygon);
  return true;
}

bool LASRboundaries::process(Header*& header)
{
  ASSERT_VALID_POINTER(header);

  if (!connections.empty()) return true;

  double xmin, ymin, xmax, ymax;

  xmin = header->min_x + buffer;
  ymin = header->min_y + buffer;
  xmax = header->max_x - buffer;
  ymax = header->max_y - buffer;

  PolygonXY bbox;
  bbox.push_back({xmin, ymin});
  bbox.push_back({xmax, ymin});
  bbox.push_back({xmax, ymax});
  bbox.push_back({xmin, ymax});
  bbox.close();
  contour.push_back(bbox);
  return true;
}

bool LASRboundaries::process(PointCloud*& las)
{
  ASSERT_VALID_POINTER(las);

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

  // Get the contour of the triangulation
  std::vector<Edge> edges;
  p->contour(edges);

  // Collect edges into OGRLineString and put in OGRGeometryCollection
  OGRGeometryCollection gc;
  for (const auto& e : edges) {
    OGRLineString ls;
    ls.addPoint(e.A.x, e.A.y);
    ls.addPoint(e.B.x, e.B.y);
    gc.addGeometry(&ls);
  }

  OGRGeometry* polys = gc.Polygonize();

  if (!polys)
  {
    last_error = "Polygonize failed";
    return false;
  }

  if (wkbFlatten(polys->getGeometryType()) != wkbGeometryCollection)
  {
    last_error = "Polygonize returned unexpected geometry type";
    OGRGeometryFactory::destroyGeometry(polys);
    return false;
  }

  contour.clear();

  OGRGeometryCollection* ogc = polys->toGeometryCollection();
  for (int i = 0; i < ogc->getNumGeometries(); ++i)
  {
    OGRGeometry* geom = ogc->getGeometryRef(i);
    if (!geom) continue;

    if (wkbFlatten(geom->getGeometryType()) == wkbPolygon)
    {
      OGRPolygon* ogrPoly = geom->toPolygon();

      OGRLinearRing* exterior = ogrPoly->getExteriorRing();
      if (!exterior) continue;

      std::vector<PointXY> coords;
      int n = exterior->getNumPoints();
      for (int j = 0 ; j < n ; j++)
        coords.emplace_back(PointXY{exterior->getX(j), exterior->getY(j)});

      PolygonXY poly(coords);
      contour.push_back(poly);
    }
  }

  std::vector<bool> is_hole(contour.size(), false);

  // First pass: determine which polygons are holes
  for (size_t i = 0; i < contour.size(); i++)
  {
    for (size_t j = 0; j < contour.size(); j++)
    {
      if (i == j) continue;

      if (contour[j].contains(contour[i]))
      {
        is_hole[i] = true;
        break;
      }
    }
  }

  // Second pass: set orientation based on hole status
  for (size_t i = 0; i < contour.size(); ++i)
  {
    if (is_hole[i])
      contour[i].make_clockwise();
    else
      contour[i].make_counterclockwise();
  }

  // Third pass: move outer ring (first non-hole) to the front
  for (size_t i = 0; i < contour.size(); ++i)
  {
    if (!is_hole[i])
    {
      if (i != 0)
        std::swap(contour[0], contour[i]);
      break;
    }
  }

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

bool LASRboundaries::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'hull'"; // # nocov
    return false; // # nocov
  }

  return true;
}