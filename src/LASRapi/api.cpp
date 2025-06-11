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
  if (it != choices.end()) throw std::invalid_argument("Invalid argument 'operation'. Available options are 'mean', 'median', 'min', 'max', 'sum'");

  if (size <= 0) throw std::invalid_argument("Size must be positive");

  Stage s("focal");
  s.set("connect", connect_uid);
  s.set("fun", fun);
  s.set("output", ofile);
  s.set_raster();

  return Pipeline(s);
}

Pipeline hull(std::string connect_uid, std::string ofile)
{
  Stage s("hulls");
  s.set("output", ofile);
  if (connect_uid != "")
    s.set("connect", connect_uid);

  return Pipeline(s);
}

Pipeline transform_with(std::string connect_uid, std::string operation, std::string store_in_attribute, bool bilinear)
{
  Stage s("transform_with");
  s.set("connect", connect_uid);
  s.set("operator", operation);
  s.set("bilinear", bilinear);

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
  Stage s("local_maximum_raster");
  s.set("connect", connect_uid);
  s.set("ws", ws);
  s.set("min_height", min_height);
  s.set("filter", filter);
  s.set("output", ofile);
  s.set_vector();

  return Pipeline(s);
}

Pipeline nothing(bool read, bool stream, bool loop)
{
  Stage s("nothing");
  s.set("read", read);
  s.set("stream", stream);
  s.set("loop", loop);

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
  if (it != choices.end()) throw std::invalid_argument("Invalid argument 'density'. Available options are 'sparse'', 'normal', 'dense', 'denser'");

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

} // namespace api
