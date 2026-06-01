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

void LASRtransformcrs::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  // The source CRS is known once set_crs() has been called by the parser. When it is
  // (the normal case), reproject the coverage extent so downstream stages (e.g. the
  // master raster of rasterize) are sized in the target CRS. Otherwise (e.g. during
  // get_pipeline_info() without files) leave the extent unchanged.
  if (source_crs.is_valid() && target_crs.is_valid())
  {
    if (reproject_bbox(source_crs, target_crs, xmin, ymin, xmax, ymax))
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
    const double sxmin = chunk.xmin, symin = chunk.ymin, sxmax = chunk.xmax, symax = chunk.ymax;
    double x0 = sxmin, y0 = symin, x1 = sxmax, y1 = symax;

    if (reproject_bbox(source_crs, target_crs, x0, y0, x1, y1))
    {
      // The tile buffer is a distance expressed in the source CRS units. Scale it to the
      // target CRS by the local linear factor of the transform (ratio of the reprojected
      // diagonal to the source diagonal) so downstream buffer-consuming stages (rasterize,
      // focal, triangulate, ...) don't combine a source-unit buffer with a target-unit
      // resolution (e.g. a 50 m buffer interpreted as 50 degrees, or vice versa).
      const double src_diag = std::hypot(sxmax - sxmin, symax - symin);
      const double tgt_diag = std::hypot(x1 - x0, y1 - y0);
      if (src_diag > 0 && tgt_diag > 0)
      {
        chunk.buffer *= tgt_diag / src_diag;
        buffer = chunk.buffer;
      }

      this->xmin = x0;
      this->ymin = y0;
      this->xmax = x1;
      this->ymax = y1;

      chunk.xmin = x0;
      chunk.ymin = y0;
      chunk.xmax = x1;
      chunk.ymax = y1;
    }
    else
    {
      // The whole chunk extent is outside the transformation domain. Do not abort the run:
      // process() drops the individual out-of-domain points and keeps the rest. Leave the
      // chunk extent unreprojected (it produces no output anyway) and warn.
      warning("transform_crs: could not reproject a chunk extent (outside the transformation domain).\n");
    }
  }

  return true;
}

bool LASRtransformcrs::process(PointCloud*& las)
{
  if (las == nullptr || las->npoints == 0) return true;

  if (!build_transform()) return false;

  AttributeSchema& schema = las->header->schema;
  Attribute& attr_x = schema.attributes[AttributeCore::X];
  Attribute& attr_y = schema.attributes[AttributeCore::Y];

  // get_x()/get_y() (and the writers) only understand INT32, FLOAT and DOUBLE for the
  // core coordinates; any other storage type decodes to 0. Refuse those rather than
  // silently corrupting the points.
  auto is_supported = [](AttributeType t)
  { return t == AttributeType::INT32 || t == AttributeType::FLOAT || t == AttributeType::DOUBLE; };
  if (!is_supported(attr_x.type) || !is_supported(attr_y.type))
  {
    last_error = "transform_crs: unsupported X/Y storage type (expected int, float or double).";
    return false;
  }

  // LAS stores X/Y as scaled 32-bit integers (scale/offset matter); PCD and other formats
  // may store them as float/double, which carry the coordinate directly and ignore
  // scale/offset (see Point::get_core_attribute_as_double). Both cases are handled below.
  const bool x_int = (attr_x.type == AttributeType::INT32);
  const bool y_int = (attr_y.type == AttributeType::INT32);

  // Only the horizontal coordinates (X/Y) are reprojected; Z is preserved as-is. This
  // matches gdaltransform/sf/terra, which do not alter heights when reprojecting unless
  // an explicit vertical/compound CRS is involved. Vertical CRS transformations are out of
  // scope. While reading, get_x()/get_y() decode with the source scale/offset because the
  // schema is not modified until the very end.

  // Pick X/Y scale factors suited to the target CRS. Reusing a projected scale (e.g. 0.01 m)
  // for a geographic target would give ~1 km resolution, while reusing a geographic scale
  // (e.g. 1e-7 deg) for a projected target would overflow the 32-bit stored integers. These
  // apply to INT32 storage directly and, for float/double storage, are used by write_las()
  // when it quantizes the coordinates to LAS int32.
  //
  // For an INT32 source the schema scale is the real LAS quantization step, so it is reused for
  // a projected target. For a float/double source (e.g. PCD) the schema scale is a placeholder
  // (typically 1.0) that is meaningless as a quantization step, so a target-appropriate scale is
  // always chosen -- otherwise a projected->projected transform would write LAS at whole-unit
  // resolution.
  double new_sx;
  double new_sy;
  if (target_crs.is_geographic())
  {
    new_sx = new_sy = 1e-7; // ~1.1 cm at the equator
  }
  else if (x_int && y_int && !source_crs.is_geographic())
  {
    // Projected -> projected with real (INT32) scales: keep them.
    new_sx = attr_x.scale_factor;
    new_sy = attr_y.scale_factor;
  }
  else
  {
    // Projected target from a geographic source, or float/double storage whose schema scale is
    // a placeholder: 1 cm is a sensible default resolution for a projected CRS.
    new_sx = new_sy = 0.01;
  }

  // Choose X/Y offsets near the reprojected data so the stored/written integers stay small.
  // Needed for INT32 storage and ALSO for float/double storage: write_las() quantizes to LAS
  // int32 using the scale/offset recorded on the header, so a representative offset must be
  // computed for every storage type even though in-memory float/double decoding ignores it.
  // The reprojected
  // center of the source bounding box is the natural choice, but it can itself fall outside the
  // transform domain (e.g. data straddling a projection or UTM-zone boundary, so the centroid
  // is undefined) even when the points are fine. Probe the center, then the bbox corners and
  // edge midpoints, and use the first that reprojects. If none do, fall back to 0 (every point
  // will be dropped below anyway). Never abort the stage here.
  double ox = 0.0;
  double oy = 0.0;
  {
    const double mnx = las->header->min_x, mxx = las->header->max_x;
    const double mny = las->header->min_y, mxy = las->header->max_y;
    const double cx = (mnx + mxx) / 2, cy = (mny + mxy) / 2;
    const double cand_x[] = { cx, mnx, mxx, mnx, mxx, cx,  cx,  mnx, mxx };
    const double cand_y[] = { cy, mny, mny, mxy, mxy, mny, mxy, cy,  cy  };
    for (size_t i = 0; i < sizeof(cand_x) / sizeof(cand_x[0]); ++i)
    {
      double tx = cand_x[i], ty = cand_y[i];
      if (transform->Transform(1, &tx, &ty, nullptr)) { ox = tx; oy = ty; break; }
    }
  }

  // Transform in batches: OGRCoordinateTransformation has a non-negligible per-call
  // overhead, so transforming arrays is much faster than one point at a time.
  const size_t BATCH = 65536;
  std::vector<double> xs(BATCH), ys(BATCH);
  std::vector<unsigned char*> ptrs(BATCH);
  std::vector<int> ok(BATCH);

  size_t n_outside = 0; // dropped: outside the transformation domain
  size_t n_range = 0;   // dropped: not representable as a 32-bit integer

  // Store one reprojected coordinate honoring the storage type. Returns false if an INT32
  // coordinate would overflow the 32-bit range (so the point can be dropped rather than
  // silently wrapping to a garbage location).
  auto store = [](unsigned char* base, const Attribute& a, bool is_int, double value, double new_s, double off) -> bool
  {
    unsigned char* ptr = base + a.offset;
    if (is_int)
    {
      long raw = std::lround((value - off) / new_s);
      if (raw > std::numeric_limits<int>::max() || raw < std::numeric_limits<int>::min()) return false;
      *reinterpret_cast<int*>(ptr) = static_cast<int>(raw);
    }
    else if (a.type == AttributeType::FLOAT)
      *reinterpret_cast<float*>(ptr) = static_cast<float>(value);
    else // DOUBLE
      *reinterpret_cast<double*>(ptr) = value;
    return true;
  };

  Point p;
  p.set_schema(&schema);

  auto flush = [&](size_t count)
  {
    if (count == 0) return;
    transform->Transform((int)count, xs.data(), ys.data(), nullptr, ok.data());
    for (size_t i = 0; i < count; ++i)
    {
      p.data = ptrs[i];
      if (!ok[i])
      {
        // A point outside the transform domain is dropped.
        p.set_deleted();
        n_outside++;
        continue;
      }
      bool ok_x = store(ptrs[i], attr_x, x_int, xs[i], new_sx, ox);
      bool ok_y = store(ptrs[i], attr_y, y_int, ys[i], new_sy, oy);
      if (!ok_x || !ok_y)
      {
        p.set_deleted();
        n_range++;
        continue;
      }
      // Z is preserved: its raw value, scale and offset are left unchanged.
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

  // Record the target CRS scale/offset on the lasR header for BOTH axes regardless of storage
  // type: write_las() quantizes the reprojected coordinates to LAS int32 using these, so they
  // must be sized for the target CRS even for float/double sources (otherwise sub-unit lon/lat
  // collapses under the default 1.0 scale factor when written to LAS).
  las->header->x_scale_factor = new_sx;
  las->header->y_scale_factor = new_sy;
  las->header->x_offset = ox;
  las->header->y_offset = oy;

  // The in-memory schema scale/offset are only meaningful for INT32 storage (LAS), where the
  // stored integers must decode to the reprojected coordinates. For float/double storage (PCD)
  // the coordinate is stored directly and every in-memory accessor (get_x, AttributeAccessor,
  // the kd-tree) expects identity scale/offset, so the schema is left untouched; the LAS writer
  // reads the header scale/offset for those axes instead.
  if (x_int)
  {
    attr_x.scale_factor = new_sx;
    attr_x.value_offset = ox;
  }
  if (y_int)
  {
    attr_y.scale_factor = new_sy;
    attr_y.value_offset = oy;
  }

  // Tag the data with the target CRS.
  las->header->crs = target_crs;

  const size_t n_dropped = n_outside + n_range;
  if (n_dropped > 0) las->delete_deleted();

  las->seek(0);
  las->update_header();

  if (las->npoints == 0)
  {
    // Every point fell outside the target CRS domain. update_header() leaves the bounding
    // box at its inverted sentinels (min > max) for an empty cloud; reset it to a benign
    // value so the empty result is not propagated downstream as a corrupt extent.
    las->header->min_x = las->header->max_x = ox;
    las->header->min_y = las->header->max_y = oy;
    las->header->min_z = las->header->max_z = 0.0;
    warning("transform_crs: all %lu point(s) fell outside the target CRS domain; the output is empty.\n", (unsigned long)n_dropped);
  }
  else if (n_dropped > 0)
  {
    if (n_range > 0)
      warning("transform_crs: dropped %lu point(s) outside the transformation domain and %lu point(s) not representable in the target CRS.\n", (unsigned long)n_outside, (unsigned long)n_range);
    else
      warning("transform_crs: dropped %lu point(s) outside the transformation domain.\n", (unsigned long)n_outside);
  }

  return true;
}
