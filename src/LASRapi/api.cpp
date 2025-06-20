#include <string>
#include <vector>

#include "api.h"

namespace api
{
Pipeline add_attribute(std::string data_type, std::string name, std::string description, double scale, double offset)
{
  std::vector<std::string> choices = {"uchar", "char", "ushort", "short", "uint", "int", "uint64", "int64", "float", "double"};
  auto it = std::find(choices.begin(), choices.end(), data_type);
  if (it == choices.end()) throw std::invalid_argument("Invalid argument 'data_type': " + data_type);

  Stage s("add_attribute");
  s.set("data_type", data_type);
  s.set("name", name);
  s.set("description", description);
  s.set("scale", scale);
  s.set("offset", offset);

  return Pipeline(s);
}

Pipeline add_rgb()
{
  Stage s("add_rgb");

  return Pipeline(s);
}

Pipeline classify_with_sor(int k, int m, int classification)
{
  Stage s("classify_with_sor");
  s.set("k", k);
  s.set("m", m);
  s.set("class", classification);

  return Pipeline(s);
}

Pipeline classify_with_ivf(double res, int n, int classification)
{
  Stage s("classify_with_ivf");
  s.set("res", res);
  s.set("n", n);
  s.set("class", classification);

  return Pipeline(s);
}

Pipeline classify_with_csf(bool slope_smooth, double class_threshold, double cloth_resolution, int rigidness, int iterations, double time_step, int classification, std::vector<std::string> filter)
{
  Stage s("classify_with_csf");
  s.set("slope_smooth", slope_smooth);
  s.set("class_threshold", class_threshold);
  s.set("cloth_resolution", cloth_resolution);
  s.set("iterations", iterations);
  s.set("time_step", time_step);
  s.set("class", classification);
  s.set("filter", filter);

  return Pipeline(s);
}

Pipeline geometry_features(int k, double r, std::string features)
{
  Stage s("svd");
  s.set("k", k);
  s.set("r", r);
  s.set("features", features);

  return Pipeline(s);
}

Pipeline delete_points(std::vector<std::string> filter)
{
  if (filter.size() == 0 || (filter.size() == 1 && filter[0] == ""))
    throw std::invalid_argument("A filter is mandatory in 'delete_points");

  Stage s("filter");
  s.set("filter", filter);

  return Pipeline(s);
}

Pipeline edit_attribute(std::vector<std::string> filter, std::string attribute, double value)
{
  static const std::vector<std::string> coords = {"x", "X", "y", "Y", "z", "Z"};
  auto it = std::find(coords.begin(), coords.end(), attribute);
  if (it != coords.end()) throw std::invalid_argument("Editing point coordinates is not allowed");

  Stage s("edit_attribute");
  s.set("filter", filter);
  s.set("attribute", attribute);
  s.set("value", value);

  return Pipeline(s);
}

Pipeline filter_with_grid(double res, std::string operation, std::vector<std::string> filter)
{
  static const std::vector<std::string> choices = {"min", "max"};
  auto it = std::find(choices.begin(), choices.end(), operation);
  if (it != choices.end()) throw std::invalid_argument("Invalid argument 'operation'. Available options are 'min', 'max'.");

  Stage s("filter_grid");
  s.set("res", res);
  s.set("operator", operation);
  s.set("filter", filter);

  return Pipeline(s);
}

Pipeline focal(std::string connect_uid, double size, std::string fun, std::string ofile)
{
  static const std::vector<std::string> choices = {"mean", "median", "min", "max", "sum"};
  auto it = std::find(choices.begin(), choices.end(), fun);
  if (it == choices.end()) throw std::invalid_argument("Invalid argument 'operation'. Available options are 'mean', 'median', 'min', 'max', 'sum'");

  if (size <= 0) throw std::invalid_argument("Size must be positive");

  Stage s("focal");
  s.set("connect", connect_uid);
  s.set("size", size);
  s.set("fun", fun);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline hull(std::string ofile)
{
  Stage s("hulls");
  s.set("output", ofile);

  return Pipeline(s);
}

Pipeline hull_triangulation(std::string connect_uid, std::string ofile)
{
  Stage s("hulls");
  s.set("output", ofile);
  s.set("connect", connect_uid);

  return Pipeline(s);
}

Pipeline info()
{
  Stage s("info");

  return Pipeline(s);
}

Pipeline load_raster(std::string file, int band)
{
  Stage s("load_raster");
  s.set("file", file);
  s.set("band", band);
  s.set_raster();

  return Pipeline(s);
}

Pipeline load_matrix(std::vector<double> matrix, bool check)
{
  if (matrix.size() != 16) throw std::invalid_argument("'matrix' is not a 4x4 matrix");

  Stage s("load_matrix");
  s.set("matrix", matrix);
  s.set("check", check);
  s.set_matrix();

  return Pipeline(s);
}

Pipeline local_maximum(double ws, double min_height, std::vector<std::string> filter, std::string ofile, std::string use_attribute, bool record_attributes)
{
  Stage s("local_maximum");
  s.set("ws", ws);
  s.set("min_height", min_height);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set("use_attribute", use_attribute);
  s.set("record_attributes", record_attributes);
  s.set_vector();

  return Pipeline(s);
}

Pipeline local_maximum_raster(std::string connect_uid, double ws, double min_height, std::vector<std::string> filter, std::string ofile)
{
  Stage s("local_maximum");
  s.set("connect", connect_uid);
  s.set("ws", ws);
  s.set("min_height", min_height);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set_vector();

  return Pipeline(s);
}

Pipeline pit_fill(std::string connect_uid, int lap_size, double thr_lap, double thr_spk, int med_size, int dil_radius, std::string ofile)
{
  Stage s("pit_fill");
  s.set("connect", connect_uid);
  s.set("lap_size", lap_size);
  s.set("thr_lap", thr_lap);
  s.set("med_size", med_size);
  s.set("thr_spk", thr_spk);
  s.set("dil_radius", dil_radius);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline rasterize(double res, double window, std::vector<std::string> operators, std::vector<std::string> filter, std::string ofile, double default_value)
{
  Stage s("rasterize");
  s.set("res", res);
  s.set("window", window);
  s.set("method", operators);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set_raster();

  if (default_value != -99999)
  {
    s.set("default_value", default_value);
  }

  return Pipeline(s);
}

Pipeline rasterize_triangulation(std::string connect_uid, double res, std::string ofile)
{
  Stage s("rasterize");
  s.set("res", res);
  s.set("connect", connect_uid);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline reader_coverage(std::vector<std::string> filter, std::string select, int copc_depth)
{
  Stage s("reader");

  if (copc_depth >= 0)
    filter.push_back("-max_depth " + std::to_string(copc_depth));

  s.set("filter", filter);

  return Pipeline(s);
}

Pipeline reader_circles(std::vector<double> xc, std::vector<double> yc, std::vector<double> r, std::vector<std::string> filter, std::string select, int copc_depth)
{
  if (xc.size() != yc.size())
    throw std::invalid_argument("xc and yc must have the same length");

  if (r.size() == 1)
  {
    r = std::vector<double>(xc.size(), r[0]); // replicate r[0] to match xc size
  }
  else if (r.size() > 1)
  {
    if (xc.size() != r.size())
      throw std::invalid_argument("xc and r must have the same length when r has more than one value");
  }

  if (copc_depth >= 0)
    filter.push_back("-max_depth " + std::to_string(copc_depth));

  Stage s("reader");
  s.set("filter", filter);
  s.set("xcenter", xc);
  s.set("ycenter", yc);
  s.set("radius", r);

  return Pipeline(s);
}

Pipeline reader_rectangles(std::vector<double> xmin, std::vector<double> ymin, std::vector<double> xmax, std::vector<double> ymax, std::vector<std::string> filter, std::string select, int copc_depth)
{
  size_t n = xmin.size();
  if (ymin.size() != n || xmax.size() != n || ymax.size() != n)
    throw std::invalid_argument("xmin, ymin, xmax, and ymax must all have the same length");

  if (copc_depth >= 0)
    filter.push_back("-max_depth " + std::to_string(copc_depth));

  Stage s("reader");
  s.set("filter", filter);
  s.set("xmin", xmin);
  s.set("xmax", xmax);
  s.set("ymin", ymin);
  s.set("ymax", ymax);

  return Pipeline(s);
}

Pipeline region_growing(std::string connect_uid_raster, std::string connect_uid_seeds, double th_tree, double th_seed, double th_cr, double max_cr, std::string ofile)
{
  Stage s("region_growing");
  s.set("connect1", connect_uid_raster);
  s.set("connect2", connect_uid_seeds);
  s.set("th_tree", th_tree);
  s.set("th_seed", th_seed);
  s.set("th_cr", th_cr);
  s.set("max_cr", max_cr);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline remove_attribute(std::string name)
{
  static const std::vector<std::string> choices = {"x", "X", "y", "Y", "z", "Z"};
  auto it = std::find(choices.begin(), choices.end(), name);
  if (it != choices.end()) throw std::invalid_argument("Removing point coordinates is not allowed");

  Stage s("pit_fill");
  s.set("name", name);

  return Pipeline(s);
}

Pipeline set_crs_epsg(int epsg)
{
  Stage s("set_crs");
  s.set("epsg", epsg);
  s.set("wkt", "");

  return Pipeline(s);
}

Pipeline set_crs_wkt(std::string wkt)
{
  Stage s("set_crs");
  s.set("epsg", 0);
  s.set("wkt", wkt);

  return Pipeline(s);
}

Pipeline sampling_voxel(double res, std::vector<std::string> filter, std::string method, int shuffle_size)
{
  static const std::vector<std::string> choices = {"random"};
  auto it = std::find(choices.begin(), choices.end(), method);
  if (it == choices.end()) throw std::invalid_argument("Invalid argument 'method'. Available options are 'ramdom'");

  Stage s("sampling_voxel");
  s.set("res", res);
  s.set("filter", filter);
  s.set("method", method);
  s.set("shuffle_size", shuffle_size);

  return Pipeline(s);
}

Pipeline sampling_pixel(double res,  std::vector<std::string> filter, std::string method, std::string use_attribute, int shuffle_size)
{
  static const std::vector<std::string> choices = {"random", "max", "min"};
  auto it = std::find(choices.begin(), choices.end(), method);
  if (it == choices.end()) throw std::invalid_argument("Invalid argument 'method'. Available options are 'ramdom', 'min', 'max'.");

  Stage s("sampling_voxel");
  s.set("res", res);
  s.set("filter", filter);
  s.set("method", method);
  s.set("use_attribute", use_attribute);
  s.set("shuffle_size", shuffle_size);

  return Pipeline(s);
}

Pipeline sampling_poisson(double distance,  std::vector<std::string> filter, int shuffle_size)
{
  Stage s("sampling_poisson");
  s.set("distance", distance);
  s.set("filter", filter);
  s.set("shuffle_size", shuffle_size);

  return Pipeline(s);
}

Pipeline stop_if_outside(double xmin, double ymin, double xmax, double ymax)
{
  Stage s("stop_if");
  s.set("condition", "outside_bbox");
  s.set("xmin", xmin);
  s.set("xmax", xmax);
  s.set("ymin", ymin);
  s.set("ymax", ymax);

  return Pipeline(s);
}

Pipeline stop_if_chunk_id_below(int index)
{
  Stage s("stop_if");
  s.set("condition", "chunk_id_below");
  s.set("index", index);

  return Pipeline(s);
}

Pipeline sort_points(bool spatial)
{
  Stage s("sort");
  s.set("spatial", spatial);

  return Pipeline(s);
}

Pipeline summarise(double zwbin, double iwbin, std::vector<std::string> metrics,  std::vector<std::string> filter)
{
  Stage s("summarise");
  s.set("zwbin", zwbin);
  s.set("iwbin", iwbin);
  s.set("filter", filter);
  if (metrics.size() > 0)
    s.set("metrics", metrics);

  return Pipeline(s);
}

Pipeline triangulate(double max_edge, std::vector<std::string> filter, std::string ofile, std::string use_attribute)
{
  Stage s("triangulate");
  s.set("max_edge", max_edge);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set("use_attribute", use_attribute);
  s.set_vector();

  return Pipeline(s);
}

Pipeline transform_with(std::string connect_uid, std::string operation, std::string store_in_attribute, bool bilinear)
{
  Stage s("transform_with");
  s.set("connect", connect_uid);
  s.set("operator", operation);
  s.set("bilinear", bilinear);
  s.set("store_in_attribute", store_in_attribute);

  return Pipeline(s);
}

Pipeline write_las(std::string ofile, std::vector<std::string> filter, bool keep_buffer)
{
  Stage s("write_las");
  s.set("output", ofile);
  s.set("filter", filter);
  s.set("keep_buffer", keep_buffer);

  return Pipeline(s);
}

Pipeline write_copc(std::string ofile, std::vector<std::string> filter, bool keep_buffer, int max_depth, std::string density)
{
  std::string ext = ofile.substr(ofile.size()-8, ofile.size());
  if (ext != ".copc.laz") throw std::invalid_argument("File must be .copc.laz");

  static const std::vector<std::string> choices = {"sparse", "normal", "dense", "denser"};
  auto it = std::find(choices.begin(), choices.end(), density);
  if (it == choices.end()) throw std::invalid_argument("Invalid argument 'density'. Available options are 'sparse'', 'normal', 'dense', 'denser'");

  int d = 256;
  if (density == "sparse") d = 64;
  if (density == "normal") d = 128;
  if (density == "dense")  d = 256;
  if (density == "denser") d = 512;


  Stage s("write_las");
  s.set("output", ofile);
  s.set("filter", filter);
  s.set("keep_buffer", keep_buffer);
  s.set("density", d);
  if (max_depth >= 0)
    s.set("max_depth", max_depth);

  return Pipeline(s);
}

Pipeline write_pcd(std::string ofile, bool binary)
{
  Stage s("write_pcd");
  s.set("output", ofile);
  s.set("binary", binary);

  return Pipeline(s);
}

Pipeline write_vpc(std::string ofile, bool absolute_path, bool use_gpstime)
{
  Stage s("write_vpc");
  s.set("output", ofile);
  s.set("absolute_path", absolute_path);
  s.set("use_gpstime", use_gpstime);

  return Pipeline(s);
}

Pipeline write_lax(bool embedded, bool overwrite)
{
  Stage s("write_lax");
  s.set("embedded", embedded);
  s.set("overwrite", overwrite);

  return Pipeline(s);
}

#ifdef USING_R
Pipeline aggregate(double res, int nmetrics, double window, std::string call_ptr, std::string env_ptr, std::vector<std::string> filter, std::string ofile)
{
  Stage s("aggregate");
  s.set("res", res);
  s.set("nmetrics", nmetrics);
  s.set("window", window);
  s.set("call", call_ptr);
  s.set("env", env_ptr);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline callback(std::string fun_ptr, std::string args_ptr, std::string expose, bool drop_buffer, bool no_las_update)
{
  Stage s("callback");
  s.set("fun", fun_ptr);
  s.set("args", args_ptr);
  s.set("expose", expose);
  s.set("drop_buffer", drop_buffer);
  s.set("no_las_update", no_las_update);

  return Pipeline(s);
}
#endif

} // namespace api

namespace nonapi
{

api::Pipeline nothing(bool read, bool stream, bool loop)
{
  api::Stage s("nothing");
  s.set("read", read);
  s.set("stream", stream);
  s.set("loop", loop);

  return api::Pipeline(s);
}

api::Pipeline neighborhood_metrics(std::string connect_uid, std::vector<std::string> metrics, int k, double r, std::string ofile)
{
  api::Stage s("neighborhood_metrics");
  s.set("connect", connect_uid);
  s.set("k", k);
  s.set("r", r);
  s.set("metrics", metrics);
  s.set("output", ofile);

  return api::Pipeline(s);
}


} // namespace api
