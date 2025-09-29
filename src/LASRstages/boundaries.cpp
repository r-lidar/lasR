#include "boundaries.h"
#include "triangulate.h"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>

bool LASRboundaries::set_parameters(const nlohmann::json& stage)
{
  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPolygon);
  return true;
}

bool LASRboundaries::process(Header*& header)
{
  ASSERT_VALID_POINTER(header);

  if (!connections.empty()) return true;

  double xmin, ymin, xmax, ymax;

  xmin = header->min_x + buffer;
  ymin = header->min_y + buffer;
  xmax = header->max_x - buffer;
  ymax = header->max_y - buffer;

  PolygonXY bbox;
  bbox.push_back({xmin, ymin});
  bbox.push_back({xmax, ymin});
  bbox.push_back({xmax, ymax});
  bbox.push_back({xmin, ymax});
  bbox.close();
  contour.push_back(bbox);
  return true;
}

bool LASRboundaries::process(PointCloud*& las)
{
  ASSERT_VALID_POINTER(las);

  if (connections.empty())  return true;

  // 'connections' contains a single stage that is supposed to be a triangulation stage.
  // This is the only supported stage as of feb 2024
  auto it = connections.begin();
  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->second);
  if (p == nullptr)
  {
    last_error  = "Internal error. Invalid pointer dynamic cast. Expecting a pointer to LASRtriangulate"; // # nocov
    return false; // # nocov
  }

  struct pair_hash
  {
    std::size_t operator()(const std::pair<PointXY, PointXY>& p) const noexcept {
      std::size_t h1 = p.first.hash();
      std::size_t h2 = p.second.hash();
      // same trick as in boost::hash_combine
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  // Get the contour of the triangulation
  std::vector<Edge> edges;
  p->contour(edges);

  // Build adjacency list
  std::unordered_map<PointXY, std::vector<PointXY>> adj;
  adj.reserve(edges.size() * 2);

  for (const auto& e : edges)
  {
    adj[e.A].push_back(e.B);
    adj[e.B].push_back(e.A); // undirected
  }

  // Track visited edges to avoid reusing them
  std::unordered_set<std::pair<PointXY, PointXY>, pair_hash> visited;

  std::vector<PolygonXY> rings;

  // Helper lambda to normalize edge key
  auto edge_key = [](const PointXY& a, const PointXY& b) {
    return (a.hash() < b.hash()) ? std::make_pair(a, b)
      : std::make_pair(b, a);
  };

  for (auto& [start, neighbors] : adj)
  {
    for (const auto& n : neighbors)
    {
      auto ek = edge_key(start, n);
      if (visited.count(ek)) continue; // edge already walked

      // Walk new ring
      PolygonXY poly;
      PointXY current = start;
      PointXY prev = n; // force starting direction

      poly.push_back(current);

      do {
        const auto& neigh = adj[current];
        if (neigh.empty())
          throw std::runtime_error("Broken polygon: dead end");

        PointXY next;
        if (neigh.size() == 1) {
          next = neigh[0];
        } else {
          // pick neighbor different from prev
          next = (neigh[0] == prev ? neigh[1] : neigh[0]);
        }

        // mark edge visited
        visited.insert(edge_key(current, next));

        poly.push_back(next);

        prev = current;
        current = next;
      }
      while (current != start);

      poly.close();

      rings.push_back(poly);
    }
  }

  // Classify: outer = CCW, holes = CW
  // Ensure orientation is OGC/WKT-compliant
  for (auto& ring : rings)
  {
    if (ring.is_clockwise())
      std::reverse(ring.coordinates.begin(), ring.coordinates.end());
  }

  // Sort: outer first (area largest)
  std::sort(rings.begin(), rings.end(), [](const PolygonXY& a, const PolygonXY& b) {
    return std::fabs(a.signed_area()) > std::fabs(b.signed_area());
  });

  for (const auto& rings : rings)
    contour.push_back(rings);

  return true;
}

void LASRboundaries::clear(bool last)
{
  contour.clear();
}

bool LASRboundaries::write()
{
  bool sucess;

  #pragma omp critical (write_contour)
  {
    sucess = vector.write(contour);
  }

  return sucess;
}

bool LASRboundaries::need_points() const
{
  return !connections.empty();
}

bool LASRboundaries::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'hull'"; // # nocov
    return false; // # nocov
  }

  return true;
}