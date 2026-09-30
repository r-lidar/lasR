#include "transformcrs.h"

#include <ogr_spatialref.h>

#include <cmath>
#include <vector>
#include <limits>
#include <algorithm>

LASRtransformcrs::LASRtransformcrs()
{
  transform = nullptr;
  target_to_source_buffer_scale = 1.0;
  target_to_source_buffer_scale_valid = false;
  data_units_buffer_scale = 1.0;
  data_units_buffer_scale_valid = false;
}

LASRtransformcrs::LASRtransformcrs(const LASRtransformcrs& other) : Stage(other)
{
  source_crs = other.source_crs;
  target_crs = other.target_crs;
  target_to_source_buffer_scale = other.target_to_source_buffer_scale;
  target_to_source_buffer_scale_valid = other.target_to_source_buffer_scale_valid;
  data_units_buffer_scale = other.data_units_buffer_scale;
  data_units_buffer_scale_valid = other.data_units_buffer_scale_valid;
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

// Largest number of source units needed to cover one target unit, in any direction, over the
// extent xmin..ymax of the source CRS. At each point of a grid covering the extent, the Jacobian J
// of the source -> target transformation is estimated by finite differences. A target
// displacement (dx, dy) comes from the source displacement J^-1 (dx, dy), so a source box
// expanded by b on each side covers a target box expanded by d on each side if b >= d * |J^-1|,
// with |.| the maximum absolute row sum norm. It covers the smaller target disc of radius d too.
// Returns 0 if the scale cannot be estimated.
static double max_source_units_per_target_unit(OGRCoordinateTransformation* ct, double xmin, double ymin, double xmax, double ymax, bool source_is_geographic)
{
  const int n = 5; // n x n samples

  double step = std::max(xmax - xmin, ymax - ymin) * 1e-3;
  if (!(step > 0)) step = source_is_geographic ? 1e-6 : 0.1;

  std::vector<double> xs, ys;
  for (int i = 0; i < n; ++i)
  {
    for (int j = 0; j < n; ++j)
    {
      double x = xmin + (xmax - xmin) * i / (n - 1);
      double y = ymin + (ymax - ymin) * j / (n - 1);
      xs.push_back(x);        ys.push_back(y);
      xs.push_back(x + step); ys.push_back(y);
      xs.push_back(x);        ys.push_back(y + step);
    }
  }

  std::vector<int> ok(xs.size(), 0);
  ct->Transform((int)xs.size(), xs.data(), ys.data(), nullptr, ok.data());

  double scale = 0;
  for (size_t k = 0; k < xs.size(); k += 3)
  {
    if (!ok[k] || !ok[k+1] || !ok[k+2]) continue;

    const double a = (xs[k+1] - xs[k]) / step; // d x_target / d x_source
    const double b = (xs[k+2] - xs[k]) / step; // d x_target / d y_source
    const double c = (ys[k+1] - ys[k]) / step; // d y_target / d x_source
    const double d = (ys[k+2] - ys[k]) / step; // d y_target / d y_source
    const double det = a * d - b * c;
    if (!std::isfinite(det) || det == 0) continue;

    // J^-1 = [d -b; -c a] / det
    const double s = std::max(std::fabs(d) + std::fabs(b), std::fabs(c) + std::fabs(a)) / std::fabs(det);
    if (std::isfinite(s)) scale = std::max(scale, s);
  }

  return scale;
}

void LASRtransformcrs::get_extent(double& xmin, double& ymin, double& xmax, double& ymax)
{
  // The source CRS is known once set_crs() has been called by the parser. When it is
  // (the normal case), reproject the coverage extent so downstream stages (e.g. the
  // master raster of rasterize) are sized in the target CRS. Otherwise (e.g. during
  // get_pipeline_info() without files) leave the extent unchanged.
  if (source_crs.is_valid() && target_crs.is_valid())
  {
    const double sxmin = xmin, symin = ymin, sxmax = xmax, symax = ymax;
    if (reproject_bbox(source_crs, target_crs, xmin, ymin, xmax, ymax))
    {
      const double src_diag = std::hypot(sxmax - sxmin, symax - symin);
      const double tgt_diag = std::hypot(xmax - xmin, ymax - ymin);
      if (src_diag > 0 && tgt_diag > 0)
      {
        target_to_source_buffer_scale = src_diag / tgt_diag;
        target_to_source_buffer_scale_valid = true;
      }

      if (build_transform())
      {
        // Add 1% to account for the variations of the scale between the samples.
        double scale = max_source_units_per_target_unit(transform, sxmin, symin, sxmax, symax, source_crs.is_geographic());
        if (scale > 0)
        {
          data_units_buffer_scale = scale * 1.01;
          data_units_buffer_scale_valid = true;
        }
      }

      this->xmin = xmin;
      this->ymin = ymin;
      this->xmax = xmax;
      this->ymax = ymax;
    }
  }
}

double LASRtransformcrs::translate_buffer_to_input(double downstream_buffer, bool data_units) const
{
  // A buffer in data units (a resolution, a window size...) is a distance in the target CRS, in
  // any direction. Express it in source units with the largest local ratio between the two CRS
  // so the reader loads at least that distance in every direction, whatever the direction of the
  // transformation (projected <-> geographic, or between two projected CRS).
  if (data_units)
  {
    if (!data_units_buffer_scale_valid) return downstream_buffer;
    return downstream_buffer * data_units_buffer_scale;
  }

  if (!target_to_source_buffer_scale_valid) return downstream_buffer;

  // Fixed-distance stages after a projected -> geographic reprojection still ask for a
  // physical halo (e.g. triangulate()'s 20 source metres). The transform stage converts
  // that halo to target degrees in set_chunk(), so the reader-side buffer should remain
  // in the projected source units here. In the inverse direction, and for projected CRSs
  // with different local units/scales, convert the target-side halo back to source units
  // so the reader does not request an enormous geographic buffer or under-read a projected
  // one. set_chunk() applies the opposite conversion before downstream stages consume it.
  if (source_crs.is_geographic() && target_crs.is_geographic())
    return downstream_buffer;

  if (target_crs.is_geographic() && !source_crs.is_geographic())
    return downstream_buffer;

  return downstream_buffer * target_to_source_buffer_scale;
}

bool LASRtransformcrs::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  if (source_crs.is_valid() && target_crs.is_valid())
  {
    const double sxmin = chunk.xmin, symin = chunk.ymin, sxmax = chunk.xmax, symax = chunk.ymax;
    double x0 = sxmin, y0 = symin, x1 = sxmax, y1 = symax;

    // A circle is not a circle in the target CRS: bound the reprojected circle instead.
    const bool is_circle = chunk.shape == ShapeType::CIRCLE;
    bool reprojected;
    if (is_circle)
      reprojected = reproject_circle_bbox(source_crs, target_crs, (sxmin + sxmax) / 2, (symin + symax) / 2, (sxmax - sxmin) / 2, x0, y0, x1, y1);
    else
      reprojected = reproject_bbox(source_crs, target_crs, x0, y0, x1, y1);

    if (reprojected)
    {
      // The reader consumes the chunk buffer in source coordinates. Downstream stages
      // consume it after the coordinates have been transformed, so convert the source-side
      // halo to target units before passing the chunk along.
      const double src_diag = std::hypot(sxmax - sxmin, symax - symin);
      const double tgt_diag = std::hypot(x1 - x0, y1 - y0);
      if (src_diag > 0 && tgt_diag > 0 &&
          !(source_crs.is_geographic() && target_crs.is_geographic()))
      {
        chunk.buffer *= tgt_diag / src_diag;
      }
      buffer = chunk.buffer;

      // The reprojected chunk is not an axis-aligned rectangle, so the core of the chunk handed to
      // the next stages is its bounding box. It also covers slivers of the neighbouring chunks.
      // The points of these slivers were read as buffer points and flagged as such by the reader
      // in the source CRS: stages that remove the buffer (write_las, summarise, callback) honor this
      // flag first, and only fall back to a geometric test against this box. That test must never
      // exclude a core point, so the box contains the whole reprojected core, plus a margin for the
      // rounding of the reprojected coordinates stored as scaled integers in process(): half a
      // quantization step, i.e. 5e-8 for a geographic target and at most 0.01 for a projected
      // target with a scale factor up to 0.02.
      const double margin = target_crs.is_geographic() ? 1e-7 : 0.01;
      x0 -= margin;
      y0 -= margin;
      x1 += margin;
      y1 += margin;

      this->xmin = x0;
      this->ymin = y0;
      this->xmax = x1;
      this->ymax = y1;

      chunk.xmin = x0;
      chunk.ymin = y0;
      chunk.xmax = x1;
      chunk.ymax = y1;

      // The next stages work on this rectangle. The exact source circle is carried by the buffer
      // flag of the points.
      if (is_circle) chunk.shape = ShapeType::RECTANGLE;
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

  // A float (e.g. PCD TYPE F SIZE 4) has 24 bits of precision: 0.5 to 1 m for projected
  // coordinates of millions of metres, and about 1 m for longitudes. It cannot hold reprojected
  // coordinates, so float X/Y are promoted to double before storing the reprojected values.
  if (attr_x.type == AttributeType::FLOAT && !las->promote_float_to_double(AttributeCore::X)) return false;
  if (attr_y.type == AttributeType::FLOAT && !las->promote_float_to_double(AttributeCore::Y)) return false;

  // LAS stores X/Y as scaled 32-bit integers (scale/offset matter); PCD and other formats
  // may store them as double, which carry the coordinate directly and ignore
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
      const double scaled = (value - off) / new_s;
      const double max_i = static_cast<double>(std::numeric_limits<int>::max()) + 0.5;
      const double min_i = static_cast<double>(std::numeric_limits<int>::min()) - 0.5;
      if (!std::isfinite(scaled) || scaled > max_i || scaled < min_i) return false;
      long long raw = std::llround(scaled);
      if (raw > std::numeric_limits<int>::max() || raw < std::numeric_limits<int>::min()) return false;
      *reinterpret_cast<int*>(ptr) = static_cast<int>(raw);
    }
    else // DOUBLE (float X/Y were promoted to double above)
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
