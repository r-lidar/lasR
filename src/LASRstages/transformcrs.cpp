#include "transformcrs.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>

// OGRCoordinateTransformation is not thread-safe so each thread needs its own. It must be a copy
// of the one built on the main thread and not a new one: PROJ may select another operation in
// another thread (e.g. a grid shift when PROJ network is enabled after the main context was
// created, as terra and sf do), and the points of some chunks would be shifted by decimetres.
LASRtransformcrs::LASRtransformcrs(const LASRtransformcrs& other) : Stage(other)
{
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);

  source_crs = other.source_crs;
  target_crs = other.target_crs;
  xoffset = other.xoffset;
  yoffset = other.yoffset;

  for (const auto& s : other.sources)
  {
    sources.push_back({s.crs, Transformation(s.transform ? s.transform->Clone() : nullptr)});
    if (s.transform.get() == other.transform) transform = sources.back().transform.get();
  }
}

bool LASRtransformcrs::set_parameters(const nlohmann::json& stage)
{
  int epsg = stage.value("epsg", 0);
  std::string wkt = stage.value("wkt", "");

  try
  {
    if (epsg > 0) target_crs = CRS(epsg, true);
    else if (wkt.size() > 0) target_crs = CRS(wkt, true);
  }
  catch(const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  if (!target_crs.is_valid())
  {
    last_error = "invalid target CRS";
    return false;
  }

  // Z is not modified. A target that describes the heights (EPSG:4979, a compound CRS) would
  // label heights that were not transformed.
  if (target_crs.get_crs().GetAxesCount() != 2)
  {
    last_error = "transform_crs: the target CRS must be a 2D CRS because Z is not transformed. The vertical CRS of the point cloud, if any, is kept.";
    return false;
  }

  return true;
}

// Z is not modified so it still refers to the vertical CRS of the source, if the source is a
// compound CRS. The output CRS is then the target CRS combined with this vertical CRS.
static CRS keep_vertical(const CRS& target, const CRS& source)
{
  if (!source.is_valid()) return target;

  OGRSpatialReference src = source.get_crs();
  if (!src.IsCompound()) return target;

  const OGR_SRSNode* node = src.GetAttrNode("VERT_CS");
  if (node == nullptr) return target;

  char* wkt = nullptr;
  OGRSpatialReference vertical;
  OGRErr err = node->exportToWkt(&wkt);
  if (err == OGRERR_NONE) err = vertical.importFromWkt(wkt);
  CPLFree(wkt);
  if (err != OGRERR_NONE) return target;

  OGRSpatialReference horizontal = target.get_crs();
  if (horizontal.GetName() == nullptr || vertical.GetName() == nullptr) return target;

  OGRSpatialReference compound;
  std::string name = std::string(horizontal.GetName()) + " + " + vertical.GetName();
  if (compound.SetCompoundCS(name.c_str(), &horizontal, &vertical) != OGRERR_NONE) return target;

  const char* options[] = {"FORMAT=WKT2", nullptr};
  wkt = nullptr;
  err = compound.exportToWkt(&wkt, options);
  CRS out = (err == OGRERR_NONE) ? CRS(std::string(wkt)) : CRS();
  CPLFree(wkt);
  return out.is_valid() ? out : target;
}

LASRtransformcrs::Transformation LASRtransformcrs::create_transformation(const CRS& source_crs) const
{
  if (!source_crs.is_valid()) return Transformation();

  // x is the longitude or the easting, whatever the axis order of the CRS
  OGRSpatialReference source = source_crs.get_crs();
  OGRSpatialReference target = target_crs.get_crs();
  source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  return Transformation(OGRCreateCoordinateTransformation(&source, &target));
}

// The CRS received is the source CRS. The next stages receive the target CRS. The
// transformations are built here, on the main thread (see the copy constructor).
void LASRtransformcrs::set_crs(const CRS& crs)
{
  this->crs = keep_vertical(target_crs, crs);

  // set_crs() is called twice when the pipeline is parsed
  if (!sources.empty() && crs == sources[0].crs) return;

  sources.clear();
  transform = nullptr;
  source_crs = crs;
  if (!source_crs.is_valid()) return;

  sources.push_back({crs, create_transformation(crs)});

  // The files are in different CRS: each one is reprojected from its own CRS. The CRS of the
  // chunk tells which one (see set_chunk()). If set_crs() assigned the CRS of the first file to
  // all the files, only the first transformation is used.
  if (file_collection != nullptr && crs == file_collection->get_crs())
  {
    for (const auto& other : file_collection->get_crs_list())
    {
      if (other.is_valid() && !other.is_same(crs)) sources.push_back({other, create_transformation(other)});
    }
  }

  transform = sources[0].transform.get();
}

// Bounding box of the reprojected box. The edges of a reprojected box are curved so they are
// sampled. Returns false, and leaves the box unchanged, if a sample cannot be reprojected.
bool LASRtransformcrs::reproject_bbox(OGRCoordinateTransformation* ct, double& xmin, double& ymin, double& xmax, double& ymax) const
{
  const int n = 20;
  std::vector<double> x, y;
  for (int i = 0 ; i <= n ; i++)
  {
    double px = xmin + (xmax - xmin) * i / n;
    double py = ymin + (ymax - ymin) * i / n;
    x.insert(x.end(), {px, px, xmin, xmax});
    y.insert(y.end(), {ymin, ymax, py, py});
  }

  std::vector<int> ok(x.size());
  ct->Transform((int)x.size(), x.data(), y.data(), nullptr, ok.data());
  if (std::find(ok.begin(), ok.end(), 0) != ok.end()) return false;

  xmin = *std::min_element(x.begin(), x.end());
  ymin = *std::min_element(y.begin(), y.end());
  xmax = *std::max_element(x.begin(), x.end());
  ymax = *std::max_element(y.begin(), y.end());
  return true;
}

// The extent of a collection of files in different CRS mixes their coordinates. The extent of
// the reprojected coverage is the union of the reprojected files.
bool LASRtransformcrs::reproject_files(double& xmin, double& ymin, double& xmax, double& ymax) const
{
  const std::vector<Header>& headers = file_collection->get_headers();
  const std::vector<CRS>& list = file_collection->get_crs_list();

  double x0 = std::numeric_limits<double>::max();
  double y0 = std::numeric_limits<double>::max();
  double x1 = std::numeric_limits<double>::lowest();
  double y1 = std::numeric_limits<double>::lowest();

  for (size_t i = 0 ; i < headers.size() ; i++)
  {
    const CRS& crs = list[file_collection->get_crs_index(i)];
    auto it = std::find_if(sources.begin(), sources.end(), [&crs](const Source& s) { return s.crs.is_same(crs); });
    if (it == sources.end() || !it->transform) return false;

    double a = headers[i].min_x, b = headers[i].min_y, c = headers[i].max_x, d = headers[i].max_y;
    if (!reproject_bbox(it->transform.get(), a, b, c, d)) return false;

    x0 = std::min(x0, a);
    y0 = std::min(y0, b);
    x1 = std::max(x1, c);
    y1 = std::max(y1, d);
  }

  xmin = x0;
  ymin = y0;
  xmax = x1;
  ymax = y1;
  return true;
}

// Like transform_with() with a matrix: the next stages must receive the extent of the
// reprojected coverage.
void LASRtransformcrs::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  if (!transform) return;

  if (sources.size() > 1)
  {
    if (!reproject_files(xmin, ymin, xmax, ymax)) return;
  }
  else if (!reproject_bbox(transform, xmin, ymin, xmax, ymax))
  {
    return;
  }

  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  // One offset for all the chunks, so a point read in two chunks gets the same coordinates
  xoffset = std::round((xmin + xmax) / 2);
  yoffset = std::round((ymin + ymax) / 2);
}

// Like transform_with() with a matrix: the next stages must receive the bounding box of the
// reprojected chunk.
bool LASRtransformcrs::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // The points of a chunk are read together: they cannot be in several CRS
  if (chunk.mixed_crs)
  {
    last_error = "transform_crs: the chunk " + chunk.name + " reads files in different CRS together (buffer or query). Files in different CRS can only be reprojected file by file, without buffer.";
    return false;
  }

  // The files are in different CRS: the transformation of the CRS of the file
  if (sources.size() > 1)
  {
    auto it = std::find_if(sources.begin(), sources.end(), [&chunk](const Source& s) { return s.crs.is_same(chunk.crs); });
    if (it == sources.end())
    {
      last_error = "transform_crs: the file " + chunk.name + " has no CRS";
      return false;
    }

    source_crs = it->crs;
    transform = it->transform.get();

    if (!keep_vertical(target_crs, source_crs).is_same(crs))
    {
      last_error = "transform_crs: the files do not have the same vertical CRS";
      return false;
    }
  }

  // The next stages receive the target CRS
  chunk.crs = crs;

  if (!transform) return true; // process() fails with an error message

  double diagonal = std::hypot(chunk.xmax - chunk.xmin, chunk.ymax - chunk.ymin);

  if (!reproject_bbox(transform, chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax))
  {
    warning("transform_crs: cannot reproject the extent of the chunk\n");
    return true;
  }

  // The buffer was read in the units of the source CRS. The next stages need it in the units of
  // the target CRS (e.g. 20 m is about 0.0002 degrees).
  if (diagonal > 0)
    chunk.buffer *= std::hypot(chunk.xmax - chunk.xmin, chunk.ymax - chunk.ymin) / diagonal;

  // The reprojected coordinates are rounded to the scale factor (see process()). A point on the
  // edge of the chunk must not be moved outside of it and be mistaken for a buffer point.
  double margin = target_crs.is_geographic() ? 1e-7 : 0.01;
  chunk.xmin -= margin;
  chunk.ymin -= margin;
  chunk.xmax += margin;
  chunk.ymax += margin;

  // A reprojected circle is not a circle
  if (chunk.shape == ShapeType::CIRCLE) chunk.shape = ShapeType::RECTANGLE;

  return Stage::set_chunk(chunk);
}

bool LASRtransformcrs::process(PointCloud*& las)
{
  if (!transform)
  {
    last_error = "transform_crs: cannot transform the CRS of the point cloud to the target CRS. Does the point cloud have a CRS?";
    return false;
  }

  // The transformation is chosen from the CRS of the file in set_chunk()
  if (!las->header->crs.is_same(source_crs))
  {
    last_error = "transform_crs: the point cloud is not in the CRS of its file";
    return false;
  }

  Attribute& attrx = las->header->schema.attributes[AttributeCore::X];
  Attribute& attry = las->header->schema.attributes[AttributeCore::Y];

  if (attrx.type != AttributeType::INT32 || attry.type != AttributeType::INT32)
  {
    last_error = "transform_crs: only X and Y stored as scaled integers (LAS, LAZ) are supported";
    return false;
  }

  // The scale factors are kept unless the units change between degrees and a linear unit:
  // 1e-7 degree is about 1 cm.
  double xscale = attrx.scale_factor;
  double yscale = attry.scale_factor;
  if (source_crs.is_geographic() != target_crs.is_geographic())
    xscale = yscale = target_crs.is_geographic() ? 1e-7 : 0.01;

  // The points are transformed by batches: transforming them one by one is much slower.
  const size_t batch = 65536;
  std::vector<double> x(batch), y(batch);
  std::vector<unsigned char*> data(batch);
  std::vector<int> ok(batch);
  size_t n = 0;
  size_t dropped = 0;

  Point p;
  p.set_schema(&las->header->schema);

  auto transform_batch = [&]()
  {
    if (n == 0) return;

    transform->Transform((int)n, x.data(), y.data(), nullptr, ok.data());

    for (size_t i = 0 ; i < n ; i++)
    {
      // Only if the extent of the coverage could not be reprojected in get_extent()
      if (ok[i] && std::isnan(xoffset))
      {
        xoffset = std::round(x[i]);
        yoffset = std::round(y[i]);
      }

      double X = std::round((x[i] - xoffset) / xscale);
      double Y = std::round((y[i] - yoffset) / yscale);

      // Outside the domain of the transformation or not storable on 32 bits
      const double max = std::numeric_limits<int>::max();
      p.data = data[i];
      if (!ok[i] || !(std::fabs(X) <= max) || !(std::fabs(Y) <= max))
      {
        p.set_deleted();
        dropped++;
        continue;
      }

      p.set_X((int)X);
      p.set_Y((int)Y);
    }

    n = 0;
  };

  while (las->read_point())
  {
    x[n] = las->point.get_x();
    y[n] = las->point.get_y();
    data[n] = las->point.data;
    n++;
    if (n == batch) transform_batch();
  }
  transform_batch();

  attrx.scale_factor = xscale;
  attry.scale_factor = yscale;
  attrx.value_offset = xoffset;
  attry.value_offset = yoffset;
  las->header->crs = crs;

  if (dropped > 0)
  {
    warning("transform_crs: %lu points that cannot be represented in the target CRS were discarded\n", (unsigned long)dropped);
    las->delete_deleted();
  }

  las->update_header();

  return true;
}
