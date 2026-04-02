/*
===============================================================================

  FILE:  bytestreamin_gdal.hpp

  CONTENTS:

    Class for VSILFILE*-based input streams with endian handling.
    Uses GDAL virtual filesystem I/O so files can be read over HTTP
    via /vsicurl/ and other GDAL virtual filesystems.

  PROGRAMMERS:

    jean-romain.roussel.1@ulaval.ca

  COPYRIGHT:

    (c) 2026, Jean-Romain Roussel

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the COPYING file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

     2 April 2026 -- created from ByteStreamInFile, replacing FILE* with VSILFILE*

===============================================================================
*/
#ifdef USING_GDAL

#ifndef BYTE_STREAM_IN_GDAL_H
#define BYTE_STREAM_IN_GDAL_H

#include "bytestreamin.hpp"

#include <cpl_vsi.h>

class ByteStreamInGDAL : public ByteStreamIn
{
public:
  ByteStreamInGDAL(VSILFILE* file);
/* read a single byte                                        */
  U32 getByte();
/* read an array of bytes                                    */
  void getBytes(U8* bytes, const U32 num_bytes);
/* is the stream seekable (e.g. stdin is not)                */
  BOOL isSeekable() const;
/* get current position of stream                            */
  I64 tell() const;
/* seek to this position in the stream                       */
  BOOL seek(const I64 position);
/* seek to the end of the file                               */
  BOOL seekEnd(const I64 distance=0);
/* destructor                                                */
  ~ByteStreamInGDAL();
protected:
  VSILFILE* file;
};

class ByteStreamInGDALLE : public ByteStreamInGDAL
{
public:
  ByteStreamInGDALLE(VSILFILE* file);
/* read 16 bit low-endian field                              */
  void get16bitsLE(U8* bytes);
/* read 32 bit low-endian field                              */
  void get32bitsLE(U8* bytes);
/* read 64 bit low-endian field                              */
  void get64bitsLE(U8* bytes);
/* read 16 bit big-endian field                              */
  void get16bitsBE(U8* bytes);
/* read 32 bit big-endian field                              */
  void get32bitsBE(U8* bytes);
/* read 64 bit big-endian field                              */
  void get64bitsBE(U8* bytes);
private:
  U8 swapped[8];
};

class ByteStreamInGDALBE : public ByteStreamInGDAL
{
public:
  ByteStreamInGDALBE(VSILFILE* file);
/* read 16 bit low-endian field                              */
  void get16bitsLE(U8* bytes);
/* read 32 bit low-endian field                              */
  void get32bitsLE(U8* bytes);
/* read 64 bit low-endian field                              */
  void get64bitsLE(U8* bytes);
/* read 16 bit big-endian field                              */
  void get16bitsBE(U8* bytes);
/* read 32 bit big-endian field                              */
  void get32bitsBE(U8* bytes);
/* read 64 bit big-endian field                              */
  void get64bitsBE(U8* bytes);
private:
  U8 swapped[8];
};

inline ByteStreamInGDAL::ByteStreamInGDAL(VSILFILE* file)
{
  this->file = file;
}

inline ByteStreamInGDAL::~ByteStreamInGDAL()
{
  if (file)
  {
    VSIFCloseL(file);
    file = NULL;
  }
}

inline U32 ByteStreamInGDAL::getByte()
{
  U8 byte;
  if (VSIFReadL(&byte, 1, 1, file) != 1)
  {
    throw EOF;
  }
  return (U32)byte;
}

inline void ByteStreamInGDAL::getBytes(U8* bytes, const U32 num_bytes)
{
  if (VSIFReadL(bytes, 1, num_bytes, file) != num_bytes)
  {
    throw EOF;
  }
}

inline BOOL ByteStreamInGDAL::isSeekable() const
{
  return TRUE;
}

inline I64 ByteStreamInGDAL::tell() const
{
  return (I64)VSIFTellL(file);
}

inline BOOL ByteStreamInGDAL::seek(const I64 position)
{
  if (tell() != position)
  {
    int result = VSIFSeekL(file, (vsi_l_offset)position, SEEK_SET);
    return !result;
  }
  return TRUE;
}

inline BOOL ByteStreamInGDAL::seekEnd(const I64 distance)
{
  int result = VSIFSeekL(file, (vsi_l_offset)(-distance), SEEK_END);
  return !result;
}

inline ByteStreamInGDALLE::ByteStreamInGDALLE(VSILFILE* file) : ByteStreamInGDAL(file)
{
}

inline void ByteStreamInGDALLE::get16bitsLE(U8* bytes)
{
  getBytes(bytes, 2);
}

inline void ByteStreamInGDALLE::get32bitsLE(U8* bytes)
{
  getBytes(bytes, 4);
}

inline void ByteStreamInGDALLE::get64bitsLE(U8* bytes)
{
  getBytes(bytes, 8);
}

inline void ByteStreamInGDALLE::get16bitsBE(U8* bytes)
{
  getBytes(swapped, 2);
  bytes[0] = swapped[1];
  bytes[1] = swapped[0];
}

inline void ByteStreamInGDALLE::get32bitsBE(U8* bytes)
{
  getBytes(swapped, 4);
  bytes[0] = swapped[3];
  bytes[1] = swapped[2];
  bytes[2] = swapped[1];
  bytes[3] = swapped[0];
}

inline void ByteStreamInGDALLE::get64bitsBE(U8* bytes)
{
  getBytes(swapped, 8);
  bytes[0] = swapped[7];
  bytes[1] = swapped[6];
  bytes[2] = swapped[5];
  bytes[3] = swapped[4];
  bytes[4] = swapped[3];
  bytes[5] = swapped[2];
  bytes[6] = swapped[1];
  bytes[7] = swapped[0];
}

inline ByteStreamInGDALBE::ByteStreamInGDALBE(VSILFILE* file) : ByteStreamInGDAL(file)
{
}

inline void ByteStreamInGDALBE::get16bitsLE(U8* bytes)
{
  getBytes(swapped, 2);
  bytes[0] = swapped[1];
  bytes[1] = swapped[0];
}

inline void ByteStreamInGDALBE::get32bitsLE(U8* bytes)
{
  getBytes(swapped, 4);
  bytes[0] = swapped[3];
  bytes[1] = swapped[2];
  bytes[2] = swapped[1];
  bytes[3] = swapped[0];
}

inline void ByteStreamInGDALBE::get64bitsLE(U8* bytes)
{
  getBytes(swapped, 8);
  bytes[0] = swapped[7];
  bytes[1] = swapped[6];
  bytes[2] = swapped[5];
  bytes[3] = swapped[4];
  bytes[4] = swapped[3];
  bytes[5] = swapped[2];
  bytes[6] = swapped[1];
  bytes[7] = swapped[0];
}

inline void ByteStreamInGDALBE::get16bitsBE(U8* bytes)
{
  getBytes(bytes, 2);
}

inline void ByteStreamInGDALBE::get32bitsBE(U8* bytes)
{
  getBytes(bytes, 4);
}

inline void ByteStreamInGDALBE::get64bitsBE(U8* bytes)
{
  getBytes(bytes, 8);
}

#endif
#endif
