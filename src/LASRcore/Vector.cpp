#include "Vector.h"
#include "NA.h"
#include "Rcompatibility.h"

#include "cpl_error.h"

Vector::Vector() : GDALdataset()
{
  writetype = UNDEFINED;
  nattr = 0;
  extent[0] = 0;
  extent[1] = 0;
  extent[2] = 0;
  extent[3] = 0;
  GDALdataset::set_vector(wkbUnknown);
}

Vector::Vector(double xmin, double ymin, double xmax, double ymax, int nattr) : GDALdataset()
{
  writetype = UNDEFINED;
  this->nattr = nattr;
  extent[0] = xmin;
  extent[1] = ymin;
  extent[2] = xmax;
  extent[3] = ymax;
  GDALdataset::set_vector(wkbUnknown);
}

Vector::Vector(const Vector& vector, const Chunk& chunk) : GDALdataset()
{
  extent[0] = chunk.xmin;
  extent[1] = chunk.ymin;
  extent[2] = chunk.xmax;
  extent[3] = chunk.ymax;

  nattr = vector.nattr;
  eGType = vector.eGType;
  dType = vector.dType;
  oSRS = vector.oSRS;
  writetype = vector.writetype;
}

/*bool Vector::write_point(double x, double y, double z)
{
  if (!dataset)
  {
    if (!create_file())
    {
      return false;
    }
  }

  if (eGType != wkbPoint25D)
  {
    last_error = "ERROR: The file is not of type POINT";
    return false;
  }

  // Write only points inside the bounding box
  if (x < extent[0] || x > extent[2] || y < extent[1] || y > extent[3])
  {
    return true;
  }

  OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
  OGRPoint point;
  point.setX(x);
  point.setY(y);
  point.setZ(z);
  feature->SetGeometry(&point);

  if (layer->CreateFeature(feature) != OGRERR_NONE)
  {
    last_error = "ERROR: Failed to create feature.";
    OGRFeature::DestroyFeature(feature);
    return false;
  }

  OGRFeature::DestroyFeature(feature);
  return true;
}

bool Vector::write_point(const PointXYZ& p)
{
  return write_point(p.x, p.y, p.z);
}*/

bool Vector::create_file()
{
  if (!GDALdataset::create_file())
  {
    return false;
  }

  if (writetype == POINTLAS)
  {
    OGRFieldDefn intensity("Intensity", OFTInteger);
    layer->CreateField(&intensity);

    OGRFieldDefn gpstime("gpstime", OFTReal);
    layer->CreateField(&gpstime);

    OGRFieldDefn returnnumber("ReturnNumber", OFTInteger);
    layer->CreateField(&returnnumber);

    OGRFieldDefn classification("Classification", OFTInteger);
    layer->CreateField(&classification);

    OGRFieldDefn scanangle("ScanAngle", OFTReal);
    layer->CreateField(&scanangle);
  }

  return true;
}

bool Vector::write(const PointLAS& p)
{
  if (!dataset)
  {
    last_error = "cannot write with uninitialized GDALDataset"; // # nocov
    return false; // # nocov
  }

  if (eGType != wkbPoint25D)
  {
    last_error = "ERROR: The file is not of type POINT"; // # nocov
    return false; // # nocov
  }

  // Write only points inside the bounding box
  if (p.x < extent[0] || p.x > extent[2] || p.y < extent[1] || p.y > extent[3])
  {
    return true;
  }

  // Check if a feature with the same FID already exists. This should not happened
  OGRFeature* existingFeature = layer->GetFeature(p.FID);
  if (existingFeature)
  {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "trying to insert a point with FID = %u that is already in the database. This may be due to overlapping tiles. Point skipped.", p.FID);
    last_error = std::string(buffer);
    last_error_code = DUPFID;
    OGRFeature::DestroyFeature(existingFeature);  // Release the existing feature
    return false;
  }

  OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
  OGRPoint point;
  point.setX(p.x);
  point.setY(p.y);
  point.setZ(p.z);
  feature->SetGeometry(&point);

  feature->SetField("Intensity", p.intensity);
  feature->SetField("gpstime", p.gps_time);
  feature->SetField("ReturnNumber", p.return_number);
  feature->SetField("Classification", p.classification);
  feature->SetField("ScanAngle", p.scan_angle);
  feature->SetFID(p.FID);

  if (layer->CreateFeature(feature) != OGRERR_NONE)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "error %d while writing point %u (%.2lf %.2lf). %s", CPLGetLastErrorNo(),  p.FID, p.x, p.y, CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    OGRFeature::DestroyFeature(feature);
    return false;
    // # nocov end
  }

  OGRFeature::DestroyFeature(feature);
  return true;
}

bool Vector::write(const std::vector<TriangleXYZ>& triangles)
{
  if (!dataset)
  {
    last_error = "cannot write with uninitialized GDALDataset"; // # nocov
    return false; // # nocov
  }

  if (eGType != wkbMultiPolygon25D)
  {
    last_error = "ERROR: The file is not of type MULTIPOLYGON"; // # nocov
    return false; // # nocov
  }

  OGRMultiPolygon triangulation;
  for (auto& tri : triangles)
  {
    PointXYZ centroid = tri.centroid();
    if (!(centroid.x >= extent[0] && centroid.x <= extent[2] && centroid.y >= extent[1] && centroid.y <= extent[3]))
      continue;

    OGRLinearRing ring;
    ring.addPoint(tri.A.x, tri.A.y, tri.A.z);
    ring.addPoint(tri.B.x, tri.B.y, tri.B.z);
    ring.addPoint(tri.C.x, tri.C.y, tri.C.z);
    ring.addPoint(tri.A.x, tri.A.y, tri.A.z);
    //feature->SetField("Attribute", std::get<3>(points[j]));

    OGRPolygon triangle;
    triangle.addRing(&ring);

    triangulation.addGeometry(&triangle);
  }

  OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
  feature->SetGeometry(&triangulation);

  if (layer->CreateFeature(feature) != OGRERR_NONE)
  {
    last_error = "ERROR: GDAL failed to create feature."; // # nocov
    OGRFeature::DestroyFeature(feature); // # nocov
    return false; // # nocov
  }

  OGRFeature::DestroyFeature(feature);
  return true;
}

bool Vector::write(const std::vector<PolygonXY>& poly)
{
  if (!dataset)
  {
    last_error = "cannot write with uninitialized GDALDataset"; // # nocov
    return false; // # nocov
  }

  if (eGType != wkbPolygon)
  {
    last_error = "ERROR: The file is not of type POLYGON"; // # nocov
    return false; // # nocov
  }

  // Write only points inside the bounding box
  /*if (tri.xmin < extent[0] || tri.xmax > extent[2] || tri.ymin < extent[1] || tri.ymax > extent[3])
  {
    return false;
  }*/

  OGRPolygon polygon;
  for (const auto& pol : poly)
  {
    OGRLinearRing ring;
    for (const auto& p : pol.coordinates)
    {
      //printf("%.2lf, %.2lf\n", p.x, p.y);
      ring.addPoint(p.x, p.y);
    }

    polygon.addRing(&ring);
  }

  // Commented to fix #6 temporarily
  /*if (!polygon.IsValid())
  {
    warning("invalid polygon\n");
  }*/

  OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
  feature->SetGeometry(&polygon);

  if (layer->CreateFeature(feature) != OGRERR_NONE)
  {
    last_error = "ERROR: GDAL failed to create feature.";  // # nocov
    OGRFeature::DestroyFeature(feature); // # nocov
    return false; // # nocov
  }

  OGRFeature::DestroyFeature(feature);
  return true;
}

void Vector::set_chunk(const Chunk& chunk)
{
  extent[0] = chunk.xmin;
  extent[1] = chunk.ymin;
  extent[2] = chunk.xmax;
  extent[3] = chunk.ymax;
}
