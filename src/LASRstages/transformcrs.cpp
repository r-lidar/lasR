#include "transformcrs.h"

#include <ogr_spatialref.h>

#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>

LASRtransformcrs::LASRtransformcrs()
{
  transform = nullptr;
}

LASRtransformcrs::LASRtransformcrs(const LASRtransformcrs& other) : Stage(other)
{
  source_crs = other.source_crs;
  target_crs = other.target_crs;
  // OGRCoordinateTransformation is not thread-safe and not trivially copyable.
  // Each clone lazily rebuilds its own transform from source_crs/target_crs.
  transform = nullptr;
}

LASRtransformcrs::~LASRtransformcrs()
{
  if (transform != nullptr)
  {
    OGRCoordinateTransformation::DestroyCT(transform);
    transform = nullptr;
  }
}

bool LASRtransformcrs::set_parameters(const nlohmann::json& stage)
{
  int epsg = stage.value("epsg", 0);
  std::string wkt = stage.value("wkt", "");

  try
  {
    if (epsg > 0)
      target_crs = CRS(epsg, true);
    else if (!wkt.empty())
      target_crs = CRS(wkt, true);
    else
    {
      last_error = "transform_crs requires a valid 'epsg' code or 'wkt' string";
      return false;
    }
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  if (!target_crs.is_valid())
  {
    last_error = "transform_crs: invalid target CRS";
    return false;
  }

  return true;
}

void LASRtransformcrs::set_crs(const CRS& crs)
{
  // The CRS flowing into this stage is the source of the reprojection.
  source_crs = crs;
  // The next stages (and writers) must see the target CRS.
  this->crs = target_crs;
}

bool LASRtransformcrs::build_transform()
{
  if (transform != nullptr) return true;

  if (!source_crs.is_valid())
  {
    last_error = "transform_crs: the source CRS is unknown. Add set_crs() upstream or read files that carry a CRS.";
    return false;
  }

  if (!target_crs.is_valid())
  {
    last_error = "transform_crs: the target CRS is invalid.";
    return false;
  }

  OGRSpatialReference oSourceSRS = source_crs.get_crs();
  OGRSpatialReference oTargetSRS = target_crs.get_crs();

  // Use traditional GIS axis order (x = lon/easting, y = lat/northing) so coordinates
  // are not swapped under modern PROJ authority-compliant axis ordering.
  oSourceSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

  CPLPushErrorHandler(CPLQuietErrorHandler);
  transform = OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS);
  CPLPopErrorHandler();

  if (transform == nullptr)
  {
    last_error = "transform_crs: failed to create a coordinate transformation between the source and target CRS.";
    return false;
  }

  return true;
}

// Reproject a bounding box. The boundary is densified so a curved (non-affine)
// reprojection is bounded correctly rather than only at the four corners.
bool LASRtransformcrs::transform_bbox(double& xmin, double& ymin, double& xmax, double& ymax)
{
  if (!build_transform()) return false;

  // Nothing to do for an empty/unset extent.
  if (xmin > xmax || ymin > ymax) return true;

  const int N = 8; // number of samples per edge
  std::vector<double> xs;
  std::vector<double> ys;
  xs.reserve(4 * (N + 1));
  ys.reserve(4 * (N + 1));

  for (int i = 0; i <= N; ++i)
  {
    double tx = xmin + (xmax - xmin) * i / N;
    double ty = ymin + (ymax - ymin) * i / N;

    xs.push_back(tx);   ys.push_back(ymin); // bottom edge
    xs.push_back(tx);   ys.push_back(ymax); // top edge
    xs.push_back(xmin); ys.push_back(ty);   // left edge
    xs.push_back(xmax); ys.push_back(ty);   // right edge
  }

  std::vector<int> ok(xs.size(), 0);
  transform->Transform((int)xs.size(), xs.data(), ys.data(), nullptr, ok.data());

  double nxmin = std::numeric_limits<double>::max();
  double nymin = std::numeric_limits<double>::max();
  double nxmax = std::numeric_limits<double>::lowest();
  double nymax = std::numeric_limits<double>::lowest();
  bool any = false;

  for (size_t i = 0; i < xs.size(); ++i)
  {
    if (!ok[i]) continue;
    any = true;
    nxmin = std::min(nxmin, xs[i]);
    nymin = std::min(nymin, ys[i]);
    nxmax = std::max(nxmax, xs[i]);
    nymax = std::max(nymax, ys[i]);
  }

  if (!any)
  {
    last_error = "transform_crs: failed to reproject the bounding box.";
    return false;
  }

  xmin = nxmin;
  ymin = nymin;
  xmax = nxmax;
  ymax = nymax;
  return true;
}

void LASRtransformcrs::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  // The source CRS is known once set_crs() has been called by the parser. When it is
  // (the normal case), reproject the coverage extent so downstream stages (e.g. the
  // master raster of rasterize) are sized in the target CRS. Otherwise (e.g. during
  // get_pipeline_info() without files) leave the extent unchanged.
  if (source_crs.is_valid() && target_crs.is_valid())
  {
    if (transform_bbox(xmin, ymin, xmax, ymax))
    {
      this->xmin = xmin;
      this->ymin = ymin;
      this->xmax = xmax;
      this->ymax = ymax;
    }
  }
}

bool LASRtransformcrs::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  if (source_crs.is_valid() && target_crs.is_valid())
  {
    double x0 = chunk.xmin, y0 = chunk.ymin, x1 = chunk.xmax, y1 = chunk.ymax;
    if (!transform_bbox(x0, y0, x1, y1)) return false;

    this->xmin = x0;
    this->ymin = y0;
    this->xmax = x1;
    this->ymax = y1;

    chunk.xmin = x0;
    chunk.ymin = y0;
    chunk.xmax = x1;
    chunk.ymax = y1;
  }

  return true;
}

bool LASRtransformcrs::process(PointCloud*& las)
{
  if (las == nullptr || las->npoints == 0) return true;

  if (!build_transform()) return false;

  AttributeSchema& schema = las->header->schema;

  // Only the horizontal coordinates (X/Y) are reprojected; Z is preserved as-is. This
  // matches gdaltransform/sf/terra, which do not alter heights when reprojecting unless
  // an explicit vertical/compound CRS is involved, and avoids surprising sub-metre
  // datum-driven height shifts. Vertical CRS transformations are out of scope.
  //
  // While reading, get_x()/get_y() decode with the source scale/offset because the schema
  // is not modified until the very end.
  double sx = schema.attributes[AttributeCore::X].scale_factor;
  double sy = schema.attributes[AttributeCore::Y].scale_factor;

  // Pick X/Y scale factors suited to the target CRS. Reusing a projected scale (e.g.
  // 0.01 m) for a geographic target would give ~1 km resolution, while reusing a
  // geographic scale (e.g. 1e-7 deg) for a projected target would overflow the 32-bit
  // stored integers.
  double new_sx = sx, new_sy = sy;
  if (target_crs.is_geographic())
  {
    new_sx = 1e-7; // ~1.1 cm at the equator
    new_sy = 1e-7;
  }
  else if (source_crs.is_geographic())
  {
    new_sx = 0.01; // 1 cm for a projected target coming from degrees
    new_sy = 0.01;
  }
  // else (projected -> projected): keep the existing scale factors.

  // Choose new X/Y offsets near the reprojected data so the stored integers stay small.
  // The center of the (source CRS) bounding box reprojected to the target CRS is a
  // representative point guaranteed to be close to every reprojected coordinate.
  double ox = (las->header->min_x + las->header->max_x) / 2;
  double oy = (las->header->min_y + las->header->max_y) / 2;
  if (!transform->Transform(1, &ox, &oy, nullptr))
  {
    last_error = "transform_crs: failed to reproject the reference offset.";
    return false;
  }

  // Transform in batches: OGRCoordinateTransformation has a non-negligible per-call
  // overhead, so transforming arrays is much faster than one point at a time.
  const size_t BATCH = 65536;
  std::vector<double> xs(BATCH), ys(BATCH);
  std::vector<unsigned char*> ptrs(BATCH);
  std::vector<int> ok(BATCH);

  Point p;
  p.set_schema(&schema);

  bool any_failed = false;

  auto flush = [&](size_t count)
  {
    if (count == 0) return;
    transform->Transform((int)count, xs.data(), ys.data(), nullptr, ok.data());
    for (size_t i = 0; i < count; ++i)
    {
      p.data = ptrs[i];
      if (!ok[i])
      {
        // A point that cannot be reprojected (outside the transform domain) is dropped.
        p.set_deleted();
        any_failed = true;
        continue;
      }
      p.set_X((int)std::lround((xs[i] - ox) / new_sx));
      p.set_Y((int)std::lround((ys[i] - oy) / new_sy));
      // Z is preserved: its raw integer, scale and offset are left unchanged.
    }
  };

  size_t n = 0;
  while (las->read_point())
  {
    xs[n] = las->point.get_x();
    ys[n] = las->point.get_y();
    ptrs[n] = las->point.data;
    n++;
    if (n == BATCH) { flush(n); n = 0; }
  }
  flush(n);

  // Now switch the schema to the target CRS scale/offset. Future decodes of the freshly
  // written integers will therefore yield the reprojected coordinates.
  schema.attributes[AttributeCore::X].scale_factor = new_sx;
  schema.attributes[AttributeCore::Y].scale_factor = new_sy;
  schema.attributes[AttributeCore::X].value_offset = ox;
  schema.attributes[AttributeCore::Y].value_offset = oy;

  // Keep the LAS header X/Y scale/offset in sync (used by the LAS/LAZ writers). The Z
  // scale/offset are left unchanged because Z is preserved.
  las->header->x_scale_factor = new_sx;
  las->header->y_scale_factor = new_sy;
  las->header->x_offset = ox;
  las->header->y_offset = oy;

  // Tag the data with the target CRS.
  las->header->crs = target_crs;

  if (any_failed) las->delete_deleted();

  las->seek(0);
  las->update_header();

  return true;
}
