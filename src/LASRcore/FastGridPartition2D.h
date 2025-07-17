#ifndef FASTGRIDPARTITION_H
#define FASTGRIDPARTITION_H

#include <vector>

#include "Shape.h"
#include "PointCloud.h"

class FastGridPartition2D
{
public:
  FastGridPartition2D();
  FastGridPartition2D(PointCloud&);
  template<typename T> void lookup(T& shape, std::vector<PointXYI>&);

private:
  unsigned int npoints;
  unsigned int ncols, nrows, ncells;
  double xmin,ymin,xmax,ymax;
  double xres, yres;
  std::vector<std::vector<PointXYI>> heap;
  enum TYPES {UKN = 0, ALS = 1, TLS = 2, UAV = 3, DAP = 4, MLS = 5};
  enum INDEXES {AUTOINDEX = 0, GRIDPARTITION = 1, VOXELPARTITION = 2};

private:
  int get_cell(double, double, double);
  bool multilayered();
};

inline FastGridPartition2D::FastGridPartition2D()
{
}

/*
  * Default constructor using an S4 LAS object. The LAS object contains a tag
* that enables to choose automatically between a grid- or voxel-based indexation
*/
inline FastGridPartition2D::FastGridPartition2D(PointCloud& las)
{
  // Number of points
  npoints = las.npoints;

  // Compute the bounding box
  xmin = las.header->min_x;
  xmax = las.header->max_x;
  ymin = las.header->min_y;
  ymax = las.header->max_y;

  double buf = 1;
  xmin -= buf;
  xmax += buf;
  ymin -= buf;
  ymax += buf;

  // Historically the spatial index was a quadtree defined by a depth
  // The depth is still used to compute the number of cells
  unsigned int depth = (npoints > 0) ? std::floor(std::log(npoints)/std::log(4)) : 0;
  ncells = (1 << depth) * (1 << depth);

  // Compute some indicator of shape
  double xrange = xmax - xmin;
  double yrange = ymax - ymin;
  double xyratio = xrange/yrange;

  // Compute the number of rows and columns in such a way that there is approximately
  // the number of wanted cells but the organization of the cell is well balanced
  // so the resolutions on x-y-z are close. We want:
  // ncols/nrows = xyratio
  // ncols/nlays = xzratio
  // nrows/nlays = yzratio
  // ncols*nrows*nlayers = ncells
  ncols = std::round(std::sqrt(ncells*xyratio));
  if (ncols <= 0) ncols = 1;
  nrows = std::round(ncols/xyratio);
  if (nrows <= 0) nrows = 1;

  ncells = ncols*nrows;

  xres = xrange / (double)ncols;
  yres = yrange / (double)nrows;

  // Precompute cell indexes and number of points per cells
  std::vector<int> cell_index(npoints, 0);
  std::vector<unsigned int> cell_points(ncells, 0);
  unsigned int i = 0;
  while (las.read_point())
  {
    double x = las.point.get_x();
    double y = las.point.get_y();
    double z = las.point.get_z();
    int cell = get_cell(x, y, z);
    cell_index[i] = cell;
    cell_points[cell]++;
    i++;
  }

  // Allocate the strict amount of memory required
  // The goal is to avoid over memory allocation when using push_back, which double
  // the size of the container when resized.
  heap.resize(ncells);
  for (unsigned int i = 0 ; i < ncells; i++)
    heap[i].reserve(cell_points[i]);

  // Insert the points. No segfault possible here because get_cell() already check
  // if the values it returns are < 0 or > ncells-1 so we are sure to do not access
  // memory beyond heap range. No need to extra security tests (hopefully)
  i = 0;
  unsigned int key;
  while (las.read_point())
  {
    double x = las.point.get_x();
    double y = las.point.get_y();
    double z = las.point.get_z();
    key = cell_index[i];
    heap[key].emplace_back(x, y, las.current_point);
    i++;
  }
}

template<typename T> void FastGridPartition2D::lookup(T& shape, std::vector<PointXYI>& res)
{
  double xmin = shape.xmin();
  double xmax = shape.xmax();
  double ymin = shape.ymin();
  double ymax = shape.ymax();

  int colmin = std::floor((xmin - this->xmin) / xres);
  int colmax = std::ceil((xmax - this->xmin) / xres);
  int rowmin = std::floor((this->ymax - ymax) / yres);
  int rowmax = std::ceil((this->ymax - ymin) / yres);

  int cell;

  res.clear();
  for (int col = std::max(colmin,0) ; col <= std::min(colmax, (int)ncols-1) ; col++) {
    for (int row = std::max(rowmin,0) ; row <= std::min(rowmax, (int)nrows-1) ; row++) {
      cell = row * ncols + col;
      for (std::vector<PointXYI>::iterator it = heap[cell].begin() ; it != heap[cell].end() ; it++) {
        if (shape.contains(it->x, it->y))
          res.emplace_back(*it);
      }
    }
  }

  return;
}

inline int FastGridPartition2D::get_cell(double x, double y, double z)
{
  int col = std::floor((x - xmin) / xres);
  int row = std::floor((ymax - y) / yres);
  if (row < 0 || row > (int)nrows-1 || col < 0 || col > (int)ncols-1)
    throw std::runtime_error("Internal error in spatial index: point out of the range."); // # nocov
    int cell = row * ncols + col;
  if (cell < 0 || cell >= (int)ncells)
    throw std::runtime_error("Internal error in spatial index: cell out of the range."); // # nocov
    return cell;
}
#endif
