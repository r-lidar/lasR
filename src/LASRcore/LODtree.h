#ifndef LODTREE_H
#define LODTREE_H

#include "macros.h"

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class PointCloud;

struct Key
{
  Key();
  Key(int32_t d, int32_t x, int32_t y, int32_t z);
  static Key root() { return Key(0, 0, 0, 0); }
  bool is_valid() const { return d >= 0 && x >= 0 && y >= 0 && z >= 0; }
  std::array<Key, 8> get_children() const;
  Key get_parent() const;

  int32_t d;
  int32_t x;
  int32_t y;
  int32_t z;
};

struct KeyHasher
{
  // PDAL hash method copied
  size_t operator()(const Key &k) const
  {
    std::hash<size_t> h;
    size_t k1 = ((size_t)k.d << 32) | k.x;
    size_t k2 = ((size_t)k.y << 32) | k.z;
    return h(k1) ^ (h(k2) << 1);
  }
};

inline bool operator==(const Key& a, const Key& b) { return a.d == b.d && a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(const Key& a, const Key& b) { return !(a == b); }
inline bool operator<(const Key& a, const Key& b)
{
  if (a.x < b.x) return true;
  if (a.x > b.x) return false;
  if (a.y < b.y) return true;
  if (a.y > b.y) return false;
  if (a.z < b.z) return true;
  if (a.z > b.z) return false;
  if (a.d < b.d) return true;
  if (a.d > b.d) return false;
  return false;
}

struct Node : public Key
{
  Node();
  void insert(size_t idx, uint32_t cell);
  size_t npoints() const {return point_idx.size(); };

  // Bounding box of the entry
  double bbox[4];
  float screen_size;

  std::vector<uint32_t> point_idx;
  std::unordered_set<uint32_t> occupancy;
};

class LODtree
{
public:
  LODtree() = default;
  LODtree(PointCloud* las);
  Key get_key(double x, double y, double z, int depth) const;
  int get_cell(double x, double y, double z, const Key& key) const;
  inline int get_max_depth() const { return max_depth; };
  inline double get_center_x() const { return (xmin+xmax)/2; };
  inline double get_center_y() const { return (ymin+ymax)/2; };
  inline double get_center_z() const { return (zmin+zmax)/2; };
  inline double get_halfsize() const { return (xmax-xmin)/2; };
  inline double get_size() const { return xmax-xmin; };
  inline double get_xmin() const { return xmin; };
  inline double get_ymin() const { return ymin; };
  inline double get_zmin() const { return zmin; };
  inline double get_xmax() const { return xmax; };
  inline double get_ymax() const { return ymax; };
  inline double get_zmax() const { return zmax; };
  inline uint32_t get_npoints() const { return npoint; };
  inline int get_gridsize() const { return grid_size; };
  void set_bbox(const Key& key, double* bb);
  inline void set_gridsize(int32_t size) { if (size > 2) grid_size = size; };
  //void write(const std::string& filename);
  //bool read(const std::string& filename);

  bool insert(double x, double y, double z, uint32_t i);
  std::unordered_map<Key, Node, KeyHasher> registry;

private:
  void compute_max_depth(size_t npts, size_t max_points_per_octant);

private:
  uint32_t npoint;

  double xmin;
  double ymin;
  double zmin;
  double xmax;
  double ymax;
  double zmax;

  int32_t max_depth;
  int32_t grid_size;
};


#include <vector>
#include <array>
#include <unordered_set>
#include <cmath>
#include <iostream>
#include "PointSchema.h"

class OctreeNode
{
public:
  OctreeNode() = default;
  OctreeNode(double min_x, double min_y, double min_z, double max_x, double max_y, double max_z, int max_depth, int depth);
  ~OctreeNode();

  // Methods
  void insert(const Point& point);
  void print() const;

  // Bounding box
  double min_x, min_y, min_z, max_x, max_y, max_z;

  // Screen size is computed by a drawer
  float screen_size;

  // Node properties
  int depth;                // Current depth
  int maxDepth;             // Maximum allowable depth
  std::vector<Point> points;
  std::array<OctreeNode*, 8> children;


private:
  double voxelSize;
  std::unordered_set<uint64_t> occupiedVoxels;

  // Private methods
  bool isLeaf() const;
  bool addToVoxel(const Point& point);
  int64_t voxelKey(double x, double y, double z) const;
  void subdivide();
};

#endif
