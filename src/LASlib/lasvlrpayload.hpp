/*
===============================================================================

  FILE:  lasvlrpayload.hpp
  
  CONTENTS:
  
    This class describes a few useful payloads for Variable Length Records
    (VLRs) and Extended Variable Length Records (EVLRs) used by LASlib.

  PROGRAMMERS:

    martin.isenburg@rapidlasso.com  -  http://rapidlasso.com

  COPYRIGHT:

    (c) 2007-2019, martin isenburg, rapidlasso - fast tools to catch reality

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the LICENSE.txt file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
     7 July 2019 -- created after US beat NL 2:0 in Women World Cup Final
  
===============================================================================
*/
#ifndef LAS_VLR_PAYLOAD_HPP
#define LAS_VLR_PAYLOAD_HPP

#include "bytestreamin_array.hpp"
#include "bytestreamout_array.hpp"

class LASvlrPayload
{
public:
  CHAR user_id[16]; 
  virtual U16 get_record_id() const = 0;
  virtual const CHAR* get_user_id() const = 0;
  virtual I64 get_payload_size() const = 0;
  virtual U8* get_payload() const = 0;
  virtual BOOL set_payload(const U8* payload, I64 size) = 0;
  virtual ~LASvlrPayload() {};
protected:
  virtual BOOL save(ByteStreamOut* stream) const = 0;
  virtual BOOL load(ByteStreamIn* stream) = 0;
};

class LASvlrRasterLAZ : public LASvlrPayload
{
public:
  I32 nbands;
  I32 nbits;
  I32 ncols;
  I32 nrows;
  U32 reserved1;
  U32 reserved2;
  F64 stepx;
  F64 stepx_y;
  F64 stepy;
  F64 stepy_x;
  F64 llx;
  F64 lly;
  F64 sigmaxy;  // horizontal uncertainty [meters] 
  LASvlrRasterLAZ()
  {
    memcpy(user_id, "Raster LAZ\0\0\0\0\0", 16);
    nbands = -1;
    nbits = -1;
    ncols = -1;
    nrows = -1;
    reserved1 = 0;
    reserved2 = 0;
    stepx = 1.0;
    stepx_y = 0.0;
    stepy = 1.0;
    stepy_x = 0.0;
    llx = 0.0;
    lly = 0.0;
    sigmaxy = 0.0;
  };
  U16 get_record_id() const
  {
    return 7113;
  }
  const CHAR* get_user_id() const
  {
    return user_id;
  }
  I64 get_payload_size() const
  {
    return (6 * sizeof(I32) + 7 * sizeof(F64));
  };
  U8* get_payload() const
  {
    U8* payload = 0;
    I64 size = get_payload_size();
    ByteStreamOutArray* stream = 0;
    if (IS_LITTLE_ENDIAN())
      stream = new ByteStreamOutArrayLE(size);
    else
      stream = new ByteStreamOutArrayBE(size);
    if (stream)
    {
      if (save(stream))
      {
        payload = stream->takeData();
      }
      delete stream;
    }
    return payload;
  };
  BOOL set_payload(const U8* payload, I64 size)
  {
    ByteStreamInArray* stream = 0;
    if (IS_LITTLE_ENDIAN())
      stream = new ByteStreamInArrayLE(payload, size);
    else
      stream = new ByteStreamInArrayBE(payload, size);
    if (stream)
    {
      if (load(stream))
      {
        delete stream;
        return TRUE;
      }
      delete stream;
    }
    return FALSE;
  }
  BOOL save(ByteStreamOut* stream) const
  {
    if (!stream->put32bitsLE((U8*)&nbands))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.nbands\n");
      return FALSE;
    }
    if (!stream->put32bitsLE((U8*)&nbits))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.nbits\n");
      return FALSE;
    }
    if (!stream->put32bitsLE((U8*)&ncols))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.ncols\n");
      return FALSE;
    }
    if (!stream->put32bitsLE((U8*)&nrows))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.nrows\n");
      return FALSE;
    }
    if (!stream->put32bitsLE((U8*)&reserved1))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.reserved1\n");
      return FALSE;
    }
    if (!stream->put32bitsLE((U8*)&reserved2))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.reserved2\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&stepx))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.stepx\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&stepx_y))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.stepx_y\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&stepy))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.stepy\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&stepy_x))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.stepy_x\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&llx))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.llx\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&lly))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.lly\n");
      return FALSE;
    }
    if (!stream->put64bitsLE((U8*)&sigmaxy))
    {
      eprint("ERROR: writing LASvlrRasterLAZ.sigmaxy\n");
      return FALSE;
    }
    return TRUE;
  };
  BOOL load(ByteStreamIn* stream)
  {
    try { stream->get32bitsLE((U8*)&nbands); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.nbands\n");
      return FALSE;
    }
    try { stream->get32bitsLE((U8*)&nbits); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.nbits\n");
      return FALSE;
    }
    try { stream->get32bitsLE((U8*)&ncols); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.ncols\n");
      return FALSE;
    }
    try { stream->get32bitsLE((U8*)&nrows); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.nrows\n");
      return FALSE;
    }
    try { stream->get32bitsLE((U8*)&reserved1); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.reserved1\n");
      return FALSE;
    }
    try { stream->get32bitsLE((U8*)&reserved2); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.reserved2\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&stepx); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.stepx\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&stepx_y); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.stepx_y\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&stepy); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.stepy\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&stepy_x); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.stepy_x\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&llx); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.llx\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&lly); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.lly\n");
      return FALSE;
    }
    try { stream->get64bitsLE((U8*)&sigmaxy); } catch(...)
    {
      eprint("ERROR: reading LASvlrRasterLAZ.sigmax\n");
      return FALSE;
    }
    return TRUE;
  };
};

#endif
