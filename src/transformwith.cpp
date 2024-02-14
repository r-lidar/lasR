#include "transformwith.h"
#include "triangulate.h"
#include "NA.h"

LASRtransformwith::LASRtransformwith(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm, std::string op, std::string attribute)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->algorithm = algorithm;
  this->attribute = attribute;

  this->op = SUB;
  if (op == "-") this->op = SUB;
  if (op == "+") this->op = ADD;
}

bool LASRtransformwith::process(LAS*& las)
{
  if (algorithm == nullptr)
  {
    last_error = "unitialized pointer to LASRalgorithm"; // # nocov
    return false; // # nocov
  }

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(algorithm);
  LASRalgorithmRaster* q = dynamic_cast<LASRalgorithmRaster*>(algorithm);

  if (p == nullptr && q == nullptr)
  {
    last_error = "invalid dynamic cast. Expecting a pointer to LASRtriangulate or LASalgorithmRaster"; // # nocov
    return false; // # nocov
  }

  int attr_index = -1;
  int data_type = -1;
  double scale = 1;
  double offset = 0;
  if (!attribute.empty())
  {
    attr_index = las->header->get_attribute_index(attribute.c_str());

    if (attr_index == -1)
    {
      last_error = "invalid attribute index";
      return false;
    }

    data_type = las->header->attributes[attr_index].data_type;

    if (data_type != LAS::LONG && data_type != LAS::DOUBLE)
    {
      last_error = "the extrabyte attribute must be of type 'int' or 'double'";
      return false;
    }

    if (data_type == LAS::LONG)
    {
      scale = las->point.quantizer->z_scale_factor;
      //offset = las->point.quantizer->z_offset;
      las->header->attributes[attr_index].set_scale(scale);
      las->header->attributes[attr_index].set_offset(offset);
      las->header->update_extra_bytes_vlr();
    }
  }

  std::vector<double> hag;

  if (p != nullptr)
  {
    p->interpolate(hag, nullptr);
  }
  else
  {
    Raster& raster = q->get_raster();
    hag.resize(las->npoints);
    std::fill(hag.begin(), hag.end(), NA_F64);

    while (las->read_point())
    {
      float val = raster.get_value(las->point.get_x(), las->point.get_y());
      if (val != raster.get_nodata()) hag[las->current_point] = val;
    }
  }

  U32 deleted = 0;
  while (las->read_point())
  {
    double z = hag[las->current_point];

    if (z == NA_F64)
    {
      las->remove_point();
      deleted++;
      continue;
    }

    switch(op)
    {
      case SUB: z = las->point.get_z() - z; break;
      case ADD: z = las->point.get_z() + z; break;
      default: last_error = "internal error, invalid operator"; return false; break; // # nocov
    }

    if (attr_index == -1)
      las->point.set_z(z);
    else
      las->point.set_attribute_as_float(attr_index, z);

    las->update_point();
  }

  if (deleted) warning("%u points outside delaunay triangulation were discarded\n", deleted);

  return true;
}