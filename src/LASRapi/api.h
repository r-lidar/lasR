#ifndef API_H
#define API_H

#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <variant>
#include <nlohmann/json.hpp>

#ifdef USING_R
#include <R.h>
#include <Rinternals.h>
#endif

namespace api
{
// MODIFY THIS FOR A PYTHON API
#if defined(USING_R)
using ReturnType = SEXP;
#elif defined(USING_PYTHON)
using ReturnType = bool;
#else
using ReturnType = bool;
#endif

void lasfilterusage();
void lastransformusage();
bool is_indexed(std::string);

class Stage
{
public:
  using Value = std::variant<
    int,
    double,
    bool,
    std::string,
    std::vector<int>,
    std::vector<double>,
    std::vector<std::string>>;

public:
  Stage(const std::string& algoname);

  void set(const std::string& key, const Value& value);
  bool has(const std::string& key) const;
  Value get(const std::string& key) const;
  nlohmann::json to_json() const;
  std::string get_name() const;
  std::string get_uid() const;
  std::string to_string() const;
  void set_raster() { raster = true;  matrix = false; vector = false; };
  void set_matrix() { raster = false; matrix = true;  vector = false;  };
  void set_vector() { raster = false; matrix = false; vector = true;  };
  bool is_raster() const { return raster; };
  bool is_matrix() const { return matrix; };
  bool is_vector() const { return vector; };

private:
  bool raster = false;
  bool matrix = false;
  bool vector = false;
  std::unordered_map<std::string, Value> attributes;

private:
  std::string generate_uid(int size = 8);
};

class Pipeline
{
public:
  Pipeline() = default;
  Pipeline(const Stage& s);

  Pipeline operator+(const Stage& s) const;
  Pipeline operator+(const Pipeline& other) const;
  Pipeline& operator+=(const Stage& s);
  Pipeline& operator+=(const Pipeline& other);

  void set_files(const std::vector<std::string>& files) { this->files = files; };
  void set_ncores(int n) { if (n > 0) opt_ncores = n; };
  void set_strategy();
  void set_verbose(bool b) { opt_verbose = b; };
  void set_buffer(double val) { opt_buffer = val; };
  void set_progress(bool b) { opt_progress = b; };
  void set_chunk(double val) { if(val> 0) opt_chunk = val; };
  void set_profile_file(const std::string& path) { opt_profiling_file = path; };

  std::string to_string() const;
  std::string write_json(const std::string& path = "") const;
  nlohmann::json generate_json() const;

private:
  bool has_reader() const;

private:
  std::list<Stage> stages;

  // Processing options
  std::vector<std::string> files;
  int opt_ncores = 1;
  std::string opt_strategy  = "concurrent-points";
  double opt_buffer = 0;
  bool opt_progress = false;
  double opt_chunk = 0;
  bool opt_verbose = false;
  std::string opt_profiling_file = "";
};

Pipeline add_attribute(std::string data_type, std::string name, std::string description, double scale = 1, double offset = 0);
Pipeline add_rgb();
Pipeline classify_with_sor(int k = 8, int m = 6, int classification = 18);
Pipeline classify_with_ivf(double res = 5, int n = 6, int classificatiob = 18);
Pipeline geometry_features(int k, double r, std::string features = "");
Pipeline delete_points(std::vector<std::string> filter = {""});
Pipeline edit_attribute(std::vector<std::string> filter = {""}, std::string attribute = "", double value = 0);
Pipeline filter_with_grid(double res, std::string operation = "min", std::vector<std::string> filter = {""});
Pipeline focal(std::string connect_uid, double size, std::string fun = "mean", std::string ofile = "");
Pipeline hull(std::string connect_uid = "", std::string ofile = "");
Pipeline info();
Pipeline load_raster(std::string file, int band = 1L);
Pipeline load_matrix(std::vector<double> matrix, bool check = true);
Pipeline local_maximum(double ws, double min_height = 2, std::vector<std::string> filter = {""}, std::string ofile = "", std::string use_attribute = "Z", bool record_attributes = false);
Pipeline local_maximum_raster(std::string connect_uid, double ws, double min_height = 2, std::vector<std::string> filter = {""}, std::string ofile = "");
//Pipeline neighborhood_metrics();
Pipeline nothing(bool read = false, bool stream = false, bool loop = false);
Pipeline pit_fill(std::string connect_uid, int lap_size = 3, double thr_lap = 0.1, double thr_spk = -0.1, int med_size = 3, int dil_radius = 0, std::string ofile = "");
//Pipeline rasterize();
//Pipeline reader();
//Pipeline reader_coverage();
//Pipeline reader_circles();
//Pipeline reader_rectangles();
//Pipeline region_growing();
//Pipeline remove_attribute();
//Pipeline set_crs();
//Pipeline sampling_voxel();
//Pipeline sampling_pixel();
//Pipeline sampling_poisson();
//Pipeline stop_if_outside();
//Pipeline stop_if_chunk_id_below();
//Pipeline sort_points();
//Pipeline summarise();
//Pipeline triangulate();
Pipeline transform_with(std::string connect_uid, std::string operation = "-", std::string store_in_attribute = "", bool bilinear = true);
Pipeline write_las(std::string ofile, std::vector<std::string> filter = {""}, bool keep_buffer = false);
Pipeline write_copc(std::string ofile, std::vector<std::string> filter = {""}, bool keep_buffer = false, int max_depth = -1, std::string density = "dense");
Pipeline write_pcd(std::string ofile, bool binary = true);
Pipeline write_vpc(std::string ofile, bool absolute_path = false, bool use_gpstime = false);
Pipeline write_lax(bool embedded = false, bool overwrite = false);

#ifdef USING_R
//Pipeline aggregate();
//Pipeline callback();
#endif
} // namespace api

#endif // API_H
