#include "LASio.h"
#include "Progress.h"
#include "error.h"

#include "lasreader.hpp"
#include "laswriter.hpp"
#include "laszip_decompress_selective_v3.hpp"
#include "lasindex.hpp"
#include "lasquadtree.hpp"

LASio::LASio()
{
  lasreadopener = nullptr;
  laswriteopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;
  laswriter = nullptr;
  point = nullptr;
  progress = nullptr;

  intensity = AttributeAccessor("Intensity");
  returnnumber = AttributeAccessor("ReturnNumber");
  numberofreturns = AttributeAccessor("NumberOfReturns");
  userdata = AttributeAccessor("UserData");
  classification = AttributeAccessor("Classification");
  psid = AttributeAccessor("PointSourceID");
  scanangle = AttributeAccessor("ScanAngle");
  gpstime = AttributeAccessor("gpstime");
  scannerchannel = AttributeAccessor("ScannerChannel");
  red = AttributeAccessor("R");
  green = AttributeAccessor("G");
  blue = AttributeAccessor("B");
  nir = AttributeAccessor("NIR");
  eof_bit = AttributeAccessor("EdgeOfFlightline");
  scandirection_bit = AttributeAccessor("ScanDirectionFlag");
  withheld_bit = AttributeAccessor("Withheld");
  synthetic_bit = AttributeAccessor("Synthetic");
  keypoint_bit = AttributeAccessor("Keypoint");
  overlap_bit = AttributeAccessor("Overlap");

  copc_depth = -1;
  copc_density = 256;
}

LASio::LASio(Progress* progress) : LASio()
{
  this->progress = progress;
}

LASio::~LASio()
{
  close();
}

bool LASio::open(const Chunk& chunk, std::vector<std::string> filters)
{
  if (laswriter)
  {
    last_error = "Internal error. This interface has been created as a writer"; // # nocov
    return false;
  }

  // Keep only LASlib string that start with - (e.g. -drop_z_above 50) and drop condition
  // like Z > 50
  std::string sfilter;
  for (const auto& filter : filters)
  {
    // Trim leading spaces
    size_t start = filter.find_first_not_of(" \t");
    std::string trimmed = (start == std::string::npos) ? "" : filter.substr(start);

    // Check if the trimmed string starts with '-'
    if (!trimmed.empty() && trimmed[0] == '-') sfilter += trimmed + " ";
  }

  //print("LASlib filters = '%s'\n", sfilter.c_str());

  const char* tmp = sfilter.c_str();
  int n = strlen(tmp)+1;
  char* filtercpy = (char*)malloc(n); memcpy(filtercpy, tmp, n);

  // The openner must survive to the reader otherwise there are some pointer invalidation.
  lasreadopener = new LASreadOpener;
  lasreadopener->set_merged(true);
  lasreadopener->set_stored(false);
  //lasreadopener->set_populate_header(true);
  //lasreadopener->set_buffer_size(chunk.buffer);
  lasreadopener->parse_str(filtercpy);
  lasreadopener->set_copc_stream_ordered_by_chunk();

  free(filtercpy);

  for (auto& file : chunk.main_files) lasreadopener->add_file_name(file.c_str(), TRUE);
  for (auto& file : chunk.neighbour_files) lasreadopener->add_file_name(file.c_str(), TRUE);

  if (chunk.shape == ShapeType::CIRCLE)
    lasreadopener->set_inside_circle((chunk.xmin+chunk.xmax)/2, (chunk.ymin+chunk.ymax)/2,  (chunk.xmax-chunk.xmin)/2 + chunk.buffer + EPSILON);
  else
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer - EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);

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

bool LASio::open(const std::string& file)
{
  if (lasheader != nullptr)
  {
    last_error = "Internal error. LASheader already initialized."; // # nocov
    return false;
  }

  if (laswriter)
  {
    last_error = "Internal error. This interface has been created as a writer"; // # nocov
    return false;
  }

  lasreadopener = new LASreadOpener;
  lasreadopener->add_file_name(file.c_str());
  lasreader = lasreadopener->open();

  if (!lasreader)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, 512, "LASlib internal error. Cannot open LASreader with %s\n", file.c_str());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  return true;
}

bool LASio::create(const std::string& file)
{
  if (lasheader == nullptr)
  {
    last_error = "Internal error. LASheader not initialized."; // # nocov
    return false;
  }

  if (lasreader)
  {
    last_error = "Internal error. This interface has been created as a reader"; // # nocov
    return false;
  }


  laswriteopener = new LASwriteOpener;
  laswriteopener->set_file_name(file.c_str());
  laswriter = laswriteopener->open(lasheader);

  if (!laswriter)
  {
    last_error = "LASlib internal error. Cannot open LASwriter."; // # nocov
    return false; // # nocov
  }

  laswriter->set_copc_depth(copc_depth);
  if (copc_density <= 64)
    laswriter->set_copc_sparse();
  else if (copc_density > 64 & copc_density <= 128)
    laswriter->set_copc_normal();
  else
    laswriter->set_copc_dense();

  return true;
}

bool LASio::populate_header(Header* header, bool read_first_point)
{
  if (lasreader == nullptr)
  {
    last_error = "Internal error. LASreader not initialized."; // # nocov
    return false;
  }

  reset_accessor();

  header->signature = "LASF";
  header->version_major = lasreader->header.version_major;
  header->version_minor = lasreader->header.version_minor;
  header->point_data_format = lasreader->header.point_data_format;

  header->min_x = lasreader->header.min_x;
  header->max_x = lasreader->header.max_x;
  header->min_y = lasreader->header.min_y;
  header->max_y = lasreader->header.max_y;
  header->min_z = lasreader->header.min_z;
  header->max_z = lasreader->header.max_z;
  header->number_of_point_records = MAX(lasreader->header.number_of_point_records, lasreader->header.extended_number_of_point_records);
  header->x_offset = lasreader->header.x_offset;
  header->y_offset = lasreader->header.y_offset;
  header->z_offset = lasreader->header.z_offset;
  header->x_scale_factor = lasreader->header.x_scale_factor;
  header->y_scale_factor = lasreader->header.y_scale_factor;
  header->z_scale_factor = lasreader->header.z_scale_factor;
  header->file_creation_year = lasreader->header.file_creation_year;
  header->file_creation_day = lasreader->header.file_creation_day;
  header->adjusted_standard_gps_time = lasreader->header.get_global_encoding_bit(0) == true;

  header->schema.add_attribute("flags", AttributeType::UINT8, 1, 0, "Internal 8-bit mask reserved for lasR core engine");
  header->schema.add_attribute("X", AttributeType::INT32, header->x_scale_factor, header->x_offset, "X coordinate");
  header->schema.add_attribute("Y", AttributeType::INT32, header->y_scale_factor, header->y_offset, "Y coordinate");
  header->schema.add_attribute("Z", AttributeType::INT32, header->z_scale_factor, header->z_offset, "Z coordinate");
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
    int data_type = lasreader->header.attributes[i].data_type;
    if (data_type > 10) continue; // Don't read deprecated types
    AttributeType type = static_cast<AttributeType>(data_type);
    double scale = lasreader->header.attributes[i].scale[0];
    double offset = lasreader->header.attributes[i].offset[0];
    header->schema.add_attribute(name, type, scale, offset, description);
    extrabytes.push_back(AttributeAccessor(name));
  }

  header->schema.add_attribute("EdgeOfFlightline", AttributeType::BIT, 1, 0, "Set when the point is at the end of a scan");
  header->schema.add_attribute("ScanDirectionFlag", AttributeType::BIT, 1, 0, "Direction in which the scanner mirror was traveling ");
  header->schema.add_attribute("Synthetic", AttributeType::BIT, 1, 0, "Point created by a technique other than direct observation");
  header->schema.add_attribute("Keypoint", AttributeType::BIT, 1, 0, "Point is considered to be a model key-point");
  header->schema.add_attribute("Withheld", AttributeType::BIT, 1, 0, "Point is supposed to be deleted)");

  if (lasreader->point.extended_point_type)
    header->schema.add_attribute("Overlap", AttributeType::BIT, 1, 0, "If set, point is within an overlap region of 2+ swaths");

  if (lasreader->header.vlr_geo_keys)
  {
    for (int j = 0; j < lasreader->header.vlr_geo_keys->number_of_keys; j++)
    {
      if (lasreader->header.vlr_geo_key_entries[j].key_id == 3072)
      {
        header->set_crs((int)lasreader->header.vlr_geo_key_entries[j].value_offset);
        break;
      }
    }
  }

  if (lasreader->header.vlr_geo_ogc_wkt)
  {
    for (unsigned int j = 0; j < lasreader->header.number_of_variable_length_records; j++)
    {
      if (lasreader->header.vlrs[j].record_id == 2112)
      {
        char* data = (char*)lasreader->header.vlrs[j].data;
        int len = strnlen(data, lasreader->header.vlrs[j].record_length_after_header);
        std::string wkt(data, len);
        header->set_crs(wkt);
        break;
      }
    }
  }

  header->spatial_index = (lasreader->get_index() != nullptr) || (lasreader->get_copcindex() != nullptr);

  if (read_first_point)
  {
    lasreader->read_point();
    header->gpstime = lasreader->point.get_gps_time();
    lasreader->seek(0);
  }
  return true;
}

bool LASio::init(const Header* header)
{
  if (lasheader != nullptr)
  {
    last_error = "Internal error. LASheader is already initialized."; // # nocov
    return false;
  }

  if (lasreader)
  {
    last_error = "Internal error. This interface has been created as a reader"; // # nocov
    return false;
  }

  int version_minor = 2;

  bool has_gps = header->schema.find_attribute("gpstime") != nullptr;
  bool has_rgb = header->schema.find_attribute("R") != nullptr;
  bool has_nir = header->schema.find_attribute("NIR") != nullptr;
  bool has_overlap = header->schema.find_attribute("Overlap") != nullptr;

  int pdf = guess_point_data_format(has_gps, has_rgb, has_nir, has_overlap);

  if (header->signature != "LASF")
    version_minor = (pdf >= 6) ? 4 : 2;
  else
    version_minor = header->version_minor;

  lasheader = new LASheader();
  lasheader->file_source_ID       = 0;
  lasheader->version_major        = 1;
  lasheader->version_minor        = version_minor;
  lasheader->header_size          = get_header_size(version_minor);
  lasheader->offset_to_point_data = get_header_size(version_minor);
  lasheader->file_creation_year   = 0;
  lasheader->file_creation_day    = 0;
  lasheader->point_data_format    = pdf;
  lasheader->point_data_record_length = get_point_data_record_length(lasheader->point_data_format);
  lasheader->x_scale_factor       = header->schema.attributes[AttributeCore::X].scale_factor;
  lasheader->y_scale_factor       = header->schema.attributes[AttributeCore::Y].scale_factor;
  lasheader->z_scale_factor       = header->schema.attributes[AttributeCore::Z].scale_factor;
  lasheader->x_offset             = header->schema.attributes[AttributeCore::X].value_offset;
  lasheader->y_offset             = header->schema.attributes[AttributeCore::Y].value_offset;
  lasheader->z_offset             = header->schema.attributes[AttributeCore::Z].value_offset;
  lasheader->number_of_point_records = 0;
  /*lasheader->min_x                = xmin;
  lasheader->min_y                = ymin;
  lasheader->max_x                = xmax;
  lasheader->max_y                = ymax;*/

  if (header->adjusted_standard_gps_time)
    lasheader->set_global_encoding_bit(0);

  reset_accessor();

  extrabytes_offsets.clear();
  for (int i = 0 ; i < header->schema.attributes.size() ; i++)
  {
    const Attribute& attribute = header->schema.attributes[i];

    if (lascoreattributes.count(attribute.name) == 0)
    {
      LASattribute attr(attribute.type-1, attribute.name.c_str(), attribute.description.c_str());
      attr.set_scale(attribute.scale_factor);
      attr.set_offset(attribute.value_offset);
      lasheader->add_attribute(attr);
      extrabytes_offsets.push_back(attribute.offset);
    }
  }

  lasheader->update_extra_bytes_vlr();
  lasheader->point_data_record_length += lasheader->get_attributes_size();

  if (!header->get_wkt().empty())
  {
    std::string wkt = header->get_wkt();
    lasheader->set_global_encoding_bit(4);
    lasheader->set_geo_ogc_wkt(wkt.size(), wkt.c_str(), false);
  }

  if (point) delete point;

  point = new LASpoint;
  point->init(lasheader, lasheader->point_data_format, lasheader->point_data_record_length, lasheader);

  reset_accessor();

  return true;
}

bool LASio::read_point(Point* p)
{
  if (!lasreader->read_point()) return false;

  p->zero();
  p->set_X(lasreader->point.get_X());
  p->set_Y(lasreader->point.get_Y());
  p->set_Z(lasreader->point.get_Z());
  intensity(p, lasreader->point.get_intensity());
  returnnumber(p, lasreader->point.get_return_number());
  numberofreturns(p, lasreader->point.get_number_of_returns());
  classification(p, lasreader->point.get_classification());
  userdata(p, lasreader->point.get_user_data());
  psid(p, lasreader->point.get_point_source_ID());
  scanangle(p, lasreader->point.get_scan_angle());
  gpstime(p, lasreader->point.get_gps_time());
  scannerchannel(p, lasreader->point.get_extended_scanner_channel());
  red(p, lasreader->point.get_R());
  green(p, lasreader->point.get_G());
  blue(p, lasreader->point.get_B());
  nir(p, lasreader->point.get_NIR());
  for (int i = 0 ; i < lasreader->header.number_attributes ; i++)
  {
    if (lasreader->header.attributes[i].data_type > 10) continue; // Don't read deprecated types
    extrabytes[i](p, lasreader->point.get_attribute_as_float(i));
  }
  eof_bit(p, lasreader->point.get_edge_of_flight_line());
  scandirection_bit(p, lasreader->point.get_scan_direction_flag());
  withheld_bit(p, lasreader->point.get_withheld_flag());
  synthetic_bit(p, lasreader->point.get_synthetic_flag());
  keypoint_bit(p, lasreader->point.get_keypoint_flag());
  overlap_bit(p, lasreader->point.get_extended_overlap_flag());
  return true;
}

bool LASio::write_point(Point* p)
{
  point->set_x(p->get_x());
  point->set_y(p->get_y());
  point->set_z(p->get_z());
  point->set_intensity(intensity(p));
  point->set_return_number(returnnumber(p));
  point->set_number_of_returns(numberofreturns(p));
  point->set_user_data(userdata(p));
  point->set_point_source_ID(psid(p));
  point->set_classification(classification(p));
  point->set_scan_angle(scanangle(p));
  point->set_gps_time(gpstime(p));
  point->set_R(red(p));
  point->set_G(green(p));
  point->set_B(blue(p));
  point->set_NIR(nir(p));
  point->set_edge_of_flight_line(eof_bit(p));
  point->set_scan_direction_flag(scandirection_bit(p));
  point->set_synthetic_flag(synthetic_bit(p));
  point->set_withheld_flag(withheld_bit(p));
  point->set_keypoint_flag(keypoint_bit(p));

  point->set_extended_overlap_flag(overlap_bit(p));
  point->set_extended_scanner_channel(scannerchannel(p));
  point->set_extended_return_number(returnnumber(p));
  point->set_extended_number_of_returns(numberofreturns(p));
  point->set_extended_classification(classification(p));

  for (int i = 0 ; i < extrabytes_offsets.size() ; i++)
    point->set_attribute(i, p->data + extrabytes_offsets[i]);

  laswriter->write_point(point);
  laswriter->update_inventory(point);

  return true;
}

bool LASio::write_lax(const std::string& file, bool overwrite, bool embedded)
{
  // Initialize las objects
  const char* filechar = const_cast<char*>(file.c_str());
  LASreadOpener lasreadopener;
  lasreadopener.set_file_name(filechar);
  LASreader* lasreader = lasreadopener.open();

  if (!lasreader)
  {
    last_error = "LASlib internal error"; // # nocov
    return false; // # nocov
  }

  // This file is already copc indexed. Exit
  if (lasreader->get_copcindex())
  {
    return true;
  }

  // This file is already indexed
  if (lasreader->get_index() && !overwrite)
  {
    lasreader->close();
    delete lasreader;
    return true;
  }

  lasreadopener.set_decompress_selective(LASZIP_DECOMPRESS_SELECTIVE_CHANNEL_RETURNS_XY);

  // setup the quadtree
  LASquadtree* lasquadtree = new LASquadtree;

  float w = lasreader->header.max_x - lasreader->header.min_x;
  float h = lasreader->header.max_y - lasreader->header.min_y;
  F32 t;

  if ((w < 1000) && (h < 1000))
    t = 10.0;
  else if ((w < 10000) && (h < 10000)) // # nocov start
    t = 100.0;
  else if ((w < 100000) && (h < 100000))
    t = 1000.0;
  else if ((w < 1000000) && (h < 1000000))
    t = 10000.0;
  else
    t = 100000.0; // # nocov end

  lasquadtree->setup(lasreader->header.min_x, lasreader->header.max_x, lasreader->header.min_y, lasreader->header.max_y, t);

  uint64_t n = MAX(lasreader->header.number_of_point_records, lasreader->header.extended_number_of_point_records);

  LASindex lasindex;
  lasindex.prepare(lasquadtree, 1000);

  progress->reset();
  progress->set_prefix("Write LAX");
  progress->set_total(n);

  while (lasreader->read_point())
  {
    lasindex.add(lasreader->point.get_x(), lasreader->point.get_y(), (U32)(lasreader->p_count-1));
    (*progress)++;
    progress->show();
  }

  lasreader->close();
  delete lasreader;

  int minimum_points = 100000;
  int maximum_intervals = -20;
  lasindex.complete(minimum_points, maximum_intervals, false);

  if (embedded)
  {
    if ( !lasindex.append(lasreadopener.get_file_name()))
    {
      lasindex.write(lasreadopener.get_file_name());
    }
  }
  else
  {
    lasindex.write(lasreadopener.get_file_name());
  }

  progress->done();

  return true;
}

bool LASio::is_opened()
{
  return (lasreader != nullptr || laswriter != nullptr);
}

int64_t LASio::p_count()
{
  return lasreader->p_count;
}

void LASio::close()
{
  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
    lasheader = nullptr;
  }

  if (laswriter)
  {
    laswriter->update_header(lasheader, true);
    laswriter->close();
    delete laswriter;
    laswriter = nullptr;

    delete lasheader;
    lasheader = nullptr;
  }

  if (lasreadopener)
  {
    delete lasreadopener;
    lasreadopener = nullptr;
  }

  if (laswriteopener)
  {
    delete laswriteopener;
    laswriteopener = nullptr;
  }

  if (point)
  {
    delete point;
    point = nullptr;
  }
}

void LASio::reset_accessor()
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
  eof_bit.reset();
  scandirection_bit.reset();
  withheld_bit.reset();
  synthetic_bit.reset();
  keypoint_bit.reset();
  overlap_bit.reset();
  for (auto& accessor : extrabytes)
    accessor.reset();
}


int LASio::guess_point_data_format(bool has_gps, bool has_rgb, bool has_nir, bool has_overlap)
{
  if (has_nir) return 8; // NIR requires format 8 or 10 (fwf), choose 8

  // Start with all possible formats
  std::vector<int> formats = {0, 1, 2, 3, 6, 7, 8};

  // Filter formats based on conditions
  formats.erase(std::remove_if(formats.begin(), formats.end(), [&](int format)
  {
    if (has_overlap && (format < 6)) return true;                            // Overlap requires 6, 7, 8
    if (has_gps && (format == 0 || format == 2)) return true;                // GPS excludes 0 and 2
    if (has_rgb && (format == 0 || format == 1 || format == 6)) return true; // RGB excludes 0, 1, 6
    return false;
  }), formats.end());

  // Return the first remaining format (or handle error if none remain)
  return formats.empty() ? 0 : formats[0];
}

int LASio::get_header_size(int minor_version)
{
  int header_size = 0;

  switch (minor_version)
  {
  case 0:
  case 1:
  case 2:
    header_size = 227;
    break;
  case 3:
    header_size = 235;
    break;
  case 4:
    header_size = 375;
    break;
  default:
    header_size = -1;
  break;
  }

  return header_size;
}

int LASio::get_point_data_record_length(int point_data_format, int num_extrabytes)
{
  switch (point_data_format)
  {
  case 0: return 20 + num_extrabytes; break;
  case 1: return 28 + num_extrabytes; break;
  case 2: return 26 + num_extrabytes; break;
  case 3: return 34 + num_extrabytes; break;
  case 4: return 57 + num_extrabytes; break;
  case 5: return 63 + num_extrabytes; break;
  case 6: return 30 + num_extrabytes; break;
  case 7: return 36 + num_extrabytes; break;
  case 8: return 38 + num_extrabytes; break;
  case 9: return 59 + num_extrabytes; break;
  case 10: return 67 + num_extrabytes; break;
  default: return 0; break;
  }
}

void LASio::set_copc_max_depth(int depth)
{
  copc_depth = depth;
}
void LASio::set_copc_density(int density)
{
  copc_density = density;
}
