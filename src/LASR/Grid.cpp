#include "Grid.h"

Grid::Grid(double xmin, double ymin, double xmax, double ymax, double res)
{
  this->xres = res;
  this->yres = res;

  this->xmin = ROUNDANY(xmin - 0.5 * xres, xres);
  this->xmax = ROUNDANY(xmax - 0.5 * xres, xres) + xres;
  this->ymin = ROUNDANY(ymin - 0.5 * yres, yres);
  this->ymax = ROUNDANY(ymax - 0.5 * yres, yres) + yres;

  this->ncols = std::round((this->xmax-this->xmin)/xres);
  this->nrows = std::round((this->ymax-this->ymin)/yres);
  this->ncells = ncols*nrows;
}

Grid::Grid(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols)
{
  this->xmin = xmin;
  this->xmax = xmax;
  this->ymin = ymin;
  this->ymax = ymax;

  this->xres = (xmax-xmin)/ncols;
  this->yres = (ymax-ymin)/nrows;

  this->nrows = nrows;
  this->ncols = ncols;
  this->ncells = ncols*nrows;
}

Grid::Grid(const Grid& grid)
{
  xmin = grid.xmin;
  xmax = grid.xmax;
  ymin = grid.ymin;
  ymax = grid.ymax;

  xres = grid.xres;
  yres = grid.yres;

  nrows = grid.nrows;
  ncols = grid.ncols;
  ncells = grid.ncells;
}

Grid::Grid(const Grid& grid, int dissagregate)
{
  xmin = grid.xmin;
  xmax = grid.xmax;
  ymin = grid.ymin;
  ymax = grid.ymax;

  xres = grid.xres/dissagregate;
  yres = grid.yres/dissagregate;

  ncols = std::round((xmax-xmin)/xres);
  nrows = std::round((ymax-ymin)/yres);
  ncells = ncols*nrows;
}

std::vector<int> Grid::get_adjacent_cells(int cell, Contiguity n) const
{
  int row = row_from_cell(cell);
  int col = col_from_cell(cell);
  std::vector<int> cells;
  cells.reserve(n);

  for (int i : {-1,0,1}) {
    for (int j : {-1,0,1}) {
      if (i == 0 && j == 0) continue;
      if (n == ROOK && !(i == 0 || j == 0)) continue;
      if ((row+i < 0) || (row+i >= nrows) || (col+j < 0) || (col+j >= ncols)) continue;

      cell = cell_from_row_col(row+i, col+j);
      cells.push_back(cell);
    }
  }

  return cells;
}

int Grid::cell_from_xy(double x, double y) const
{
  if (x < xmin || x > xmax || y < ymin || y > ymax)
    return -1;

  int col = std::floor((x - xmin) / xres);
  int row = std::floor((ymax - y) / yres);
  if (y == ymin) row = nrows-1;
  if (x == xmax) col = ncols-1;
  return row * ncols + col;
}

void Grid::get_cells(double xmin, double ymin, double xmax, double ymax, std::vector<int>& cells) const
{
  int colmin = std::floor((xmin - this->xmin) / xres);
  int colmax = std::ceil((xmax - this->xmin) / xres);
  int rowmin = std::floor((this->ymax - ymax) / yres);
  int rowmax = std::ceil((this->ymax - ymin) / yres);
  cells.clear();

  for (int col = std::max(colmin,0) ; col <= std::min(colmax, (int)ncols-1) ; col++) {
    for (int row = std::max(rowmin,0) ; row <= std::min(rowmax, (int)nrows-1) ; row++) {
      cells.push_back(row * ncols + col);
    }
  }
}

/*std::vector<int> Grid::get_cells_dissagregated(int cell, int factor, float drop_buffer) const
{
  int row = row_from_cell(cell);
  int col = col_from_cell(cell);
  int off = std::ceil(drop_buffer/factor);

  std::vector<int> out;
  out.reserve(factor*factor);

  for (int i = 0+off ; i < factor-off ; i++)
  {
    for (int j = 0+off ; j < factor-off ; j++)
    {
      int new_row = row*factor+i;
      int new_col = col*factor+j;
      int new_cell = new_row * ncols*factor + new_col;

      if (new_cell < 0 || new_cell >= (int)ncells*factor*factor)
        continue;

      out.push_back(new_cell);
    }
  }

  return out;
}

std::vector<int> Grid::get_cells_dissagregated(const std::vector<int>& cells, int factor, float drop_buffer) const
{
  std::vector<int> out;
  for (int cell : cells)
  {
    std::vector<int> tmp = get_cells_dissagregated(cell, factor, drop_buffer);
    out.insert(std::end(out), std::begin(tmp), std::end(tmp));
  }

  return out;
}*/

std::pair<double, double> Grid::get_cell_center(int cell) const
{
  if (cell > ncells) throw "Internal error: cell > ncells";

  int row = row_from_cell(cell);
  int col = col_from_cell(cell);
  double x = xmin + col*xres + xres/2;
  double y = ymin + row*yres + xres/2;
  return std::pair<double, double>{x,y};
}




