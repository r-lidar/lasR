#ifndef PTD_H
#define PTD_H

#include <vector>

#include "hporro/delaunay.h"
#include "hporro/geometry.h"
#include "nanoflann/nanoflann.h"

#include "Grid.h"
#include "Shape.h"

// Configuration struct to pass parameters easily
struct PTDParameters
{
  double seed_resolution_search = 10.0;
  double max_iteration_angle = 30.0;
  double max_iteration_distance = 2.0;
  double min_triangle_size = 0.25;
  double buffer_size = 30.0;
  int max_iter = 50;
  bool verbose = false;
};

class PTD
{
public:
  PTD(const PTDParameters& params);
  ~PTD();

  bool run(std::vector<IncrementalDelaunay::Vec2>& candidates);
  std::vector<unsigned int> get_ground_fid() const;
  std::vector<unsigned int> get_spikes_fid() const;

private:
  // Core logic steps
  void calculate_bounds(const std::vector<IncrementalDelaunay::Vec2>& points);
  void sort_points_by_z(std::vector<IncrementalDelaunay::Vec2>& points);
  void make_seeds(const std::vector<IncrementalDelaunay::Vec2>& points);
  void make_buffer();
  void tin_buffer();
  void tin_seeds(const std::vector<IncrementalDelaunay::Vec2>& points);
  void densify_tin(const std::vector<IncrementalDelaunay::Vec2>& points);
  void detect_spikes();

  // Helpers
  static bool axelsson_metrics(const PointXYZ& P, const TriangleXYZ& triangle, double& dist_d, double& angle);
  static double distance_to_fitted_plane(const IncrementalDelaunay::Vec2& query, const std::vector<size_t>& neighbor_indices, const IncrementalDelaunay::Vertex* vertices);

private:
  PTDParameters params;

  double x_min, y_min, x_max, y_max, z_default;

  std::vector<IncrementalDelaunay::Vec2> seeds;
  std::vector<IncrementalDelaunay::Vec2> vbuff;
  std::vector<bool> inserted;
  std::vector<bool> is_spike;

  Grid gd;
  IncrementalDelaunay::Triangulation* d;
  std::string last_error;
};

#endif // PTD_H