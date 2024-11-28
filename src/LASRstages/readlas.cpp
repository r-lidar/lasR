#include "readlas.h"
#include "LASlibextension.hpp"

#include "lasreader.hpp"


LASRlasreader::LASRlasreader()
{
  lasreadopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;
  header = nullptr;
  streaming = true;

  intensity = AttributeHandler("Intensity");
  returnnumber = AttributeHandler("ReturnNumber");
  numberofreturns = AttributeHandler("NumberOfReturns");
  userdata = AttributeHandler("UserData");
  classification = AttributeHandler("Classification");
  psid = AttributeHandler("PointSourceID");
  scanangle = AttributeHandler("ScanAngle");
  gpstime = AttributeHandler("gpstime");
  scannerchannel = AttributeHandler("ScannerChannel");
  red = AttributeHandler("R");
  green = AttributeHandler("G");
  blue = AttributeHandler("B");
  nir = AttributeHandler("NIR");
}

bool LASRlasreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // New chunk -> new reader for a new file. We can delete the previous reader and build a new one
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

  // Create a reader
  LASlibFilterParserExtension parser;
  for (auto& s : filters) s = parser.parse(s);
  std::string sfilter = std::accumulate(filters.begin(), filters.end(), std::string(" "));

  const char* tmp = sfilter.c_str();
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

  if (circular)
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
  /*if (chunk.buffer > 0)
  {
    lasheader->set_lasoriginal();
    memset((void*)lasheader->vlr_lasoriginal, 0, sizeof(LASvlr_lasoriginal));
    lasheader->vlr_lasoriginal->min_x = chunk.xmin;
    lasheader->vlr_lasoriginal->min_y = chunk.ymin;
    lasheader->vlr_lasoriginal->max_x = chunk.xmax;
    lasheader->vlr_lasoriginal->max_y = chunk.ymax;
  }*/

  return true;
}

bool LASRlasreader::process(Header*& header)
{
  // LASRlasreader is responsible for populating the header.
  // It is called first before LASRlasreader::process(Point) (streaming) or LASRlasreader::process(LAS) (in memory)
  // If the point is null then we create one Header. This object own the Header
  if (header != nullptr) return true;

  header = new Header;
  header->min_x = lasreader->header.min_x;
  header->max_x = lasreader->header.max_x;
  header->min_y = lasreader->header.min_y;
  header->max_y = lasreader->header.max_y;
  header->min_z = lasreader->header.min_z;
  header->max_z = lasreader->header.max_z;
  header->number_of_point_records = MAX(lasreader->header.number_of_point_records, lasreader->header.extended_number_of_point_records);

  header->schema.add_attribute("X", AttributeType::INT32, lasheader->x_scale_factor, lasheader->x_offset, "X coordinate");
  header->schema.add_attribute("Y", AttributeType::INT32, lasheader->y_scale_factor, lasheader->y_offset, "Y coordinate");
  header->schema.add_attribute("Z", AttributeType::INT32, lasheader->z_scale_factor, lasheader->z_offset, "Z coordinate");
  header->schema.add_attribute("flags", AttributeType::UINT8, 1, 0, "Internal 8-bit mask reserved lasR core engine");
  header->schema.add_attribute("Intensity", AttributeType::UINT16, 1, 0, "Pulse return magnitude");
  header->schema.add_attribute("ReturnNumber", AttributeType::UINT8, 1, 0, "Pulse return number for a given output pulse");
  header->schema.add_attribute("NumberOfReturns", AttributeType::UINT8, 1, 0, "Total number of returns for a given pulse");
  header->schema.add_attribute("Classification", AttributeType::UINT8, 1, 0, "The 'class' attributes of a point");
  header->schema.add_attribute("UserData", AttributeType::UINT8, 1, 0, "Used at the user’s discretion");
  header->schema.add_attribute("PointSourceID", AttributeType::INT16, 1, 0, "Source from which this point originated");

  if (lasreader->point.extended_point_type)
  {
    header->schema.add_attribute("ScanAngle", AttributeType::FLOAT, 1, 0, "Angle at which the laser point was output");
    header->schema.add_attribute("ScannerChannel", AttributeType::UINT8, 1, 0, "Channel (scanner head) of a multi-channel system");
  }
  else
  {
    header->schema.add_attribute("ScanAngle", AttributeType::INT8, 1, 0, "Rounded angle at which the laser point was output");
  }

  if (lasreader->point.have_gps_time)
  {
    header->schema.add_attribute("gpstime", AttributeType::DOUBLE, 1, 0, "Time tag value at which the point was observed");
  }

  if (lasreader->point.have_rgb)
  {
    header->schema.add_attribute("R", AttributeType::UINT16, 1, 0, "Red image channel");
    header->schema.add_attribute("G", AttributeType::UINT16, 1, 0, "Green image channel");
    header->schema.add_attribute("B", AttributeType::UINT16, 1, 0, "Blue image channel");
  }

  if (lasreader->point.have_nir)
  {
    header->schema.add_attribute("NIR", AttributeType::UINT16, 1, 0, "Near infrared channel value");
  }

  for (int i = 0 ; i < lasreader->header.number_attributes ; i++)
  {
    std::string name(lasreader->header.attributes[i].name);
    std::string description(lasreader->header.attributes[i].description);
    AttributeType type = static_cast<AttributeType>(lasreader->header.attributes[i].data_type);
    double scale = lasreader->header.attributes[i].scale[0];
    double offset = lasreader->header.attributes[i].offset[0];
    header->schema.add_attribute(name, type, scale, offset, description);
  }

  reset_accessor();
  for (int i = 0 ; i < lasreader->header.number_attributes ; i++)
  {
    std::string name(lasreader->header.attributes[i].name);
    extrabytes.push_back(AttributeHandler(name));
  }

  this->header = header;

  return true;
}

// Streaming mode
bool LASRlasreader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);
  else
    point->zero();

  if (lasreader->read_point())
  {
    point->zero();
    point->set_X(lasreader->point.get_X());
    point->set_Y(lasreader->point.get_Y());
    point->set_Z(lasreader->point.get_Z());
    intensity(point, lasreader->point.get_intensity());
    returnnumber(point, lasreader->point.get_return_number());
    numberofreturns(point, lasreader->point.get_number_of_returns());
    classification(point, lasreader->point.get_classification());
    userdata(point, lasreader->point.get_user_data());
    psid(point, lasreader->point.get_point_source_ID());
    scanangle(point, lasreader->point.get_scan_angle());
    gpstime(point, lasreader->point.get_gps_time());
    scannerchannel(point, lasreader->point.get_extended_scanner_channel());
    red(point, lasreader->point.get_R());
    green(point, lasreader->point.get_G());
    blue(point, lasreader->point.get_B());
    nir(point, lasreader->point.get_NIR());
    for (int i = 0 ; i < lasreader->header.number_attributes ; i++)
      extrabytes[i](point, lasreader->point.get_attribute_as_float(i));

    if (point->inside_buffer(xmin, ymin, ymax, ymax, circular))
      point->set_buffered();
  }
  else
  {
    // In streaming mode this triggers a stop
    delete point;
    point = nullptr;
  }

  return true;
}

// In memory mode
bool LASRlasreader::process(LAS*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new LAS(header);

  streaming = false;

  progress->reset();
  progress->set_total(lasreader->npoints);
  progress->set_prefix("read_las");

  Point p(&header->schema);

  while (lasreader->read_point())
  {
    if (progress->interrupted()) break;

    p.zero();
    p.set_X(lasreader->point.get_X());
    p.set_Y(lasreader->point.get_Y());
    p.set_Z(lasreader->point.get_Z());
    intensity(&p, lasreader->point.get_intensity());
    returnnumber(&p, lasreader->point.get_return_number());
    numberofreturns(&p, lasreader->point.get_number_of_returns());
    classification(&p, lasreader->point.get_classification());
    userdata(&p, lasreader->point.get_user_data());
    psid(&p, lasreader->point.get_point_source_ID());
    scanangle(&p, lasreader->point.get_scan_angle());
    gpstime(&p, lasreader->point.get_gps_time());
    scannerchannel(&p, lasreader->point.get_extended_scanner_channel());
    red(&p, lasreader->point.get_R());
    green(&p, lasreader->point.get_G());
    blue(&p, lasreader->point.get_B());
    nir(&p, lasreader->point.get_NIR());
    for (int i = 0 ; i < lasreader->header.number_attributes ; i++)
      extrabytes[i](&p, lasreader->point.get_attribute_as_float(i));

    if (p.inside_buffer(xmin, ymin, xmax, ymax, circular))
      p.set_buffered();

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

void LASRlasreader::reset_accessor()
{
  intensity.reset();
  returnnumber.reset();
  numberofreturns.reset();
  userdata.reset();
  classification.reset();
  psid.reset();
  scanangle.reset();
  gpstime.reset();
  scannerchannel.reset();
  red.reset();
  green.reset();
  blue.reset();
  nir.reset();
  extrabytes.clear();
}

void LASRlasreader::clear(bool)
{
  // Called at the end of the pipeline. We can delete the header
  if (streaming && header)
  {
    delete header;
    header = nullptr;
  }
}