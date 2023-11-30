#ifndef VECTOR_H
#define VECTOR_H

#include "GDALdataset.h"
#include "Shape.h"

class Vector : public GDALdataset
{
public:
  Vector();
  Vector(double xmin, double ymin, double xmax, double ymax, int nattr = 1);
  //bool write_point(double x, double y, double z);
  //bool write_point(const PointXYZ& p);
  bool write_point(const PointLAS& p);
  bool write_triangulation(const std::vector<TriangleXYZ>& triangles);
  bool write_polygon(const std::vector<PolygonXY>& poly);
  //void set_attr_name(std::string name, int attr);
  void set_chunk(double xmin, double ymin, double xmax, double ymax);

private:
  int nattr;
  double extent[4];
};


#endif //VECTOR_H