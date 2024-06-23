#ifndef GRID_H
#define GRID_H

#include "macros.h"

#include <cmath>
#include <vector>

struct Voxel
{
  int i, j, k;
  Voxel() : i(0), j(0), k(0) {}
  Voxel(int i, int j, int k) : i(i), j(j), k(k) {}
  size_t hash() const
  {
    // Create individual hashes for x and y
    size_t hashi = std::hash<int>{}(i);
    size_t hashj = std::hash<int>{}(j);
    size_t hashk = std::hash<int>{}(k);

    // Combine the individual hashes using a hash combiner
    size_t seed = 0;
    seed ^= hashi + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hashj + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hashk + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  };
  bool operator==(const Voxel& other) const { return i == other.i && j == other.j && k == other.k; };
};

struct VoxelHash
{
  std::size_t operator()(const Voxel& voxel) const { return voxel.hash() ;}
};

class Grid
{
public:
  enum Contiguity {ROOK = 4, QUEEN = 8};

  Grid(double xmin, double ymin, double xmax, double ymax, double res);
  Grid(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols);
  Grid(const Grid& grid);
  //Grid(const Grid& grid, int dissagregate);

  int cell_from_xy(double x, double y) const;
  inline int col_from_cell(int cell) const { return cell%ncols; }
  inline int row_from_cell(int cell) const { return std::floor(cell/ncols); }
  inline int cell_from_row_col(int row, int col) const { return row * ncols + col; }
  inline double x_from_col(int col) const { return xmin + ((col+0.5) * xres); };
  inline double y_from_row(int row) const { return ymax - ((row+0.5) * yres); };
  inline double x_from_cell(int cell) const { return x_from_col(col_from_cell(cell)); };
  inline double y_from_cell(int cell) const { return y_from_row(row_from_cell(cell)); };

  inline int get_ncells() const { return ncells; }
  inline int get_nrows() const { return nrows; }
  inline int get_ncols() const { return ncols; }
  inline double get_xmin() const { return xmin; }
  inline double get_ymin() const { return ymin; }
  inline double get_xmax() const { return xmax; }
  inline double get_ymax() const { return ymax; }
  inline double get_xres() const { return xres; }
  inline double get_yres() const { return yres; }

  std::vector<int> get_adjacent_cells(int cell, Contiguity n = QUEEN) const;
  //std::vector<int> get_cells_dissagregated(int cell, int factor, float drop_buffer = 0) const;
  //std::vector<int> get_cells_dissagregated(const std::vector<int>& cells, int factor, float drop_buffer = 0) const;
  void get_cells(double xmin, double ymin, double xmax, double ymax, std::vector<int>& cell) const;
  //std::pair<double, double> get_cell_center(int cell) const;

protected:
  int ncols, nrows, ncells;
  double xmin,ymin,xmax,ymax;
  double xres, yres;
};

/*class Grid3D : public Grid
{
  Grid3D(double xmin, double ymin, double zmin, double xmax, double ymax, double zmax, double res);
  Grid3D(double xmin, double ymin, double zmin, double xmax, double ymax, double zmax, int nrows, int ncols, int nlyrs);
  Grid3D(const Grid3D& grid);

  void get_cells(double xmin, double ymin, double zmin, double xmax, double ymax, double zmax, std::vector<int>& cell) const;

  int cell_from_xyz(double x, double y, double z) const;
  inline int col_from_cell(int cell) const { return cell % ncols; }
  inline int row_from_cell(int cell) const { return (cell / ncols) % nrows; }
  inline int lyr_from_cell(int cell) const { return cell / (ncols * nrows); }
  inline int cell_from_row_col_lyr(int row, int col, int lyr) const { return (lyr * nrows * ncols) + (row * ncols) + col; }
  inline double z_from_lyr(int lyr) const { return zmin + ((lyr + 0.5) * zres); }
  inline double z_from_cell(int cell) const { return z_from_lyr(lyr_from_cell(cell)); }


  inline int get_nlyrs() const { return nlyrs; }
  inline double get_zres() const { return zres; }

  int nlyrs;
  double zmin, zmax;
  double zres;
};*/

#endif //GRID_H
