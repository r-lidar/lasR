#include "regiongrowing.h"
#include "localmaximum.h"
#include "Shape.h"

#include <array>
#include <unordered_map>

#include <ctime>

static double taketime()
{
  return (double)(clock())/CLOCKS_PER_SEC;
}


LASRregiongrowing::LASRregiongrowing(double xmin, double ymin, double xmax, double ymax, double th_seed, double th_crown, double th_tree, double DIST, LASRalgorithm* algorithm_input_rasters, LASRalgorithm* algorithm_input_seeds)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->th_seed = th_seed;
  this->th_crown = th_crown;
  this->th_tree = th_tree;
  this->DIST = DIST*DIST;

  set_connection(algorithm_input_rasters);
  set_connection(algorithm_input_seeds);

  // Initialize the output raster from input raster
  LASRlocalmaximum* p = dynamic_cast<LASRlocalmaximum*>(algorithm_input_seeds);
  LASRalgorithmRaster* q = dynamic_cast<LASRalgorithmRaster*>(algorithm_input_rasters);
  if (p && q)  raster = Raster(q->get_raster());
}

bool LASRregiongrowing::process(LAS*& las)
{
  double ti = taketime();

  // We do not know, in the map, which pointer is the pointer to the seeds and which one
  // is the pointer to the raster because the map is ordered by UID.
  LASRlocalmaximum* p = nullptr;
  LASRalgorithmRaster* q = nullptr;
  auto it1 = connections.begin();
  auto it2 = --connections.end();
  p = dynamic_cast<LASRlocalmaximum*>(it1->second);
  if (p == nullptr)
  {
    p = dynamic_cast<LASRlocalmaximum*>(it2->second);
    q = dynamic_cast<LASRalgorithmRaster*>(it1->second);
  }
  else
  {
    q = dynamic_cast<LASRalgorithmRaster*>(it2->second);
  }

  if (p == nullptr || q == nullptr)
  {
    last_error = "invalid pointers: must be 'LASRlocalmaximum' and 'LASRalgorithmRaster'. Please report this error."; // # nocov
    return false; // # nocov
  }

  progress->reset();
  progress->set_prefix("growing region");
  progress->set_total(raster.get_ncells());

  const std::vector<PointLAS> lm = p->get_maxima();
  const Raster& image = q->get_raster();

  struct Region
  {
    PointXYZ top;
    std::vector<int> cells;
    unsigned int FID;
    double sum_height;

    int npixels() { return cells.size(); };
    double mean_height() { return sum_height/npixels(); };
  };

  std::map<int, Region> regions;
  for (const auto& pt : lm)
  {
    int cell = raster.cell_from_xy(pt.x, pt.y);
    raster.set_value(cell, pt.FID);
    Region region;
    region.top = pt;
    region.cells.push_back(cell);
    region.FID = pt.FID;
    region.sum_height = image.get_value(cell);
    regions[cell] = region;
  }

  bool grown = false;
  std::array<Pixel, 4> neighbours;

  do
  {
    grown = false;

    for (auto& pair : regions)        // Loops across all regions
    {
      Region& region = pair.second;

      for(int cell : region.cells)     // Loop across all cells of a region
      {
        double hSeed = region.top.z;           // Seed height
        double mhCrown= region.mean_height();  // Mean height of the crown

        auto neighbours = raster.get_adjacent_cells(cell, Grid::ROOK);

        for(unsigned int i = 0 ; i < neighbours.size() ; i++) // For each neighbouring pixel
        {
          int cell = neighbours[i];
          if (raster.get_value(cell) != raster.get_nodata()) continue;

          float val = image.get_value(cell);
          double threshold1 = MIN3(th_tree, hSeed*th_seed, mhCrown*th_crown);
          double threshold2 = hSeed+hSeed*0.05;
          double x = raster.x_from_cell(cell);
          double y = raster.y_from_cell(cell);
          double sqdistance = (x-region.top.x)*(x-region.top.x) + (y-region.top.y)*(y-region.top.y);

          bool expend = (val > threshold1) && (val <= threshold2) && (sqdistance < DIST);

          if (expend)                                     // The pixel in part of the region
          {
            raster.set_value(cell, (float)region.FID);    // Assign the ID to the output raster
            region.cells.push_back(cell);                 // Add the pixel to the region
            region.sum_height += image.get_value(cell);   // Update the sum of the height of the region
            grown = true;
            (*progress)++;
          }
        }
      }

      progress->show();
    }
  } while (grown);

  progress->done();

  if (verbose) print("region growing took %.2g sec\n", taketime()-ti);

  return true;
}