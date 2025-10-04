#ifndef VECTOR_H
#define VECTOR_H

#include "GDALdataset.h"
#include "PointLAS.h"
#include "Chunk.h"

typedef std::pair<std::string, OGRFieldType> Field;

class Vector : public GDALdataset
{
public:
  Vector();
  Vector(double xmin, double ymin, double xmax, double ymax, int nattr = 1);
  Vector(const Vector& vector, const Chunk& chunk);
  bool create_file();
  bool write(const std::vector<PointLAS>& batch, bool write_attributes = false);
  bool write(const PointXYZAttrs& p);
  bool write(const std::vector<TriangleXYZ>& triangles);
  bool write(const std::vector<PolygonXY>& poly);
  void add_field(const std::string& name, OGRFieldType type);
  void set_chunk(const Chunk& chunk);
  int get_dupfid() const { return dupfid; };
  //void set_fields_for(writable type) { writetype = type; };

private:
  int nattr;
  int writetype;
  int dupfid;
  double extent[4];
  std::vector<Field> fields;
};


#endif //VECTOR_H