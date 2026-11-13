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
  if (other.transform) transform.reset(other.transform->Clone());
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

  return true;
}

// The CRS received is the source CRS. The next stages receive the target CRS.
void LASRtransformcrs::set_crs(const CRS& crs)
{
  this->crs = target_crs;

  if (transform && crs == source_crs) return;

  source_crs = crs;
  transform.reset();
  if (!source_crs.is_valid()) return;

  // x is the longitude or the easting, whatever the axis order of the CRS
  OGRSpatialReference source = source_crs.get_crs();
  OGRSpatialReference target = target_crs.get_crs();
  source.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  target.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  transform.reset(OGRCreateCoordinateTransformation(&source, &target));
}

// Bounding box of the reprojected box. The edges of a reprojected box are curved so they are
// sampled. Returns false, and leaves the box unchanged, if a sample cannot be reprojected.
bool LASRtransformcrs::reproject_bbox(double& xmin, double& ymin, double& xmax, double& ymax) const
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
  transform->Transform((int)x.size(), x.data(), y.data(), nullptr, ok.data());
  if (std::find(ok.begin(), ok.end(), 0) != ok.end()) return false;

  xmin = *std::min_element(x.begin(), x.end());
  ymin = *std::min_element(y.begin(), y.end());
  xmax = *std::max_element(x.begin(), x.end());
  ymax = *std::max_element(y.begin(), y.end());
  return true;
}

// Like transform_with() with a matrix: the next stages must receive the extent of the
// reprojected coverage.
void LASRtransformcrs::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  if (!transform) return;
  if (!reproject_bbox(xmin, ymin, xmax, ymax)) return;

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

  if (!transform) return true; // process() fails with an error message

  double diagonal = std::hypot(chunk.xmax - chunk.xmin, chunk.ymax - chunk.ymin);

  if (!reproject_bbox(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax))
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
  las->header->crs = target_crs;

  if (dropped > 0)
  {
    warning("transform_crs: %lu points that cannot be represented in the target CRS were discarded\n", (unsigned long)dropped);
    las->delete_deleted();
  }

  las->update_header();

  return true;
}
