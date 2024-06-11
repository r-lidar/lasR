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


LASRregiongrowing::LASRregiongrowing(double xmin, double ymin, double xmax, double ymax, double th_seed, double th_crown, double th_tree, double DIST, Stage* algorithm_input_rasters, Stage* algorithm_input_seeds)
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
  LASRlocalmaximum* lmf = dynamic_cast<LASRlocalmaximum*>(algorithm_input_seeds);
  StageRaster*  rst = dynamic_cast<StageRaster*>(algorithm_input_rasters);
  if (lmf && rst)  raster = Raster(rst->get_raster());
}

bool LASRregiongrowing::process(LAS*& las)
{
  double ti = taketime();

  // We do not know, in the map, which pointer is the pointer to the seeds and which one
  // is the pointer to the raster because the map is ordered by UID.
  LASRlocalmaximum* lmf = nullptr;
  StageRaster* rst = nullptr;
  auto it1 = connections.begin();
  auto it2 = --connections.end();
  lmf = dynamic_cast<LASRlocalmaximum*>(it1->second);
  if (lmf == nullptr)
  {
    lmf = dynamic_cast<LASRlocalmaximum*>(it2->second);
    rst = dynamic_cast<StageRaster*>(it1->second);
  }
  else
  {
    rst = dynamic_cast<StageRaster*>(it2->second);
  }

  if (lmf == nullptr || rst == nullptr)
  {
    last_error = "invalid pointers: must be 'LASRlocalmaximum' and 'StageRaster'. Please report this error."; // # nocov
    return false; // # nocov
  }

  // Test if the lmf was computed on a point cloud. If there is no connection it means local maximum was not connected
  // to a raster stage and was applied on the point cloud. If there is a connection it means it was computed on a raster
  // we must check if it was computed on the raster we are processing. A possible user error might be to compute lmf on a raster
  // then process the raster with e.g. pit_fill then feed growing region with pit_fill but proving seeds from the raw chm.
  if (lmf->get_connection().size() == 0)
  {
    warning("computing region_growing on a raster but seeds were found using the point cloud\n");
  }
  else
  {
    const Raster& ref_rast = ((StageRaster*)lmf->get_connection().begin()->second)->get_raster();
    const Raster& this_rast = rst->get_raster();

    if (&ref_rast != &this_rast)
    {
      warning("computing region_growing on a raster but seeds were found using another raster\n");
    }
  }

  progress->reset();
  progress->set_prefix("growing region");
  progress->set_total(raster.get_ncells());

  const std::vector<PointLAS>& lm = lmf->get_maxima();
  const Raster& image = rst->get_raster();

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
    if (progress->interrupted()) break;

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
  }
  while (grown);

  progress->done();

  if (verbose) print("region growing took %.2g sec\n", taketime()-ti);

  return true;
}