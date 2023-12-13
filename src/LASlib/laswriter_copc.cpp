/*
===============================================================================

  FILE:  laswriter_copc.cpp

  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    Jean-Romain Roussel

  COPYRIGHT:

    (c) 2023, rapidlasso GmbH - fast tools to catch reality

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the LICENSE.txt file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "lascopc.hpp"
#include "laswriter_copc.hpp"

#include "bytestreamout_nil.hpp"
#include "bytestreamout_file.hpp"
#include "bytestreamout_ostream.hpp"
#include "laswritepoint.hpp"
#include "geoprojectionconverter.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <stdlib.h>
#include <string.h>

static inline F64 get_gps_time(const U8* buf) { return *((F64*)&buf[22]); };
static inline U8 get_scanner_channel(const U8* buf) { return (buf[15] >> 4) & 0x03; };
static inline U8 get_return_number(const U8* buf) { return buf[14] & 0x0F; };
static int compare_buffers(const void *a, const void *b)
{
  if (get_gps_time((U8*)a) < get_gps_time((U8*)b)) return -1;
  if (get_gps_time((U8*)a) > get_gps_time((U8*)b)) return 1;
  if (get_scanner_channel((U8*)a) < get_scanner_channel((U8*)b)) return -1;
  if (get_scanner_channel((U8*)a) > get_scanner_channel((U8*)b)) return 1;
  if (get_return_number((U8*)a) < get_return_number((U8*)b)) return -1;
  return 1;
}

struct COPCoctant
{
  U8* point_buffer;
  I32 point_count;
  I32 point_size;
  I32 point_capacity;
  std::unordered_map<I32, I32> occupancy;

  COPCoctant(const U32 size)
  {
    point_size = size;
    point_count = 0;
    point_capacity = 25000;
    point_buffer = (U8*)malloc(point_capacity * point_size);
    occupancy.reserve(point_capacity);
  };

  void insert(const U8* buffer, const I32 cell)
  {
    if (point_count == point_capacity)
    {
      point_capacity *= 2;
      point_buffer = (U8*)realloc(point_buffer, point_capacity * point_size);
    }

    memcpy(point_buffer + point_count * point_size, buffer, point_size);

    // cell = -1 means that recording the location of the point is useless (save memory)
    if (cell >= 0) occupancy.insert({cell, point_count});

    point_count++;
  };

  void insert(const LASpoint *laspoint, const I32 cell)
  {
    if (point_count == point_capacity)
    {
      point_capacity *= 2;
      point_buffer = (U8*)realloc(point_buffer, point_capacity * point_size);
    }

    laspoint->copy_to(point_buffer + point_count * point_size);

    // cell = -1 means that recording the location of the point is useless (save memory)
    if (cell >= 0) occupancy.insert({cell, point_count});

    point_count++;
  };

  void sort()
  {
    qsort((void*)point_buffer, point_count, point_size, compare_buffers);
  };

  I32 npoints() const
  {
    return point_count;
  };

  void clean()
  {
    if (point_buffer) free(point_buffer);
  };
};

BOOL LASwriterCOPC::refile(FILE* file)
{
  return LASwriterLAS::refile(file);
}

BOOL LASwriterCOPC::open(const LASheader* header, I32 requested_version)
{
  make_copc_header(header);
  return LASwriterLAS::open(this->header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, requested_version, 0);
}

BOOL LASwriterCOPC::open(const char* file_name, const LASheader* header, I32 requested_version, I32 io_buffer_size)
{
  make_copc_header(header);
  return LASwriterLAS::open(file_name, this->header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, requested_version, 0, io_buffer_size);
}

BOOL LASwriterCOPC::open(FILE* file, const LASheader* header, I32 requested_version)
{
  make_copc_header(header);
  return LASwriterLAS::open(file, this->header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, requested_version, 0);
}

BOOL LASwriterCOPC::open(ostream& stream, const LASheader* header, I32 requested_version)
{
  make_copc_header(header);
  return LASwriterLAS::open(stream, this->header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, requested_version, 0);
}

BOOL LASwriterCOPC::open(ByteStreamOut* stream, const LASheader* header, I32 requested_version)
{
  make_copc_header(header);
  return LASwriterLAS::open(stream, this->header, LASZIP_COMPRESSOR_LAYERED_CHUNKED, 2, 0);
}

BOOL LASwriterCOPC::write_point(const LASpoint* point)
{
  if (capacity == 0)
  {
    capacity = 10000;
    points_buffer = (U8*)malloc(capacity * this->point->total_point_size);
  }

  if (capacity == p_count)
  {
    capacity = capacity*2;
    points_buffer = (U8*)realloc(points_buffer, capacity * this->point->total_point_size);
  }

  if (gpstime_maximum < point->get_gps_time()) gpstime_maximum = point->get_gps_time();
  if (gpstime_minimum > point->get_gps_time()) gpstime_minimum = point->get_gps_time();

  *this->point = *point; // format conversion
  this->point->copy_to(points_buffer + p_count*this->point->total_point_size);
  p_count++;
  return true;
}

BOOL LASwriterCOPC::chunk()
{
  return false;
}

BOOL LASwriterCOPC::update_header(const LASheader* header, BOOL use_inventory, BOOL update_extra_bytes)
{
  return false;
}

typedef std::unordered_map<EPTkey, COPCoctant, EPTKeyHasher> Registry;

I64 LASwriterCOPC::close(BOOL update_npoints)
{
  inventory.update_header(header);

  EPToctree octree(*header);
  octree.set_gridsize(root_grid_size);

  if (max_depth < 0)
  {
    max_depth = EPToctree::compute_max_depth(*header, max_points_per_octant);
    max_depth = (max_depth > 10) ? 10 : max_depth;
  }

  LASvlr_copc_info* info = (LASvlr_copc_info*)header->vlrs[0].data;
  info->center_x = octree.get_center_x();
  info->center_y = octree.get_center_y();
  info->center_z = octree.get_center_z();
  info->gpstime_maximum = gpstime_maximum;
  info->gpstime_minimum = gpstime_minimum;
  info->halfsize = octree.get_halfsize();
  info->spacing = (octree.get_halfsize() * 2) / octree.get_gridsize();
  info->root_hier_offset = 0; // delayed write when closing the writer
  info->root_hier_size = 0;   // delayed write when closing the writer

  // First, we shuffle the points
  I32 elem_size = point->total_point_size;
  U8* temp = (U8*)malloc(elem_size);
  for (I64 i = 0; i < p_count; i++)
  {
    I64 j = rand() % p_count;
    U8* block1 = points_buffer + i * elem_size;
    U8* block2 = points_buffer + j * elem_size;
    memcpy(temp, block1, elem_size);
    memcpy(block1, block2, elem_size);
    memcpy(block2, temp, elem_size);
  }
  free(temp);

  // The octree is an std::unordered_map
  Registry registry;
  Registry::iterator it;

  // EPT hierarchy
  std::vector<LASvlr_copc_entry> entries;

  // Delayed write
  for (I64 i = 0 ; i < p_count ; i++)
  {
    // Get a point
    point->copy_from(points_buffer+i*point->total_point_size);

    // Search a place to insert the point
    I32 lvl = 0;
    I32 cell = 0;
    bool accepted = false;
    while (!accepted)
    {
      EPTkey key = octree.get_key(point, lvl);
      //print("key %d-%d-%d-%d\n", key.d, key.x, key.y, key.z);

      if (lvl == max_depth)
        cell = -1; // Do not build an occupancy grid for last level. Point must be inserted anyway.
      else
        cell = octree.get_cell(point, key);

      it = registry.find(key);
      if (it == registry.end())
      {
        it = registry.insert({key, COPCoctant(elem_size)}).first;
      }

      auto it2 = it->second.occupancy.find(cell);
      accepted = (it2 == it->second.occupancy.end()) || (lvl == max_depth);

      lvl++;
    }

    // Insert the point
    it->second.insert(point, cell);
  }

  // We no longer need the buffeer of points
  free(points_buffer);
  points_buffer = 0;

  // All the octant are sorted
  for (it = registry.begin(); it != registry.end();)
  {
    // Bounding box of the octant
    F64 res = octree.get_size() / (1 << it->first.d);
    F64 minx = res * it->first.x + octree.get_xmin();
    F64 miny = res * it->first.y + octree.get_ymin();
    F64 minz = res * it->first.z + octree.get_zmin();
    F64 maxx = minx + res;
    F64 maxy = miny + res;
    F64 maxz = minz + res;

    // Check if the chunk is not too small. Otherwise, redistribute the points in the parent octant.
    // There is no guarantee that parents still exist. They may have already been written and freed.
    // (Requiring that chunks have more than min_points_per_octant is not a strong requirement,
    // but producing a LAZ chunks with only 2 or 3 points is suboptimal).
    if (it->second.npoints() <= min_points_per_octant)
    {
      bool moved = false;
      EPTkey key = it->first;
      while (key != EPTkey::root() && moved == false)
      {
        key = key.get_parent();
        auto it2 = registry.find(key);
        if (it2 != registry.end())
        {
          for (I32 k = 0; k < it->second.npoints(); k++)
            it2->second.insert(it->second.point_buffer + k * elem_size, -1);

          it->second.clean();

          // The octant must be inserted in the list because it may have childs
          if (it->first.d < max_depth)
          {
            LASvlr_copc_entry entry;
            entry.key.depth = it->first.d;
            entry.key.x = it->first.x;
            entry.key.y = it->first.y;
            entry.key.z = it->first.z;
            entry.point_count = 0;
            entry.offset = 0;
            entry.byte_size = 0;
            entries.push_back(entry);
          }

          it = registry.erase(it);
          moved = true;
        }
      }

      // Points were moved in another octant and the octant was deleted: we do not write this octant
      if (moved) continue;
    }

    // The octant is finalized: we can write the chunk and free up the memory
    LASvlr_copc_entry entry;
    entry.key.depth = it->first.d;
    entry.key.x = it->first.x;
    entry.key.y = it->first.y;
    entry.key.z = it->first.z;
    entry.point_count = it->second.npoints();
    entry.offset = LASwriterLAS::tell();

    // The points *MUST* be sorted (to optimize compression)
    it->second.sort();

    // Write the chunk
    for (I32 k = 0; k < it->second.npoints(); k++)
    {
      point->copy_from(it->second.point_buffer + k * elem_size);
      LASwriterLAS::write_point(point); p_count--;
    }
    LASwriterLAS::chunk();

    // Record the VLR entry
    entry.byte_size = (I32)(LASwriterLAS::tell() - entry.offset);
    entries.push_back(entry);

    // We will never see this octant again. Goodbye.
    it->second.clean();
    it = registry.erase(it);
  }

  // Construct the EPT hierarchy eVLR
  LASvlr_copc_entry* hierarchy = new LASvlr_copc_entry[entries.size()];
  std::copy(entries.begin(), entries.end(), hierarchy);
  header->evlrs[0].record_length_after_header = entries.size()*sizeof(LASvlr_copc_entry);
  header->evlrs[0].data = (U8*)hierarchy;

  LASwriterLAS::update_header(header, TRUE, TRUE);

  return LASwriterLAS::close(update_npoints);
}

BOOL LASwriterCOPC::make_copc_header(const LASheader* header)
{
  this->header = new LASheader;

  // Copy the header because the received one is const
  *this->header = *header;

  // zero the pointers of the other header so they don't get desallocated twice and they
  // the original header must not be modified anyway
  this->header->unlink();

  // Target PDRF if conversion required
  U8 target_point_data_format = 6;
  if (header->point_data_format == 2 || header->point_data_format == 3 || header->point_data_format == 5 || header->point_data_format ==  7) target_point_data_format = 7;
  if (header->point_data_format == 8) target_point_data_format = 8;
  this->header->point_data_format = (U8)target_point_data_format;

  // Upgrade to LAS 1.4
  if (header->version_minor < 3)
  {
    this->header->header_size += (8 + 140);
    this->header->offset_to_point_data += (8 + 140);
    this->header->start_of_waveform_data_packet_record = 0;
  }
  else if (header->version_minor == 3)
  {
    this->header->header_size += 140;
    this->header->offset_to_point_data += 140;
  }

  if (header->version_minor < 4)
  {
    this->header->version_minor = 4;
    this->header->extended_number_of_point_records = this->header->number_of_point_records;
    this->header->number_of_point_records = 0;
    for (U32 i = 0; i < 5; i++)
    {
      this->header->extended_number_of_points_by_return[i] = this->header->number_of_points_by_return[i];
      this->header->number_of_points_by_return[i] = 0;
    }
  }

  // Upgrade to point format > 5
  if (header->point_data_format < 6 || header->point_data_format > 8)
  {
    // were there extra bytes before
    I32 num_extra_bytes = 0;
    switch (header->point_data_format)
    {
    case 0: num_extra_bytes = header->point_data_record_length - 20; break;
    case 1: num_extra_bytes = header->point_data_record_length - 28; break;
    case 2: num_extra_bytes = header->point_data_record_length - 26; break;
    case 3: num_extra_bytes = header->point_data_record_length - 34; break;
    case 4: num_extra_bytes = header->point_data_record_length - 57; break;
    case 5: num_extra_bytes = header->point_data_record_length - 63; break;
    case 6: num_extra_bytes = header->point_data_record_length - 30; break;
    case 7: num_extra_bytes = header->point_data_record_length - 36; break;
    case 8: num_extra_bytes = header->point_data_record_length - 38; break;
    case 9: num_extra_bytes = header->point_data_record_length - 59; break;
    case 10: num_extra_bytes = header->point_data_record_length - 67; break;
    }
    if (num_extra_bytes < 0)
    {
      fprintf(stderr, "ERROR: point record length has %d fewer bytes than needed\n", num_extra_bytes);
      return false;
    }

    this->header->clean_laszip();
    switch (this->header->point_data_format)
    {
    case 6: this->header->point_data_record_length = 30 + num_extra_bytes; break;
    case 7: this->header->point_data_record_length = 36 + num_extra_bytes; break;
    case 8: this->header->point_data_record_length = 38 + num_extra_bytes; break;
    }
  }

  // For format conversion
  point = new LASpoint;
  point->init(this->header, this->header->point_data_format, this->header->point_data_record_length);

  // Placeholder of the COPC info VLR. The actual content is resolved when closing the writer.
  LASvlr_copc_info *info = new LASvlr_copc_info[1];
  memset(info, 0, sizeof(LASvlr_copc_info));
  this->header->add_vlr("copc", 1, sizeof(LASvlr_copc_info), (U8*)info, FALSE, "copc info");

  // Placeholder to write a proper header. The actual content is resolved when closing the writer.
  this->header->add_evlr("copc", 1000, 0, 0, FALSE, "EPT hierarchy");

  // Deep copy of the original VLR and EVLR
  for (I32 i = 0; i < header->number_of_variable_length_records; i++)
  {
    LASvlr* vlr = &header->vlrs[i];
    U8* data = new U8[vlr->record_length_after_header];
    memcpy(data, vlr->data, vlr->record_length_after_header);
    this->header->add_vlr(vlr->user_id, vlr->record_id, vlr->record_length_after_header, data, FALSE, vlr->description);
  }
  for (I32 i = 0; i < header->number_of_extended_variable_length_records; i++)
  {
    LASevlr* evlr = &header->evlrs[i];
    U8* data = new U8[evlr->record_length_after_header];
    memcpy(data, evlr->data, evlr->record_length_after_header);
    this->header->add_evlr(evlr->user_id, evlr->record_id, evlr->record_length_after_header, data, FALSE, evlr->description);
  }

  // convert CRS from GeoTIF to OGC WKT
  if (header->vlr_geo_keys)
  {
    GeoProjectionConverter geoprojectionconverter;
    char* ogc_wkt = 0;
    I32 len = 0;
    geoprojectionconverter.set_projection_from_geo_keys(header->vlr_geo_keys[0].number_of_keys, (GeoProjectionGeoKeys*)header->vlr_geo_key_entries, header->vlr_geo_ascii_params, header->vlr_geo_double_params);
    if (!geoprojectionconverter.get_ogc_wkt_from_projection(len, &ogc_wkt))
    {
      REprintf("WARNING: cannot convert CRS from GeoTIFF to OGC WKT.\n");
    }

    if (ogc_wkt)
    {
      this->header->set_global_encoding_bit(LAS_TOOLS_GLOBAL_ENCODING_BIT_OGC_WKT_CRS);
      this->header->remove_vlr("LASF_Projection", 34735);
      this->header->del_geo_ascii_params();
      this->header->del_geo_double_params();
      this->header->set_geo_ogc_wkt(len, ogc_wkt);
      free(ogc_wkt);
    }
  }

  return true;
}

LASwriterCOPC::LASwriterCOPC()
{
  points_buffer = 0;
  capacity = 0;
  gpstime_maximum = F64_MIN;
  gpstime_minimum = F64_MAX;
  max_depth = -1;
  root_grid_size = 256;
  max_points_per_octant = 100000; // not absolute, only used to estimate the depth.
  min_points_per_octant = 100;    // not absolute, use to (maybe) remove too small chunks
}

LASwriterCOPC::~LASwriterCOPC()
{
  if (points_buffer) free(points_buffer);
  if (point) delete point;
  if (header) delete header;
}
