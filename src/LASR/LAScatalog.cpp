#include "LAScatalog.h"

#include "lasreader.hpp"
#include "laskdtree.hpp"

#include <algorithm>

LAScatalog::LAScatalog()
{
  xmin = F64_MAX;
  ymin = F64_MAX;
  xmax = F64_MIN;
  ymax = F64_MIN;
  npoints = 0;
  buffer = 0;
  chunk_size = 0;
  use_dataframe = true;

  nocrs_number = 0;
  crs_conflict_number = 0;
  epsg = 0;

  lasreadopener = new LASreadOpener;
  laskdtree = new LASkdtreeRectangles;
  laskdtree->init();
}

LAScatalog::~LAScatalog()
{
  if (lasreadopener) delete lasreadopener;
  if (laskdtree) delete laskdtree;
  for (auto p : queries) delete p;
}

void LAScatalog::add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed)
{
  Rectangle bb(xmin, ymin, xmax, ymax);
  bboxes.push_back(bb);

  if (this->xmin > xmin) this->xmin = xmin;
  if (this->ymin > ymin) this->ymin = ymin;
  if (this->xmax < xmax) this->xmax = xmax;
  if (this->ymax < ymax) this->ymax = ymax;

  laskdtree->add(xmin, ymin, xmax, ymax);
  this->indexed.push_back(indexed);
}

bool LAScatalog::add_file(std::string file)
{
  lasreadopener->add_file_name(file.c_str());
  LASreader* lasreader = lasreadopener->open();
  if (lasreader == 0)
  {
    last_error = "cannot open not open lasreader"; // # nocov
    return false; // # nocov
  }

  set_crs(&lasreader->header);

  add_bbox(lasreader->header.min_x, lasreader->header.min_y, lasreader->header.max_x, lasreader->header.max_y, lasreader->get_index() || lasreader->get_copcindex());

  npoints += lasreader->header.number_of_point_records;

  lasreader->close();
  delete lasreader;

  use_dataframe = false;
  return true;
}

void LAScatalog::add_query(double xmin, double ymin, double xmax, double ymax)
{
  Rectangle* rect = new Rectangle(xmin, ymin, xmax, ymax);
  queries.push_back(rect);
}

void LAScatalog::add_query(double xcenter, double ycenter, double radius)
{
  Circle* circ = new Circle(xcenter, ycenter, radius);
  queries.push_back(circ);
}

int LAScatalog::get_number_chunks() const
{
  if (queries.size() == 0)
    return get_number_files();

  return queries.size();
}

bool LAScatalog::get_chunk(int i, Chunk& chunk)
{
  U32 index;
  chunk.clear();

  if (i < 0 || i > get_number_chunks())
  {
    last_error = "chunk request out of bounds"; // # nocov
    return false; // # nocov
  }

  if (!laskdtree->was_built())
  {
    laskdtree->build();
  }

  if (queries.size() == 0)
  {
    Rectangle bb = bboxes[i];
    chunk.xmin = bb.xmin();
    chunk.ymin = bb.ymin();
    chunk.xmax = bb.xmax();
    chunk.ymax = bb.ymax();

    if (!use_dataframe)
    {
      chunk.main_files.push_back(lasreadopener->get_file_name(i));
      chunk.name = lasreadopener->get_file_name_only(i);
      chunk.name = chunk.name.substr(0, chunk.name.size()-4);
    }
    else
    {
      chunk.main_files.push_back("data.frame");
      chunk.name = "data.frame";
    }

    if (buffer <= 0)
    {
      return true;
    }

    chunk.buffer = buffer;

    // We are working with an R data.frame: there is no file and especially
    // no neighbouring files. We can exit now.
    if (use_dataframe)
    {
      return true;
    }

    laskdtree->overlap(bb.xmin() - buffer, bb.ymin() - buffer, bb.xmax() + buffer, bb.ymax() + buffer);
    if (laskdtree->has_overlaps())
    {
      while (laskdtree->get_overlap(index))
      {
        std::string file = lasreadopener->get_file_name(index);
        if (chunk.main_files[0] != file)
        {
          chunk.neighbour_files.push_back(file);
        }
      }
    }

    return true;
  }

  // Some shape are provided. We are performing queries i.e not processing the entire collection
  Shape* q = queries[i];
  double minx = q->xmin();
  double miny = q->ymin();
  double maxx = q->xmax();
  double maxy = q->ymax();
  double centerx = q->centroid().x;
  double centery = q->centroid().y;
  double epsilon = 1e-8;

  // Search if there is a match
  laskdtree->overlap(minx, miny,  maxx, maxy);
  if (!laskdtree->has_overlaps())
  {
    char buff[64];
    snprintf(buff, sizeof(buff), "cannot find any file at %.1lf, %.1lf", centerx, centery);
    last_error = buff;
    return false;
  }

  // There is a match we can update the chunk
  chunk.xmin = minx;
  chunk.ymin = miny;
  chunk.xmax = maxx;
  chunk.ymax = maxy;
  chunk.buffer = buffer;
  chunk.shape = q->type();

  // With an R data.frame there is no file and thus no neighbouring files. We can exit.
  if (use_dataframe)
  {
    chunk.main_files.push_back("dataframe");
    chunk.name = "data.frame";
    return true;
  }

  // We are working with a single file there is no neighbouring files. We can exit.
  if (get_number_files() == 1)
  {
    chunk.main_files.push_back(lasreadopener->get_file_name(0));
    chunk.name = lasreadopener->get_file_name_only(0);
    chunk.name = chunk.name.substr(0, chunk.name.size()-4);
    return true;
  }

  // There are likely multiple files that intersect the query. If there is only one it is easy. We can exit.
  if (laskdtree->get_num_overlaps() == 1)
  {
    laskdtree->get_overlap(index);
    chunk.main_files.push_back(lasreadopener->get_file_name(index));
    chunk.name = lasreadopener->get_file_name_only(index);
    chunk.name = chunk.name.substr(0, chunk.name.size()-4) + "_" + std::to_string(i);
    return true;
  }

  // There are multiple files. All the files are part of the main
  while (laskdtree->get_overlap(index))
  {
    chunk.main_files.push_back(lasreadopener->get_file_name(index));
  }

  // We search the file that contains the centroid of the query to assign a name to the query
  laskdtree->overlap(centerx - epsilon, centery - epsilon,  centerx + epsilon, centery + epsilon);
  if (laskdtree->has_overlaps())
  {
    laskdtree->get_overlap(index);
    chunk.name = lasreadopener->get_file_name_only(index);
    chunk.name = chunk.name.substr(0, chunk.name.size()-4) + "_" + std::to_string(i);
  }
  else
  {
    chunk.name = chunk.main_files[0];
    chunk.name = chunk.name.substr(0, chunk.name.size()-4) + "_" + std::to_string(i);
  }

  // We perform a query again with buffered shape to get the other files in the buffer
  if (chunk.buffer > 0)
  {
    laskdtree->overlap(minx - buffer, miny - buffer, maxx + buffer, maxy + buffer);
    while (laskdtree->get_overlap(index))
    {
      std::string file = lasreadopener->get_file_name(index);

      // if the file is not part of the main files
      auto it = std::find(chunk.main_files.begin(), chunk.main_files.end(), file);
      if (it != chunk.main_files.end())
      {
        chunk.neighbour_files.push_back(file);
      }
    }
  }

  return true;
}

void LAScatalog::set_crs(const LASheader* header)
{
  if (header->vlr_geo_keys)
  {
    nocrs_number++;
    for (int j = 0; j < header->vlr_geo_keys->number_of_keys; j++)
    {
      if (header->vlr_geo_key_entries[j].key_id == 3072)
      {
        nocrs_number--;
        int tmp = header->vlr_geo_key_entries[j].value_offset;

        if (epsg == 0 && wkt.empty())
          epsg = tmp;

        if (epsg != tmp || !wkt.empty())
          crs_conflict_number++;

        break;
      }
    }
  }
  else if (header->vlr_geo_ogc_wkt)
  {
    std::string tmp = std::string(header->vlr_geo_ogc_wkt);

    if (epsg == 0 && wkt.empty())
      wkt = tmp;

    if (epsg != 0 || wkt != tmp)
      crs_conflict_number++;
  }
  else
  {
    nocrs_number++;
  }
}

void LAScatalog::check_spatial_index()
{
  if (get_number_files() > 1 && get_buffer() > 0 && get_number_indexed_files() != get_number_files())
  {
    warning("%d files do not have spatial index. Spatial indexing speeds-up drastically tile buffering.", get_number_files()-get_number_indexed_files());
  }
}

int LAScatalog::get_number_indexed_files() const
{
  return std::count(indexed.begin(), indexed.end(), true);
}




