#ifndef VECTOR_H
#define VECTOR_H

#include "GDALdataset.h"
#include "Shape.h"
#include "Chunk.h"

class Vector : public GDALdataset
{
public:
  enum writable {POINTXYZ, POINTLAS, TRIANGULATION, POLYGON};

  Vector();
  Vector(double xmin, double ymin, double xmax, double ymax, int nattr = 1);
  Vector(const Vector& vector, const Chunk& chunk);
  bool create_file();
  bool write(const PointLAS& p);
  bool write(const std::vector<TriangleXYZ>& triangles);
  bool write(const std::vector<PolygonXY>& poly);
  //void set_attr_name(std::string name, int attr);
  void set_chunk(const Chunk& chunk);
  void set_fields_for(writable type) { writetype = type; };

private:
  int nattr;
  double extent[4];
  int writetype;
};


#endif //VECTOR_H