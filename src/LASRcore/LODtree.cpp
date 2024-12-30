#include <cstdio>
#include <cmath>
#include <limits>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <stdexcept>

#include "LODtree.h"
#include "PointCloud.h"

Key::Key(int32_t d, int32_t x, int32_t y, int32_t z) : d(d), x(x), y(y), z(z) {}
Key::Key() : Key(-1, -1, -1, -1) {}

std::array<Key, 8> Key::get_children() const
{
  std::array<Key, 8> children;
  int32_t next_d = d + 1;
  int32_t base_x = x * 2, base_y = y * 2, base_z = z * 2;

  for (unsigned char direction = 0; direction < 8; ++direction)
  {
    children[direction] = Key(
      next_d,
      base_x + ((direction & 1) ? 1 : 0),
      base_y + ((direction & 2) ? 1 : 0),
      base_z + ((direction & 4) ? 1 : 0)
    );
  }
  return children;
}

Key Key::get_parent() const
{
  if (!is_valid()) return Key();
  if (d == 0) return Key();
  return Key(d - 1, x >> 1, y >> 1, z >> 1);
}

Node::Node()
{
  bbox[0] = 0;
  bbox[1] = 0;
  bbox[2] = 0;
  bbox[3] = 0;
  screen_size = 0;
}

void Node::insert(size_t idx, uint32_t cell)
{
  point_idx.push_back(idx);
  if (cell >= 0) occupancy.insert(cell); // cell = -1 means that recording the location of the point is useless (save memory)
};

LODtree::LODtree(PointCloud* las)
{
  if (las->npoints > UINT32_MAX)
    throw std::runtime_error("Spatial indexation is bound to 4,294 billion points");

  npoint = las->npoints;
  max_depth = 0;
  grid_size = 128;

  xmin = las->header->min_x;
  ymin = las->header->min_y;
  zmin = las->header->min_z;
  xmax = las->header->max_x;
  ymax = las->header->max_y;
  zmax = las->header->max_z;
  double center_x = (xmin+xmax)/2;
  double center_y = (ymin+ymax)/2;
  double center_z = (zmin+zmax)/2;
  double halfsize = MAX3(xmax-xmin, ymax-ymin, zmax-zmin); halfsize /=2;

  xmin = center_x - halfsize;
  ymin = center_y - halfsize;
  zmin = center_z - halfsize;
  xmax = center_x + halfsize;
  ymax = center_y + halfsize;
  zmax = center_z + halfsize;

  compute_max_depth(las->npoints, 10000);
}

void LODtree::compute_max_depth(size_t npts, size_t max_points_per_octant)
{
  // strategy to regulate the maximum depth of the LODtree
  double xsize = xmax-xmin;
  double ysize = ymax-ymin;
  double zsize = zmax-zmin;
  double size  = MAX3(xsize, ysize, zsize);

  max_depth = 0;

  while (npts > max_points_per_octant)
  {
    if (xsize >= size) { npts /= 2; }
    if (ysize >= size) { npts /= 2; }
    if (zsize >= size) { npts /= 2; }
    size /= 2;
    max_depth++;
  }

  //printf("Max depth = %d\n", max_depth);
}

bool LODtree::insert(double x, double y, double z, uint32_t i)
{
  //print("1 lvl max = %d\n", max_depth);

  std::unordered_map<Key, Node, KeyHasher>::iterator it;

  // Search a place to insert the point
  int lvl = 0;
  int cell = 0;
  bool accepted = false;
  while (!accepted)
  {
    Key key = get_key(x, y, z, lvl);

    if (lvl == max_depth)
      cell = -1; // Do not build an occupancy grid for last level. Point must be inserted anyway.
    else
      cell = get_cell(x, y, z, key);

    it = registry.find(key);
    if (it == registry.end())
    {
      Node node;
      set_bbox(key, node.bbox);
      it = registry.emplace(key, node).first;
    }

    auto it2 = it->second.occupancy.find(cell);
    accepted = (it2 == it->second.occupancy.end()) || (lvl == max_depth);

    lvl++;
  }

  it->second.insert(i, cell);

  //if (it->first.d == 0)
  //printf("Insert point i = %u (%.1lf, %.1lf, %.1lf) in %d-%d-%d-%d in cell %d n = %lu\n", i,  x, y, z, it->first.x, it->first.y, it->first.z, it->first.d, cell, it->second.point_idx.size());

  return true;
}

Key LODtree::get_key(double x, double y, double z, int depth) const
{
  int grid_size = 1 << depth;  // 2^depth
  double grid_resolution = (xmax - xmin) / grid_size;

  int xi = static_cast<int>((x - xmin) / grid_resolution);
  int yi = static_cast<int>((y - ymin) / grid_resolution);
  int zi = static_cast<int>((z - zmin) / grid_resolution);

  xi = std::clamp(xi, 0, grid_size - 1);
  yi = std::clamp(yi, 0, grid_size - 1);
  zi = std::clamp(zi, 0, grid_size - 1);

  return Key(depth, xi, yi, zi);
}

void LODtree::set_bbox(const Key& key, double* bb)
{
  double size = get_halfsize()*2;
  double res  = size / (1 << key.d);

  double minx = res * key.x + (get_center_x() - get_halfsize());
  double miny = res * key.y + (get_center_y() - get_halfsize());
  double minz = res * key.z + (get_center_z() - get_halfsize());
  double maxx = minx + res;
  double maxy = miny + res;
  double maxz = minz + res;

  bb[0] = (minx+maxx)/2;
  bb[1] = (miny+maxy)/2;
  bb[2] = (minz+maxz)/2;
  bb[3] = res/2;

  return;
}

int LODtree::get_cell(double x, double y, double z, const Key& key) const
{
  double size = get_halfsize()*2;
  double res  = size / (1 << key.d);

  double minx = res * key.x + (get_center_x() - get_halfsize());
  double miny = res * key.y + (get_center_y() - get_halfsize());
  double minz = res * key.z + (get_center_z() - get_halfsize());
  double maxx = minx + res;

  // Get cell id in this octant
  double grid_resolution = (maxx - minx) / grid_size;
  int xi = (int)std::floor((x - minx) / grid_resolution);
  int yi = (int)std::floor((y - miny) / grid_resolution);
  int zi = (int)std::floor((z - minz) / grid_resolution);
  xi = std::clamp(xi, 0, grid_size - 1);
  yi = std::clamp(yi, 0, grid_size - 1);
  zi = std::clamp(zi, 0, grid_size - 1);

  return zi * grid_size * grid_size + yi * grid_size + xi;
}

const std::string FILE_SIGNATURE = "HNOF";
const int FILE_VERSION_MAJOR = 1;
const int FILE_VERSION_MINOR = 0;

// Write function (not working)
void LODtree::write(const std::string& filename)
{
  //printf("write\n");
  std::ofstream outFile(filename, std::ios::binary);

  if (!outFile)
    throw std::runtime_error("Failed to open file for writing: " + filename);

  // Write file signature
  outFile.write(FILE_SIGNATURE.c_str(), FILE_SIGNATURE.size());

  // Write file version
  outFile.write(reinterpret_cast<const char*>(&FILE_VERSION_MAJOR), 4);
  outFile.write(reinterpret_cast<const char*>(&FILE_VERSION_MINOR), 4);

  // Write the bounding box
  outFile.write(reinterpret_cast<const char*>(&xmin), 8);
  outFile.write(reinterpret_cast<const char*>(&ymin), 8);
  outFile.write(reinterpret_cast<const char*>(&zmin), 8);
  outFile.write(reinterpret_cast<const char*>(&xmax), 8);
  outFile.write(reinterpret_cast<const char*>(&ymax), 8);
  outFile.write(reinterpret_cast<const char*>(&zmax), 8);

  // Write the grid spacing
  outFile.write(reinterpret_cast<const char*>(&zmax), 8);

  // Write the size of the unordered_map
  std::uint64_t mapSize = registry.size(); // Use uint64_t for large sizes
  outFile.write(reinterpret_cast<const char*>(&mapSize), 8);

  // Iterate through each key-value pair
  for (const auto& pair : registry)
  {
    // Write each int of Key
    outFile.write(reinterpret_cast<const char*>(&pair.first.d), 4);
    outFile.write(reinterpret_cast<const char*>(&pair.first.x), 4);
    outFile.write(reinterpret_cast<const char*>(&pair.first.y), 4);
    outFile.write(reinterpret_cast<const char*>(&pair.first.z), 4);

    // Write the size of the vector<int> (octant)
    size_t vectorSize = pair.second.point_idx.size();
    outFile.write(reinterpret_cast<const char*>(&vectorSize), sizeof(size_t));

    // Write the vector<int> data
    outFile.write(reinterpret_cast<const char*>(pair.second.point_idx.data()), vectorSize * 4);
  }

  outFile.close();
}

// Read function (not working)
bool LODtree::read(const std::string& filename)
{
  //printf("read\n");

  std::ifstream inFile(filename, std::ios::binary);

  if (!inFile)
    throw std::runtime_error("Failed to open file for reading: " + filename);

  // Read and validate file signature
  char signature[FILE_SIGNATURE.size()];
  inFile.read(signature, FILE_SIGNATURE.size());
  if (std::string(signature, FILE_SIGNATURE.size()) != FILE_SIGNATURE)
    throw std::runtime_error("Invalid file signature.");

  // Read and validate file version
  int fileVersionMajor;
  inFile.read(reinterpret_cast<char*>(&fileVersionMajor), 4);
  int fileVersionMinor;
  inFile.read(reinterpret_cast<char*>(&fileVersionMinor), 4);
  if (fileVersionMajor != 1 || fileVersionMinor != 0)
    throw std::runtime_error(std::string("Unsupported file version: ") + std::to_string(fileVersionMajor) + "." + std::to_string(fileVersionMinor));

  // Read the size of the unordered_map
  uint64_t mapSize;
  inFile.read(reinterpret_cast<char*>(&mapSize), 8);

  // Read the bbox
  inFile.read(reinterpret_cast<char*>(&xmin), 8);
  inFile.read(reinterpret_cast<char*>(&ymin), 8);
  inFile.read(reinterpret_cast<char*>(&zmin), 8);
  inFile.read(reinterpret_cast<char*>(&xmax), 8);
  inFile.read(reinterpret_cast<char*>(&ymax), 8);
  inFile.read(reinterpret_cast<char*>(&zmax), 8);

  // Read the grid spacing
  inFile.read(reinterpret_cast<char*>(&grid_size), 4);

  // Clear the existing map
  registry.clear();

  // Read each key-value pair
  npoint = 0;
  max_depth = 0;
  for (size_t i = 0; i < mapSize; ++i)
  {
    // Read Key (4 integers)
    Key key;
    inFile.read(reinterpret_cast<char*>(&key), sizeof(Key));

    if (key.d > max_depth) max_depth = key.d;

    // Read the size of the vector<int>
    uint64_t vectorSize;
    inFile.read(reinterpret_cast<char*>(&vectorSize), 8);

    npoint += vectorSize;

    Node octant;
    set_bbox(key, octant.bbox);
    octant.point_idx.resize(vectorSize);

    // Read the vector<int> data
    inFile.read(reinterpret_cast<char*>(&(octant.point_idx[0])), vectorSize * 4);

    // Insert the pair into the unordered_map
    registry.emplace(key, std::move(octant));
  }

  inFile.close();
  return true;
}


// Helper function for generating unique voxel keys
int64_t OctreeNode::voxelKey(double x, double y, double z) const
{
  int64_t col = std::floor((x - min_x) / voxelSize);
  int64_t row = std::floor((max_y - y) / voxelSize);
  int64_t lay = std::floor((z - min_z) / voxelSize);
  return  lay * 64 * 64 + row * 64 + col;
}

// Constructor
OctreeNode::OctreeNode(double min_x, double min_y, double min_z, double max_x, double max_y, double max_z, int max_depth, int depth)
  : min_x(min_x), min_y(min_y), min_z(min_z),  max_x(max_x), max_y(max_y), max_z(max_z), maxDepth(max_depth), depth(depth)
{
  voxelSize = (max_x-min_x)/64;
  children.fill(nullptr);
}

// Destructor
OctreeNode::~OctreeNode()
{
  for (auto& child : children) {
    delete child;
  }
}

// Check if the node is a leaf
bool OctreeNode::isLeaf() const
{
  return children[0] == nullptr;
}

// Add point to voxel if not already occupied
bool OctreeNode::addToVoxel(const Point& point)
{
  if (depth == maxDepth) return true;

  int64_t key = voxelKey(point.get_x(), point.get_y(), point.get_z());

  /*if (depth == 0)
  {
  printf("Key %ld \n Occupied: ", key);
  for (auto k : occupiedVoxels)
    printf("%ld ", k);
  printf("\n");
  }*/
  if (occupiedVoxels.find(key) != occupiedVoxels.end()) return false; // Voxel already occupied
  occupiedVoxels.insert(key);
  return true;
}

// Subdivide the current node
void OctreeNode::subdivide()
{
  double mid_x = (min_x + max_x) / 2.0;
  double mid_y = (min_y + max_y) / 2.0;
  double mid_z = (min_z + max_z) / 2.0;

  children[0] = new OctreeNode(min_x, min_y, min_z, mid_x, mid_y, mid_z, maxDepth, depth + 1);
  children[1] = new OctreeNode(mid_x, min_y, min_z, max_x, mid_y, mid_z, maxDepth, depth + 1);
  children[2] = new OctreeNode(min_x, mid_y, min_z, mid_x, max_y, mid_z, maxDepth, depth + 1);
  children[3] = new OctreeNode(mid_x, mid_y, min_z, max_x, max_y, mid_z, maxDepth, depth + 1);
  children[4] = new OctreeNode(min_x, min_y, mid_z, mid_x, mid_y, max_z, maxDepth, depth + 1);
  children[5] = new OctreeNode(mid_x, min_y, mid_z, max_x, mid_y, max_z, maxDepth, depth + 1);
  children[6] = new OctreeNode(min_x, mid_y, mid_z, mid_x, max_y, max_z, maxDepth, depth + 1);
  children[7] = new OctreeNode(mid_x, mid_y, mid_z, max_x, max_y, max_z, maxDepth, depth + 1);
}

// Insert a point into the octree
void OctreeNode::insert(const Point& point)
{
  //printf("Received a point at level %d\n", depth);
  if (depth == maxDepth)
  {
    //printf("Insert at max depth %d\n", maxDepth);
    points.push_back(point);
    return;
  }

  // Add to voxel if possible
  if (addToVoxel(point))
  {
    //printf("Insert at lvl %d\n", depth);
    points.push_back(point);
    return;
  }

  // Otherwise, subdivide and distribute points
  if (isLeaf())
  {
    //printf("Subdivide\n");
    subdivide();
  }

  for (auto& child : children)
  {
    if (child->min_x <= point.get_x() && point.get_x() < child->max_x &&
        child->min_y <= point.get_y() && point.get_y() < child->max_y &&
        child->min_z <= point.get_z() && point.get_z() < child->max_z)
    {
      child->insert(point);
      return;
    }
  }
}

// Print the octree (for debugging)
void OctreeNode::print() const
{
  /*std::cout << "Node Depth: " << depth
            << ", Points: " << points.size()
            << ", Bounds: [" << min_x << ", " << min_y << ", " << min_z
            << "] to [" << max_x << ", " << max_y << ", " << max_z << "]\n";*/

  for (const auto& child : children)
  {
    if (child != nullptr)
    {
      child->print();
    }
  }
}




