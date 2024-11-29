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

// LASRtransformwith with an Affine Transformation Matrix is special: it modified the extend of the point cloud
// and thus affects subsequent stages bounding boxes. This method updates the bounding box
// in the parser to assign the proper bbox to the next stages
void LASRtransformwith::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  auto it = connections.begin();
  StageMatrix* mat = dynamic_cast<StageMatrix*>(it->second);
  if (mat != nullptr)
  {
    double z0 = 0;
    double x1 = xmin;
    double y1 = ymin;
    double x2 = xmax;
    double y2 = ymin;
    double x3 = xmax;
    double y3 = ymax;
    double x4 = xmin;
    double y4 = ymax;

    mat->transform(x1, y1, z0);
    mat->transform(x2, y2, z0);
    mat->transform(x3, y3, z0);
    mat->transform(x4, y4, z0);

    this->xmin = std::min(std::min(x1, x2), std::min(x3, x4));
    this->ymin = std::min(std::min(y1, y2), std::min(y3, y4));
    this->xmax = std::max(std::max(x1, x2), std::max(x3, x4));
    this->ymax = std::max(std::max(y1, y2), std::max(y3, y4));
  }

  xmin = this->xmin;
  ymin = this->ymin;
  xmax = this->xmax;
  ymax = this->ymax;
}

// LASRtransformwith with an Affine Transformation Matrix is special: it modified the extend of the point cloud
// and thus affects subsequent stages bounding boxes. This method updates the chunk bounding box
// in the parser to assign the proper bbox to the next stages
bool LASRtransformwith::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  auto it = connections.begin();
  StageMatrix* mat = dynamic_cast<StageMatrix*>(it->second);
  if (mat != nullptr)
  {
    double x1 = xmin;
    double y1 = ymin;
    double z1 = 0;
    double x2 = xmax;
    double y2 = ymin;
    double z2 = 0;
    double x3 = xmax;
    double y3 = ymax;
    double z3 = 0;
    double x4 = xmin;
    double y4 = ymax;
    double z4 = 0;

    mat->transform(x1, y1, z1);
    mat->transform(x2, y2, z2);
    mat->transform(x3, y3, z3);
    mat->transform(x4, y4, z4);

    this->xmin = std::min(std::min(x1, x2), std::min(x3, x4));
    this->ymin = std::min(std::min(y1, y2), std::min(y3, y4));
    this->xmax = std::max(std::max(x1, x2), std::max(x3, x4));
    this->ymax = std::max(std::max(y1, y2), std::max(y3, y4));

    chunk.xmin = this->xmin;
    chunk.ymin = this->ymin;
    chunk.xmax = this->xmax;
    chunk.ymax = this->ymax;
  }

  return true;
}

bool LASRtransformwith::process(LAS*& las)
{
  if (connections.empty())
  {
    last_error = "unitialized pointer to Stage"; // # nocov
    return false; // # nocov
  }

  auto it = connections.begin();
  LASRtriangulate* triangulation = dynamic_cast<LASRtriangulate*>(it->second);
  StageRaster* rasterization = dynamic_cast<StageRaster*>(it->second);
  StageMatrix* mat = dynamic_cast<StageMatrix*>(it->second);

  // The stage is neither a triangulation nor a raster stage
  if (triangulation == nullptr && rasterization == nullptr && mat == nullptr)
  {
    last_error = "invalid dynamic cast. Expecting a pointer to LASRtriangulate, LASalgorithmRaster or LASRicp"; // # nocov
    return false; // # nocov
  }

  // With a triangulation or a raster we are only updating Z
  if (triangulation != nullptr || rasterization != nullptr)
  {
    AttributeAccessor set_and_get_value("Z");

    if (!attribute.empty())
    {
      int index = las->header->schema.get_attribute_index(attribute);

      if (index == -1)
      {
        last_error = "invalid attribute";
        return false;
      }

      Attribute* attr = &las->header->schema.attributes[index];
      AttributeType data_type = attr->type;

      if (data_type != AttributeType::INT32 && data_type != AttributeType::DOUBLE)
      {
        last_error = "the attribute " + attribute + " must be of type 'int' or 'double'";
        return false;
      }

      if (data_type == AttributeType::INT32 && attr->scale_factor == 1)
      {
        attr->scale_factor = las->header->schema.attributes[2].scale_factor;
        //last_error = "the attribute " + attribute + " is of type 'int' but does not have as scale factor";
        //return false;
      }

      set_and_get_value = AttributeAccessor(attribute);
    }

    std::vector<double> hag;

    if (triangulation != nullptr)
    {
      if (!triangulation->interpolate(hag, nullptr))
        return false;
    }
    else if (rasterization != nullptr)
    {
      const Raster& raster = rasterization->get_raster();
      hag.resize(las->npoints);
      std::fill(hag.begin(), hag.end(), NA_F64);

      while (las->read_point())
      {
        float val = raster.get_value_bilinear(las->p.get_x(), las->p.get_y());
        if (val != raster.get_nodata()) hag[las->current_point] = val;
      }
    }

    unsigned int deleted = 0;
    while (las->read_point())
    {
      double z = hag[las->current_point];

      if (z == NA_F64)
      {
        las->delete_point();
        deleted++;
        continue;
      }

      switch(op)
      {
      case SUB: z = las->p.get_z() - z; break;
      case ADD: z = las->p.get_z() + z; break;
      default: last_error = "internal error, invalid operator"; return false; break; // # nocov
      }

      set_and_get_value(&las->p, z);
    }

    las->update_header();

    //if (deleted) warning("%u points outside delaunay triangulation were discarded\n", deleted);
    if (deleted == hag.size()) warning("No Delaunay triangulation. All points were discarded\n");
  }
  // With a matrix we are transforming the 3 coordinates
  else if (mat != nullptr)
  {
    double x = 0;
    double y = 0;
    double z = 0;

    double new_xoffset = las->header->schema.attributes[0].offset;
    double new_yoffset = las->header->schema.attributes[1].offset;
    double new_zoffset = las->header->schema.attributes[2].offset;

    mat->transform(new_xoffset, new_yoffset, new_zoffset);

    while (las->read_point())
    {
      x = las->p.get_x();
      y = las->p.get_y();
      z = las->p.get_z();

      mat->transform(x,y,z);

      las->p.set_X((x-new_xoffset)/las->header->schema.attributes[0].scale_factor);
      las->p.set_Y((y-new_yoffset)/las->header->schema.attributes[1].scale_factor);
      las->p.set_Z((z-new_zoffset)/las->header->schema.attributes[2].scale_factor);
    }


    las->header->schema.attributes[0].value_offset = new_xoffset;
    las->header->schema.attributes[1].value_offset = new_yoffset;
    las->header->schema.attributes[2].value_offset = new_zoffset;

    las->seek(0);

    las->update_header();
  }

  return true;
}

bool LASRtransformwith::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(s);
  StageRaster* q = dynamic_cast<StageRaster*>(s);
  StageMatrix* m = dynamic_cast<StageMatrix*>(s);

  if (p)
    set_connection(p);
  else if(q)
    set_connection(q);
  else if (m)
    set_connection(m);
  else
  {
    last_error = "Incompatible stage combination for 'transform_with'"; // # nocov
    return false; // # nocov
  }

  return true;
}