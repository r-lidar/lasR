#include <stdexcept>
#include <limits>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>

#include "PTD.h"

namespace PTD
{

bool axelsson_metrics(const Vec3& P, const Triangle& triangle, double& dist_d, double& angle);
double distance_to_fitted_plane(const Point& query, const std::vector<size_t>& neighbor_indices, const IncrementalDelaunay::Vertex* vertices);

struct PointAdaptor
{
  const std::vector<Point>& pts;
  PointAdaptor(const std::vector<Point>& pts_) : pts(pts_) {}
  inline size_t kdtree_get_point_count() const { return pts.size(); }
  inline double kdtree_get_pt(const size_t idx, const size_t dim) const { return (dim == 0 ? pts[idx].x : pts[idx].y); }
  template <class BBOX> bool kdtree_get_bbox(BBOX&) const { return false; }
};

struct VertexAdaptor
{
  const IncrementalDelaunay::Vertex* vertices;
  size_t count;
  VertexAdaptor(const IncrementalDelaunay::Vertex* v, size_t c) : vertices(v), count(c) {}
  inline size_t kdtree_get_point_count() const { return count; }
  inline double kdtree_get_pt(const size_t idx, const size_t dim) const {
    if (dim == 0) return vertices[idx].pos.x;
    if (dim == 1) return vertices[idx].pos.y;
    return vertices[idx].pos.z;
  }
  template <class BBOX> bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }
};

PTD::PTD(const PTDParameters& p) : params(p), d(nullptr)
{
  if (params.seed_resolution_search <= 0.0) throw std::invalid_argument("seed_resolution_search must be > 0");
  if (params.max_iteration_angle < 0.0 || params.max_iteration_angle > 90.0) throw std::invalid_argument("max_iteration_angle must be in [0, 90] degrees");
  if (params.max_iteration_distance <= 0.0) throw std::invalid_argument("max_iteration_distance must be > 0");
  if (params.min_triangle_size < 0.0) throw std::invalid_argument("min_triangle_size must be >= 0");
  if (params.buffer_size < 0.0) throw std::invalid_argument("buffer_size must be >= 0");
  if (params.max_iter < 0) throw std::invalid_argument("max_iter must be >= 0");
  params.min_triangle_size = params.min_triangle_size *  params.min_triangle_size; //squared comparable distance
}

PTD::~PTD() { if (d) delete d; }

bool PTD::run(std::vector<Point>& points)
{
  if (d) delete d;
  if (points.empty())
    throw std::runtime_error("0 point to process");

  inserted.assign(points.size(), false);

  calculate_bounds(points); // Bounding box of the points
  sort_points_by_z(points); // Sort to process bottom to top
  make_seeds(points);       // Generate the seed points for iteration 0
  make_buffer();            // Generate extra virtual buffer seeds

  // Grid used as spatial index by the triangulation and this class
  double bs = params.buffer_size;
  gd = IncrementalDelaunay::Grid(x_min - bs, y_min - bs, x_max + bs, y_max + bs, 1);

  // Incremental triangulation
  d = new IncrementalDelaunay::Triangulation(gd);

  // For the few first points spatial index search is slow because triangle
  // are too big. We can deactivate spatial indexing. This is actually faster
  d->desactivate_spatial_index();

  tin_buffer();
  tin_seeds(points);

  // Progressive densification
  d->activate_spatial_index(); // Now the edges and seeds are inserted we can leverage spatial indexing.
  densify_tin(points);

  // Detect spikes to find outlier
  detect_spikes();

  return true;
}

void PTD::calculate_bounds(const std::vector<Point>& points)
{
  x_min = std::numeric_limits<double>::infinity();
  y_min = std::numeric_limits<double>::infinity();
  x_max = -std::numeric_limits<double>::infinity();
  y_max = -std::numeric_limits<double>::infinity();

  for (const auto& v : points)
  {
    x_min = std::min(x_min, v.x);
    y_min = std::min(y_min, v.y);
    x_max = std::max(x_max, v.x);
    y_max = std::max(y_max, v.y);
  }
}

void PTD::sort_points_by_z(std::vector<Point>& points)
{
  std::sort(points.begin(), points.end(), [](const auto& a, const auto& b)
  {
    return a.z < b.z;
  });
}

void PTD::make_seeds(const std::vector<Point>& points)
{
  IncrementalDelaunay::Grid grid(x_min, y_min, x_max, y_max, params.seed_resolution_search);
  seeds.clear();
  seeds.resize(grid.get_ncells());
  for (auto& seed : seeds) seed.z = std::numeric_limits<double>::max();

  for (const auto& v : points)
  {
    int cell = grid.cell_from_xy(v.x, v.y);
    if (cell < 0) continue;

    if (v.z < seeds[cell].z) {
      seeds[cell] = v;
    }
  }

  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const Point& p) { return p.z == std::numeric_limits<double>::max(); });
  seeds.erase(new_end, seeds.end());

  if (seeds.empty()) throw std::runtime_error("0 seed to process");

  z_default = std::accumulate(seeds.begin(), seeds.end(), 0.0, [](double sum, const Point& p) { return sum + p.z; }) / seeds.size();
  z_default -= 100;
}

void PTD::make_buffer()
{
  if (params.buffer_size <= 0) return;

  std::mt19937 generator(std::random_device{}());
  std::uniform_real_distribution<double> distribution(-0.5, 0.5);

  double xmin = x_min - (params.buffer_size - 1);
  double ymin = y_min - (params.buffer_size - 1);
  double xmax = x_max + (params.buffer_size - 1);
  double ymax = y_max + (params.buffer_size - 1);

  double dx = xmax - xmin;
  double dy = ymax - ymin;

  int nx = std::round(dx / params.seed_resolution_search);
  int ny = std::round(dy / params.seed_resolution_search);
  double sx = dx / nx;
  double sy = dy / ny;

  vbuff.clear();
  vbuff.reserve(2 * (nx + ny) + 4); // perimeter only

  Point p;
  double x, y, x_noise, y_noise;

  // Bottom and top edges
  for (int i = 0; i <= nx; i++)
  {
    x = xmin + i * sx;
    x_noise = distribution(generator);

    // Bottom (ymin)
    y_noise = distribution(generator);
    p.x = x + x_noise;
    p.y = ymin + y_noise;
    vbuff.push_back(p);

    // Top
    y_noise = distribution(generator);
    p.x = x + x_noise;
    p.y = ymax + y_noise;
    vbuff.push_back(p);
  }

  // Left and right edges (skip corners to avoid duplicates)
  for (int j = 1; j < ny; j++)
  {
    y = ymin + j * sy;
    y_noise = distribution(generator);

    // Left (xmin)
    x_noise = distribution(generator);
    p.x = xmin + x_noise;
    p.y = y + y_noise;
    vbuff.push_back(p);

    // Right (xmax)
    x_noise = distribution(generator);
    p.x = xmax + x_noise;
    p.y = y + y_noise;
    vbuff.push_back(p);
  }


  // Using nanoflann to set Z for buffer points based on nearest seeds
  PointAdaptor adaptor(seeds);
  typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double, PointAdaptor>, PointAdaptor, 2> kd_tree_t;
  kd_tree_t index(2, adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  index.buildIndex();

  for (auto& vp : vbuff)
  {
    double query_pt[2] = { vp.x, vp.y };
    size_t ret_index;
    double out_dist_sqr;
    nanoflann::KNNResultSet<double> resultSet(1);
    resultSet.init(&ret_index, &out_dist_sqr);
    index.findNeighbors(resultSet, query_pt, nanoflann::SearchParameters());
    vp.z = seeds[ret_index].z;
  }
}

void PTD::tin_buffer()
{
  int t;
  for (auto& p : vbuff)
  {
    t = d->findContainerTriangleFast(p);
    if (!d->delaunayInsertion(p, t))
    {
      throw std::runtime_error("Internal error: virtual seed point not inserted");
    }
  }
}

void PTD::tin_seeds(const std::vector<Point>& points)
{
  // To retrive the index
  std::unordered_map<unsigned int, std::size_t> fid_to_candidate_idx;
  fid_to_candidate_idx.reserve(points.size());
  for (std::size_t i = 0; i < points.size(); ++i)
    fid_to_candidate_idx[points[i].fid] = i;

  int t;
  for (auto& seed : seeds)
  {
    t = d->findContainerTriangleFast(seed);
    if (d->delaunayInsertion(seed, t))
    {
      auto it = fid_to_candidate_idx.find(seed.fid);
      if (it != fid_to_candidate_idx.end())
      {
        inserted[it->second] = true;
      }
      else
      {
        throw std::runtime_error("Internal error: invalid fid detected");
      }
    }
  }
}


void PTD::densify_tin(const std::vector<Point>& points)
{
  int iteration = 0;
  int count = 0;
  int t = -1;

  int n_cells = gd.get_ncells();
  std::vector<bool> active_regions(n_cells, true);

  // We loop while we are adding new triangles
  do
  {
    if (params.max_iter == 0) break;

    // Reset the dirty tracker inside the triangulation
    // so it only records changes happening during this specific iteration
    d->reset_dirty_cells();

    count = 0;
    iteration++;

    for (size_t i = 0; i < points.size(); i++)
    {
      if (inserted[i]) continue;

      const auto& P = points[i];

      // Optimization Step
      // Check if the region containing this point was modified in the previous step.
      // If the cell is valid and was NOT marked active/modified in the previous loop,
      // we can skip this point entirely. The triangulation here hasn't changed.
      int cell_id = gd.cell_from_xy(P.x, P.y);
      if (!active_regions[cell_id]) continue;

      t = d->findContainerTriangleFast(P);
      if (t < 0) continue;

      // Retrieve vertices (A, B, C)
      IncrementalDelaunay::Triangle& tri = d->triangles[t];
      Vec3 A(d->vertices[tri.v[0]].pos);
      Vec3 B(d->vertices[tri.v[1]].pos);
      Vec3 C(d->vertices[tri.v[2]].pos);

      // Skip too small triangles. Small triangle are frozen and cannot be subdivided.
      Vec3 u(A.x - B.x, A.y - B.y);
      Vec3 v(A.x - C.x, A.y - C.y);
      Vec3 w(B.x - C.x, B.y - C.y);
      double edge_AB = u.x * u.x + u.y * u.y;
      double edge_AC = v.x * v.x + v.y * v.y;
      double edge_BC = w.x * w.x + w.y * w.y;
      double sq_max_edge = std::max(edge_AB, std::max(edge_AC, edge_BC));
      if (sq_max_edge < params.min_triangle_size)
      {
        inserted[i] = true; // Freeze
        continue;
      }

      double dist_d, angle;
      Triangle triangle(A, B, C);
      Vec3 p(P);
      if (!axelsson_metrics(p, triangle, dist_d, angle)) continue;

      if (angle < params.max_iteration_angle && dist_d < params.max_iteration_distance)
      {
        if (d->delaunayInsertion(P, t))
        {
          inserted[i] = true;
          count++;
        }
      }
    }

    active_regions = d->get_dirty_cells();
    float perc = (double)count / (double)d->vcount;
    if (perc < 0.5f / 1000.0f) break;

  } while (count > 0 && iteration < params.max_iter);
}

void PTD::detect_spikes()
{
  is_spike.assign(d->vcount, false);
  if (params.max_iter == 0) return;

  const int k = 8;
  const double threshold = 0.75;

  VertexAdaptor adaptor(d->vertices, d->vcount);
  typedef nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double, VertexAdaptor>,VertexAdaptor, 2> kdtree;

  kdtree index(2, adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
  index.buildIndex();

  // Iterate over all vertices
  // Pre-allocate memory for search results
  std::vector<unsigned int> ret_index(k + 1); // +1 because the point itself will be found
  std::vector<double> out_dist_sqr(k + 1);

  for (int i = 0; i < d->vcount; ++i)
  {
    double query_pt[2] = {d->vertices[i].pos.x, d->vertices[i].pos.y};
    size_t num_results = index.knnSearch(&query_pt[0], k + 1, &ret_index[0], &out_dist_sqr[0]);

    std::vector<size_t> neighbors;
    neighbors.reserve(k);
    for (size_t j = 0; j < num_results; ++j)
    {
      if (ret_index[j] != (size_t)i)
      {
        neighbors.push_back(ret_index[j]);
      }
    }

    // We need at least 3 points to define a plane
    if (neighbors.size() < 3) continue;

    // Fit plane and check distance
    double diff = distance_to_fitted_plane(d->vertices[i].pos, neighbors, d->vertices);

    // Check against threshold (absolute value for spikes both up and down)
    if (std::abs(diff) > threshold)
    {
      is_spike[i] = true;
    }
  }
}

std::vector<unsigned int> PTD::get_ground_fid() const
{
  std::vector<unsigned int> idx;
  idx.reserve(d->vcount);
  unsigned int offset = (4 + vbuff.size() + 1);

  for (unsigned int i = offset; i < d->vcount; i++)
  {
    unsigned int fid = d->vertices[i].pos.fid;
    if (!is_spike[i]) idx.push_back(fid);
  }

  return idx;
}

std::vector<unsigned int> PTD::get_spikes_fid() const
{
  std::vector<unsigned int> idx;
  idx.reserve(d->vcount);
  unsigned int offset = (4 + vbuff.size() + 1);

  for (unsigned int i = offset; i < d->vcount; i++)
  {
    unsigned int fid = d->vertices[i].pos.fid;
    if (is_spike[i]) idx.push_back(fid);
  }

  return idx;
}

bool axelsson_metrics(const Vec3& P, const Triangle& triangle, double& dist_d, double& angle)
{
  Vec3 A = triangle.A;
  Vec3 n = triangle.normal();
  Vec3 v = P - A;

  dist_d = std::abs(v.dot(n));

  // Calculate projection of P onto the triangle plane
  Vec3 P_proj = P - n * v.dot(n);

  // Check if the projection falls inside the triangle
  if (!triangle.contains(P_proj)) return false;

  double h0 = P_proj.distance(triangle.A);
  double h1 = P_proj.distance(triangle.B);
  double h2 = P_proj.distance(triangle.C);

  double alpha = std::atan2(dist_d, h0) * 180.0 / M_PI;
  double beta  = std::atan2(dist_d, h1) * 180.0 / M_PI;
  double gamma = std::atan2(dist_d, h2) * 180.0 / M_PI;

  angle = std::max(alpha, std::max(beta, gamma));

  return true;
}

double distance_to_fitted_plane(const Point& query, const std::vector<size_t>& neighbor_indices, const IncrementalDelaunay::Vertex* vertices)
{
  if (neighbor_indices.size() < 3)
    return 0.0;

  // --- Centroid for numerical stability ---
  double cx = 0, cy = 0, cz = 0;
  for (size_t idx : neighbor_indices)
  {
    cx += vertices[idx].pos.x;
    cy += vertices[idx].pos.y;
    cz += vertices[idx].pos.z;
  }
  cx /= neighbor_indices.size();
  cy /= neighbor_indices.size();
  cz /= neighbor_indices.size();

  // --- Least squares fit: z' = a*x' + b*y' ---
  double sxx = 0, sxy = 0, syy = 0;
  double sxz = 0, syz = 0;

  for (size_t idx : neighbor_indices)
  {
    double dx = vertices[idx].pos.x - cx;
    double dy = vertices[idx].pos.y - cy;
    double dz = vertices[idx].pos.z - cz;

    sxx += dx * dx;
    sxy += dx * dy;
    syy += dy * dy;
    sxz += dx * dz;
    syz += dy * dz;
  }

  double det = sxx * syy - sxy * sxy;

  // Degenerate XY configuration
  if (std::abs(det) < 1e-9)
    return 0.0;

  double a = (syy * sxz - sxy * syz) / det;
  double b = (sxx * syz - sxy * sxz) / det;

  // --- Predicted Z at query position ---
  double qdx = query.x - cx;
  double qdy = query.y - cy;
  double predicted_z = (a * qdx + b * qdy) + cz;

  // --- Vertical residual ---
  double vertical_residual = query.z - predicted_z;

  // --- Convert to perpendicular distance ---
  // Plane normal = (-a, -b, 1)
  double normal_length = std::sqrt(a * a + b * b + 1.0);

  return vertical_residual / normal_length;
}

}