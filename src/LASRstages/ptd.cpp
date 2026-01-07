#include "ptd.h"
#include "Profiler.h"
#include "Raster.h"
#include "NA.h"

#include "nanoflann/nanoflann.h"
#include "hporro/delaunay.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>

struct Vec2Adaptor
{
  const std::vector<IncrementalDelaunay::Vec2>& pts;
  Vec2Adaptor(const std::vector<IncrementalDelaunay::Vec2>& pts_) : pts(pts_) {}
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

double distance_to_fitted_plane(const IncrementalDelaunay::Vec2& query, const std::vector<size_t>& neighbor_indices, const IncrementalDelaunay::Vertex* vertices);
bool axelsson_metrics(const PointXYZ& P, const TriangleXYZ& triangle, double& dist_d, double& angle);

LASRptd::LASRptd()
{
  this->las = nullptr;
  this->d = nullptr;
}

bool LASRptd::set_parameters(const nlohmann::json& stage)
{
  this->max_iteration_distance = stage.value("distance", 1);
  this->max_iteration_angle = stage.value("angle", 15);
  this->seed_resolution_search = stage.value("res", 50);
  double min_size = stage.value("min_size", 0.1);
  this->min_triangle_size = min_size*min_size;
  this->classification = stage.value("class", 2);
  this->max_iter = stage.value("max_iter", 1000);
  this->buffer_size = stage.value("buffer_size", 30);
  return true;
}

bool LASRptd::process(PointCloud*& las)
{
  Profiler tot;

  // Compute some offset to geographic coordinate
  // and move close to (0,0) for floating point
  // accuracy
  this->las = las;
  this->x_offset = las->header->min_x;
  this->y_offset = las->header->min_y;
  this->z_offset = las->header->min_z;
  size_t n = las->npoints;

  double min_x = las->header->min_x;
  double min_y = las->header->min_y;
  double max_x = las->header->max_x;
  double max_y = las->header->max_y;

  // Used to retain if a point is already part of the triangulation
  // and skip some computation
  std::vector<bool> inserted(n, false);

  Profiler prof;

  // =========================================
  // Order in which points should be processed
  // =========================================

  if (verbose) print(" Sorting points: ");

  // Sort by height (first element of pair)
  std::vector<std::pair<float, unsigned int>> items;
  items.reserve(n);
  unsigned int i = 0;
  while (las->read_point())items.emplace_back(las->point.get_z(), i++);
  std::sort(items.begin(), items.end(),  [](const auto& a, const auto& b) {  return a.first < b.first; });
  std::vector<unsigned int> index; index.reserve(n);
  for (const auto& item : items) index.push_back(item.second);
  items.clear();
  items.shrink_to_fit();

  if (verbose) print("%.2f secs\n", prof.elapsed());

  // =====================
  // Find some seed points
  // =====================

  make_seeds();

  if (seeds.size() == 0) {
    last_error = "0 point to process";
    return false;
  }

  // ============================
  // Create additional virtual
  // edge seeds
  // ============================

  make_buffer(min_x, min_y, max_x, max_y);

  // ============================
  // Prepare the triangulation and
  // its spatial index
  // ============================

  double bs = buffer_size;
  double xo = x_offset;
  double yo = y_offset;

  Grid gd(min_x-xo-bs, min_y-yo-bs, max_x-xo+bs, max_y-yo+bs, 1);
  d = new IncrementalDelaunay::Triangulation(gd);

  // ============================
  // Triangulate edges first
  // ============================

  prof = Profiler();

  int t = -1;

  // For the few first points spatial index search is slow because triangle
  // are too big. We can deactivate spatial indexing this is actually faster
  d->desactivate_spatial_index();

  if (verbose)  print(" Buffer: adding %lu virtual seed ", vbuff.size());

  for (auto& p : vbuff)
  {
    t = d->findContainerTriangleFast(p);
    if (!d->delaunayInsertion(p, t))
    {
      last_error = "Internal error: virtual seed point not inserted for unknown reason";
      return false;
    }
  }

  if (verbose) print(" took %.2f secs\n", prof.elapsed());

  // ============================
  // Triangulate the seed points
  // ============================

  if (verbose) print(" Iteration 0: ");

  prof = Profiler();

  int iteration = 0;

  if (verbose) print("adding %lu seeds to the ground", seeds.size());

  for (auto& seed : seeds)
  {
    t = d->findContainerTriangleFast(seed);

    if (d->delaunayInsertion(seed, t))
    {
      inserted[seed.fid] = true;
    }
  }

  if (verbose) print(" took %.2f secs\n", prof.elapsed());

  // =============================
  // Progressive TIN densification
  // =============================

  if (verbose) print(" Progressive TIN densification:\n");

  prof = Profiler();

  // Now the edges and seeds are inserted we can leverage spatial indexing.
  d->activate_spatial_index();

  int count = 0;

  std::vector<bool> active_regions(gd.get_ncells(), true);

  // We loop while we are adding new triangles
  do
  {
    if (max_iter == 0) break;

    // Reset the dirty tracker inside the triangulation
    // so it only records changes happening during this specific iteration
    d->reset_dirty_cells();

    count = 0;

    /*progress->reset();
    progress->set_prefix("PDT");
    progress->set_total(las->npoints);
    progress->show();*/

    Profiler prof2;

    iteration++;
    t = -1;

    // Process all the points in order
    for (int id : index)
    {
      // This point has already been inserted in the triangulation. Skip next computations
      if (inserted[id]) continue;

      las->seek(id);
      double x = las->point.get_x();
      double y = las->point.get_y();
      double z = las->point.get_z();

      //(*progress)++;
      //progress->show();

      if (pointfilter.filter(&las->point)) continue;

      IncrementalDelaunay::Vec2 P(x - xo, y - yo, z);
      P.fid = id;

      // Optimization Step
      // Check if the region containing this point was modified in the previous step.
      // If the cell is valid and was NOT marked active/modified in the previous loop,
      // we can skip this point entirely. The triangulation here hasn't changed.
      int cell_id = gd.cell_from_xy(P.x, P.y);
      if (!active_regions[cell_id]) continue;

      t = d->findContainerTriangleFast(P);

      if (t < 0) continue;

      // Retrieve the vertices of the triangle
      const IncrementalDelaunay::Triangle& tri = d->triangles[t];
      const IncrementalDelaunay::Vec2& p0 = d->vertices[tri.v[0]].pos;
      const IncrementalDelaunay::Vec2& p1 = d->vertices[tri.v[1]].pos;
      const IncrementalDelaunay::Vec2& p2 = d->vertices[tri.v[2]].pos;
      PointXYZ A(p0.x, p0.y, p0.z);
      PointXYZ B(p1.x, p1.y, p1.z);
      PointXYZ C(p2.x, p2.y, p2.z);
      TriangleXYZ triangle(A,B,C);

      double max_edge = triangle.square_max_edge_size();

      // Skip too small triangles. Small triangle are frozen and cannot be subdivided.
      if (max_edge < min_triangle_size)
      {
        // will be skipped next time since because the triangle is frozen.
        inserted[id] = true;
        continue;
      }

      double dist_d, angle;
      PointXYZ pt(P.x, P.y, P.z);
      if (!axelsson_metrics(pt, triangle, dist_d, angle)) continue;

      if (angle < max_iteration_angle && dist_d < max_iteration_distance)
      {
        if (d->delaunayInsertion(P, t))
        {
          inserted[id] = true;
          count++;
        }
      }

      progress->update(id);
    }

    // Prepare for next iteration
    // The active regions for the next loop are the ones that became "dirty"
    // during this loop.
    active_regions = d->get_dirty_cells();

    float perc = (double)count / (double)d->vcount;

    if (iteration == 1 && perc < 0.90)
    {
      warning("Internal debug warning. The first iteration is expected to insert more than 90%% of the point.\n");
    }

    if (verbose) print("  Iteration %d: adding %d points (+%.1f%%) to the ground took %.2f secs\n", iteration, count, perc * 100, prof2.elapsed());

    if (perc < 0.5f / 1000.0f) break; // We actually stop the loop when < 0.05% additions

  } while (count > 0 && iteration < max_iter);


  if (verbose) print(" PDT took %.2f secs\n", tot.elapsed());

  //progress->done();

  // =====================================
  // Detect spikes
  // =====================================

  std::vector<bool> is_spike = detect_spikes();

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  if (verbose) print(" Classification\n");

  AttributeAccessor get_and_set_classification("Classification");

  // Reset classification to non classified
  while (las->read_point())
  {
    if (get_and_set_classification(&las->point) == classification)
      get_and_set_classification(&las->point, 1);
  }

  // =====================================
  // Classify ground points
  // =====================================

  for (unsigned int i = 0; i < d->tcount; i++)
  {
    // Check all 3 vertices of the triangle
    for (int j = 0; j < 3; j++)
    {
      // The index of the vertices
      int v_idx = d->triangles[i].v[j];

      if (v_idx < 4) continue;                     // 1. Skip Super Vertices (0, 1, 2, 3)
      if (v_idx < (4 + vbuff.size())) continue;    // 2. Skip Virtual Buffer points

      unsigned int fid = d->vertices[v_idx].pos.fid;
      las->seek(fid);

      if (!is_spike[v_idx])
        get_and_set_classification(&las->point, classification);
      else
        get_and_set_classification(&las->point, 7);
    }
  }

  d->write("/home/jr/delaunay25.obj");

  return true;
}


void LASRptd::clear(bool last)
{
  delete d;
  d = nullptr;
}

void LASRptd::make_seeds()
{
  auto prof = Profiler();

  if (verbose) print(" Searching seeds: ");

  Grid grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, seed_resolution_search);
  seeds.clear();
  seeds.resize(grid.get_ncells());
  for (auto& seed : seeds) seed.z = std::numeric_limits<double>::max();

  int counter = 0;
  while (las->read_point())
  {
    if (pointfilter.filter(&las->point)) continue;

    double x = las->point.get_x();
    double y = las->point.get_y();
    double z = las->point.get_z();
    unsigned int id = las->current_point;
    int cell = grid.cell_from_xy(x, y);

    IncrementalDelaunay::Vec2& p = seeds[cell];
    if (z < p.z)
    {
      p.x = x-x_offset;
      p.y = y-y_offset;
      p.z = z;
      p.fid = id;
    }
  }

  // We created 1 seed per grid cell. Some cell maybe empty. In these cells: z = INF
  auto new_end = std::remove_if(seeds.begin(), seeds.end(), [](const IncrementalDelaunay::Vec2& p) { return p.z == std::numeric_limits<double>::max(); });
  seeds.erase(new_end, seeds.end());

  // In the triangulation the 4 first default points at infinity need a Z value. We will use mean z of seeds
  z_default = std::accumulate(seeds.begin(), seeds.end(), 0.0, [](double sum, const IncrementalDelaunay::Vec2& p) { return sum + p.z; }) / seeds.size();
  z_default -= 100;

  if (verbose) print("%.2f secs\n", prof.elapsed());
}

void LASRptd::make_buffer(double xmin, double ymin, double xmax, double ymax)
{
  if (buffer_size <= 0) return;

  std::mt19937 generator(std::random_device{}());
  double noise_magnitude = 1;
  std::uniform_real_distribution<double> distribution(-noise_magnitude / 2.0, noise_magnitude / 2.0);

  xmax += (buffer_size-1);
  ymax += (buffer_size-1);
  xmin -= (buffer_size-1);
  ymin -= (buffer_size-1);

  double dx = xmax - xmin;
  double dy = ymax - ymin;

  int nx = std::round(dx / seed_resolution_search);
  int ny = std::round(dy / seed_resolution_search);

  double sx = dx / nx;
  double sy = dy / ny;

  vbuff.clear();
  vbuff.reserve(2 * (nx + ny) + 4); // perimeter only

  IncrementalDelaunay::Vec2 p;
  double x, y, x_noise, y_noise;

  // Bottom and top edges
  for (int i = 0; i <= nx; i++)
  {
    x = xmin + i * sx;
    x_noise = distribution(generator);

    // Bottom (ymin)
    y_noise = distribution(generator);
    p.x = x + x_noise - x_offset;
    p.y = ymin + y_noise - y_offset;
    vbuff.push_back(p);

    // Top
    y_noise = distribution(generator);
    p.x = x + x_noise - x_offset;
    p.y = ymax + y_noise - y_offset;
    vbuff.push_back(p);
  }

  // Left and right edges (skip corners to avoid duplicates)
  for (int j = 1; j < ny; j++)
  {
    y = ymin + j * sy;
    y_noise = distribution(generator);

    // Left (xmin)
    x_noise = distribution(generator);
    p.x = xmin + x_noise  - x_offset;
    p.y = y + y_noise  - y_offset;
    vbuff.push_back(p);

    // Right (xmax)
    x_noise = distribution(generator);
    p.x = xmax + x_noise - x_offset;
    p.y = y + y_noise - y_offset;
    vbuff.push_back(p);
  }

  using namespace nanoflann;
  Vec2Adaptor adaptor(seeds);
  typedef KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<double, Vec2Adaptor>,Vec2Adaptor,2> kd_tree_t;

  kd_tree_t index(2, adaptor, KDTreeSingleIndexAdaptorParams(10));
  index.buildIndex();

  for (auto& p : vbuff)
  {
    double query_pt[2] = { p.x, p.y };

    size_t ret_index;
    double out_dist_sqr;

    KNNResultSet<double> resultSet(1);
    resultSet.init(&ret_index, &out_dist_sqr);

    index.findNeighbors(resultSet, query_pt, nanoflann::SearchParameters());

    p.z = seeds[ret_index].z;
  }
}

std::vector<bool> LASRptd::detect_spikes()
{
  std::vector<bool> is_spike(d->vcount, false);

  if (max_iter == 0) return is_spike;

  if (verbose) print(" Spikes removal: ");

  Profiler prof;

  int k = 8;
  double threshold = 0.75;

  // KD-Tree adaptor
  VertexAdaptor adaptor(d->vertices, d->vcount);

  // We search neighbors in 2D (XY) for DTMs
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

    // 3. Fit plane and check distance
    double diff = distance_to_fitted_plane(d->vertices[i].pos, neighbors, d->vertices);

    // Check against threshold (absolute value for spikes both up and down)
    if (std::abs(diff) > threshold)
    {
      is_spike[i] = true;
    }
  }

  if (verbose)
  {
    int sum = std::accumulate(is_spike.begin(), is_spike.end(), 0);
    print("%d spike detected. Took %.2f secs\n", sum, prof.elapsed());
  }

  return is_spike;
}

double distance_to_fitted_plane(const IncrementalDelaunay::Vec2& query, const std::vector<size_t>& neighbor_indices, const IncrementalDelaunay::Vertex* vertices)
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

bool axelsson_metrics(const PointXYZ& P, const TriangleXYZ& triangle, double& dist_d, double& angle)
{
  PointXYZ A = triangle.A;
  PointXYZ n = triangle.normal();

  // Projection and distance
  PointXYZ v = P - A;
  dist_d = std::abs(v.dot(n));
  PointXYZ P_proj = P - n * v.dot(n);
  if (!triangle.contains(P_proj)) return false;

  // Distances to vertices
  double h0 = P_proj.distance(triangle.A);
  double h1 = P_proj.distance(triangle.B);
  double h2 = P_proj.distance(triangle.C);

  double alpha = atan2(dist_d, h0) * 180.0 / M_PI;
  double beta  = atan2(dist_d, h1) * 180.0 / M_PI;
  double gamma = atan2(dist_d, h2) * 180.0 / M_PI;

  angle = MAX3(alpha, beta, gamma);
  return true;
}


// See LASRtriangulate::interpolate
/*void LASRptd::interpolate(std::vector<double>& x) const
{
  Profiler prof;
  prof.tic();

  x.resize(las->npoints);
  std::fill(x.begin(), x.end(), NA_F64);

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < d->tcount; i++)
  {
    int idA = d->triangles[i].v[0] - 4;
    int idB = d->triangles[i].v[1] - 4;
    int idC = d->triangles[i].v[2] - 4;

    if (idA < 0 || idB < 0 || idC < 0)
      continue;

    PointXYZ A,B,C;
    Point p(&las->header->schema);

    las->get_point(index_map[idA], &p);
    A.x = p.get_x();
    A.y = p.get_y();
    A.z = p.get_z();

    las->get_point(index_map[idB], &p);
    B.x = p.get_x();
    B.y = p.get_y();
    B.z = p.get_z();

    las->get_point(index_map[idC], &p);
    C.x = p.get_x();
    C.y = p.get_y();
    C.z = p.get_z();
    TriangleXYZ triangle(A, B, C);

    std::vector<Point> points;
    las->query(&triangle, points);

    for (auto& p : points)
    {
      triangle.linear_interpolation(p);
      x[p.FID] = p.z;
    }
  }

  prof.toc();
  if (verbose) print("  LAS interpolation took %.2f secs\n", prof.elapsed());
}*/

// See LASRtriangulate::interpolate
/*void LASRptd::interpolate(Raster* r) const
 {
 Profiler prof;
 prof.tic();

 r->set_value(0, r->get_nodata());

 // Generate the raster points in this triangle
 double xres = r->get_xres();
 double yres = r->get_yres();

#pragma omp parallel for num_threads(ncpu)
 for (unsigned int i = 0 ; i < d->tcount; i++)
 {
 int idA = d->triangles[i].v[0] - 4;
 int idB = d->triangles[i].v[1] - 4;
 int idC = d->triangles[i].v[2] - 4;

 if (idA < 0 || idB < 0 || idC < 0)
 continue;

 PointXYZ A,B,C;
 Point p;
 p.set_schema(&las->header->schema);

 las->get_point(index_map[idA], &p);
 A.x = p.get_x();
 A.y = p.get_y();
 A.z = p.get_z();

 las->get_point(index_map[idB], &p);
 B.x = p.get_x();
 B.y = p.get_y();
 B.z = p.get_z();

 las->get_point(index_map[idC], &p);
 C.x = p.get_x();
 C.y = p.get_y();
 C.z = p.get_z();

 TriangleXYZ triangle(A, B, C);

 double minx = ROUNDANY(triangle.xmin() - 0.5 * xres, xres);
 double miny = ROUNDANY(triangle.ymin() - 0.5 * yres, yres);
 double maxx = ROUNDANY(triangle.xmax() - 0.5 * xres, xres) + xres;
 double maxy = ROUNDANY(triangle.ymax() - 0.5 * yres, yres) + yres;

 for (double x = minx ; x <= maxx ; x += xres)
 {
 for (double y = miny ; y <= maxy ; y += yres)
 {
 if (!triangle.contains({x,y})) continue;

 PointXYZ p(x,y);
 triangle.linear_interpolation(p);

 int cell = r->cell_from_xy(x,y);
 if (cell != -1) r->set_value(cell, p.z);
 }
 }
 }

 prof.toc();
 if (verbose) print("  DTM interpolation took %.2f secs\n", prof.elapsed());
 }*/

