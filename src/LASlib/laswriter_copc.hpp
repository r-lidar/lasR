/*
===============================================================================

  FILE:  laswriter_copc.hpp

  CONTENTS:

    Writes LiDAR points to the COPC format

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

    11 December 2023 -- creation of LASwriterCOPC

===============================================================================
*/

#ifndef LAS_WRITER_COPC_HPP
#define LAS_WRITER_COPC_HPP

#include "laswriter.hpp"
#include "laswriter_las.hpp"

#include <stdio.h>

#ifdef LZ_WIN32_VC6
#include <fstream.h>
#else
#include <istream>
#include <fstream>
using namespace std;
#endif

class ByteStreamOut;
class LASwritePoint;

class LASwriterCOPC : public LASwriterLAS
{
public:
  BOOL open(const LASheader* header, I32 requested_version=0);
  BOOL open(const char* file_name, const LASheader* header, I32 requested_version=0, I32 io_buffer_size=LAS_TOOLS_IO_OBUFFER_SIZE);
  BOOL open(FILE* file, const LASheader* header, I32 requested_version=0);
  BOOL open(ostream& ostream, const LASheader* header, I32 requested_version=0);
  BOOL open(ByteStreamOut* stream, const LASheader* header, I32 requested_version=0);

  BOOL write_point(const LASpoint* point);
  BOOL chunk();

  BOOL update_header(const LASheader* header, BOOL use_inventory=FALSE, BOOL update_extra_bytes=FALSE);
  I64 close(BOOL update_npoints=TRUE);

  LASwriterCOPC();
  ~LASwriterCOPC();

private:
  BOOL make_copc_header(const LASheader* header);

private:
  // stores point in memory to delay write
  LASpoint* point;
  LASheader* header;
  U8* points_buffer;
  U64 capacity;

  // COPC information for (E)VLR and octree
  F64 gpstime_maximum;
  F64 gpstime_minimum;
  U32 root_grid_size;
  U32 max_points_per_octant;
  I32 min_points_per_octant;
};

#endif
