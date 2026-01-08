#include "index.h"

#include <stdexcept>

namespace IncrementalDelaunay
{

Grid::Grid() : ncols(0), nrows(0), ncells(0), xmin(0), ymin(0), xmax(0), ymax(0), xres(0), yres(0) {}

Grid::Grid(double x_min, double y_min, double x_max, double y_max, double res) : xres(res), yres(res)
{
  xmin = round_any(x_min - 0.5 * xres, xres);
  xmax = round_any(x_max - 0.5 * xres, xres) + xres;
  ymin = round_any(y_min - 0.5 * yres, yres);
  ymax = round_any(y_max - 0.5 * yres, yres) + yres;

  ncols = static_cast<int>(std::round((xmax - xmin) / xres));
  nrows = static_cast<int>(std::round((ymax - ymin) / yres));
  ncells = ncols * nrows;

  if (nrows > 0 && ncols != ncells / nrows)
    throw std::runtime_error("Grid size exceeds integer limits.");
}

int Grid::cell_from_xy(double x, double y) const
{
  if (x < xmin || x > xmax || y < ymin || y > ymax)
    return -1;

  int col = std::floor((x - xmin) / xres);
  int row = std::floor((ymax - y) / yres);
  if (y == ymin) row = nrows-1;
  if (x == xmax) col = ncols-1;
  return cell_from_row_col(row, col);
}

void Grid::get_cells(double xmin, double ymin, double xmax, double ymax, std::vector<int>& cells) const
{
  int colmin = (xmin - this->xmin) / xres;
  int colmax = (xmax - this->xmin) / xres;
  int rowmin = (this->ymax - ymax) / yres;
  int rowmax = (this->ymax - ymin) / yres;
  cells.clear();

  for (int col = std::max(colmin,0) ; col <= std::min(colmax, (int)ncols-1) ; col++) {
    for (int row = std::max(rowmin,0) ; row <= std::min(rowmax, (int)nrows-1) ; row++) {
      cells.push_back(row * ncols + col);
    }
  }
}

}