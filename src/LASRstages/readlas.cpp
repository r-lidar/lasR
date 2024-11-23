#include "readlas.h"

#include "lasreader.hpp"

LASRlasreader::LASRlasreader()
{
  lasreadopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;
}

bool LASRlasreader::set_chunk(Chunk& chunk)
{
  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
    lasheader = nullptr;
  }
  if (lasreadopener)
  {
    delete lasreadopener;
    lasreadopener = nullptr;
  }

  const char* tmp = filter.c_str();
  int n = strlen(tmp)+1;
  char* filtercpy = (char*)malloc(n); memcpy(filtercpy, tmp, n);

  // The openner must survive to the reader otherwise there are some pointer invalidation.
  lasreadopener = new LASreadOpener;
  lasreadopener->set_merged(true);
  lasreadopener->set_stored(false);
  lasreadopener->set_populate_header(true);
  lasreadopener->set_buffer_size(chunk.buffer);
  lasreadopener->parse_str(filtercpy);
  lasreadopener->set_copc_stream_ordered_by_chunk();

  free(filtercpy);

  // In theory if buffer = 0 we should not have neighbor files on a properly tiled dataset. If the files
  // overlap this could arise but the neighbor file won't be read because buffer = 0 does allows to instantiate
  // LASreaderBuffer. We force an epsilon buffer
  if (chunk.buffer == 0 && chunk.neighbour_files.size() > 0)
    lasreadopener->set_buffer_size(EPSILON);

  for (auto& file : chunk.main_files) lasreadopener->add_file_name(file.c_str());
  for (auto& file : chunk.neighbour_files) lasreadopener->add_neighbor_file_name(file.c_str());

  if (chunk.shape == ShapeType::RECTANGLE)
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);
  else if (chunk.shape == ShapeType::CIRCLE)
    lasreadopener->set_inside_circle((chunk.xmin+chunk.xmax)/2, (chunk.ymin+chunk.ymax)/2,  (chunk.xmax-chunk.xmin)/2 + chunk.buffer + EPSILON);
  else
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);

  lasreader = lasreadopener->open();
  if (!lasreader)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, 512, "LASlib internal error. Cannot open LASreader with %s\n", chunk.main_files[0].c_str());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  if (chunk.buffer == 0)
  {
    lasreader->header.clean_lasoriginal();
  }

  lasheader = &lasreader->header;

  // We did not use LASreaderBuffered so we build a LASvlr_lasoriginal by hand.
  if (chunk.buffer > 0)
  {
    lasheader->set_lasoriginal();
    memset((void*)lasheader->vlr_lasoriginal, 0, sizeof(LASvlr_lasoriginal));
    lasheader->vlr_lasoriginal->min_x = chunk.xmin;
    lasheader->vlr_lasoriginal->min_y = chunk.ymin;
    lasheader->vlr_lasoriginal->max_x = chunk.xmax;
    lasheader->vlr_lasoriginal->max_y = chunk.ymax;
  }

  return true;
}

bool LASRlasreader::process(LASheader*& header)
{
  header = this->lasheader;
  return true;
}

bool LASRlasreader::process(LASpoint*& point)
{
  if (lasreader->read_point())
    point = &lasreader->point;
  else
    point = nullptr;

  return true;
}

bool LASRlasreader::process(LAS*& las)
{
  Header* header = new Header;
  header->min_x = lasreader->header.min_x;
  header->max_x = lasreader->header.max_x;
  header->min_y = lasreader->header.min_y;
  header->max_y = lasreader->header.max_y;
  header->min_z = lasreader->header.min_z;
  header->max_z = lasreader->header.max_z;
  header->number_of_point_records = MAX(lasreader->header.number_of_point_records, lasreader->header.extended_number_of_point_records);

  header->schema.add_attribute("x", AttributeType::INT32, lasheader->x_scale_factor, lasheader->x_offset);
  header->schema.add_attribute("y", AttributeType::INT32, lasheader->y_scale_factor, lasheader->y_offset);
  header->schema.add_attribute("z", AttributeType::INT32, lasheader->z_scale_factor, lasheader->z_offset);
  header->schema.add_attribute("f", AttributeType::UINT8, lasheader->z_scale_factor, lasheader->z_offset); // custom lasR flags
  header->schema.add_attribute("i", AttributeType::UINT16);
  header->schema.add_attribute("c", AttributeType::UINT8);


  /*header.schema.add_attribute("r", AttributeType::UINT8);
  header->schema.add_attribute("n", AttributeType::UINT8);
  header->schema.add_attribute("c", AttributeType::UINT8);
  header->schema.add_attribute("a", AttributeType::INT16);
  header->schema.add_attribute("u", AttributeType::UINT8);
  header->schema.add_attribute("p", 2);

  if (lasreader->header.point_data_formatader. == 1)
  {
    header->schema.add_attribute("t", 8);
  }

  if (lasreader->header.point_data_formatader. == 2)
  {
    header->schema.add_attribute("R", 2);
    header->schema.add_attribute("G", 2);
    header->schema.add_attribute("B", 2);
  }

  if (lasreader->header.point_data_formatader. == 3)
  {
    header->schema.add_attribute("t", 8);
    header->schema.add_attribute("R", 2);
    header->schema.add_attribute("G", 2);
    header->schema.add_attribute("B", 2);
  }

  if (lasreader->header.point_data_formatader. == 4)
  {
    header->schema.add_attribute("t", 8);
  }

  if (lasreader->header.point_data_formatader. == 5)
  {
    header->schema.add_attribute("t", 8);
    header->schema.add_attribute("R", 2);
    header->schema.add_attribute("G", 2);
    header->schema.add_attribute("B", 2);
  }*/

  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new LAS(header);

  progress->reset();
  progress->set_total(lasreader->npoints);
  progress->set_prefix("read_las");

  Point p(&header->schema);
  AttributeWriter set_intensity("i", &header->schema);
  AttributeWriter set_classification("c", &header->schema);

  while (lasreader->read_point())
  {
    if (progress->interrupted()) break;
    p.set_x(lasreader->point.get_x());
    p.set_y(lasreader->point.get_y());
    p.set_z(lasreader->point.get_z());
    set_intensity(&p, lasreader->point.get_intensity());
    set_classification(&p, lasreader->point.get_classification());
    if (!las->add_point(p)) return false;
    progress->update(lasreader->p_count);
    progress->show();
  }

  las->update_header();

  progress->done();

  if (verbose) print(" Number of point read %d\n", las->npoints);

  //delete lasreadopener;
  //lasreadopener = nullptr;
  //lasreader->close();
  //delete lasreader;
  //lasreader = nullptr;

  return true;
}

LASRlasreader::~LASRlasreader()
{
  if (lasreadopener)
  {
    delete lasreadopener;
    lasreadopener = nullptr;
  }

  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
  }
}
