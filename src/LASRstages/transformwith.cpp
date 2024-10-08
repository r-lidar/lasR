#include "transformwith.h"
#include "triangulate.h"
#include "NA.h"

bool LASRtransformwith::set_parameters(const nlohmann::json& stage)
{
  std::string op = stage.value("operator", "-");
  attribute = stage.value("store_in_attribute", "");

  this->op = SUB;
  if (op == "-") this->op = SUB;
  if (op == "+") this->op = ADD;

  return true;
}

bool LASRtransformwith::process(LAS*& las)
{
  if (connections.empty())
  {
    last_error = "unitialized pointer to Stage"; // # nocov
    return false; // # nocov
  }

  // 'connections' contains a single stage that is supposed to be a either a triangulation stage.
  // or a raster stage. This is the only two supported stage as of feb 2024
  auto it = connections.begin();
  LASRtriangulate* triangulation = dynamic_cast<LASRtriangulate*>(it->second);
  StageRaster* rasterization = dynamic_cast<StageRaster*>(it->second);

  // The stage is neither a triangulation nor a raster stage
  if (triangulation == nullptr && rasterization == nullptr)
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

  if (triangulation != nullptr)
  {
    if (!triangulation->interpolate(hag, nullptr))
      return false;
  }
  else
  {
    const Raster& raster = rasterization->get_raster();
    hag.resize(las->npoints);
    std::fill(hag.begin(), hag.end(), NA_F64);

    while (las->read_point())
    {
      float val = raster.get_value(las->point.get_x(), las->point.get_y());
      if (val != raster.get_nodata()) hag[las->current_point] = val;
    }
  }

  unsigned int deleted = 0;
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

  las->update_header();

  if (deleted == hag.size()) warning("No Delaunay triangulation. All points were discarded\n");

  //if (deleted) warning("%u points outside delaunay triangulation were discarded\n", deleted);

  return true;
}

bool LASRtransformwith::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(s);
  StageRaster* q = dynamic_cast<StageRaster*>(s);

  if (p)
    set_connection(p);
  else if(q)
    set_connection(q);
  else
  {
    last_error = "Incompatible stage combination for 'transform_with'"; // # nocov
    return false; // # nocov
  }

  return true;
}