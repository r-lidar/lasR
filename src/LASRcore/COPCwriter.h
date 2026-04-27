#ifndef COPC_WRITER_H
#define COPC_WRITER_H

#include <string>
#include <vector>

#include "COPChierarchy.h"
#include "COPCspill.h"

class LASwriterLAS;
class LASheader;
class LASpoint;

// Facade that writes a Cloud Optimized Point Cloud (COPC) LAZ file. Composes
// (not inherits) LASlib's LASwriterLAS for byte-level LAZ output. Memory
// strategy: adaptive per-cell RAM buffers spill to per-cell temp files only
// under pressure (COPCspill). At close(), the octree is finalised via
// COPChierarchy and one LAZ chunk is emitted per final octant.
//
// API is shaped to drop into LASio::create() in place of LASwriteOpener::open()
// for .copc.laz outputs.
class COPCwriter
{
public:
  COPCwriter();
  ~COPCwriter();

  COPCwriter(const COPCwriter&) = delete;
  COPCwriter& operator=(const COPCwriter&) = delete;

  // Configuration (call before open()).
  void set_copc_depth(I32 depth) { copc_depth = depth; }       // -1 = auto
  void set_copc_density(I32 density) { copc_density = density; } // grid side, typically 128/256/512
  void set_min_points_per_chunk(I32 n) { min_points_per_chunk = n; }

  // Open the output file. Applies the COPC header transformations (upgrade to
  // LAS 1.4, promote PDRF to 6/7/8, add COPC info VLR placeholder and EPT
  // hierarchy eVLR placeholder). Returns false on failure (e.g. cannot open
  // the output, unsupported PDRF).
  bool open(const char* file_name, const LASheader* source_header, I32 io_buffer_size);

  // Write one point. Format-converts to target PDRF, computes max-depth leaf
  // key, hands to COPCspill. Returns false if writer is poisoned or I/O fails.
  bool write_point(const LASpoint* p);

  // Finalise: collapse, sort per-octant, LAZ-chunk emit, write hierarchy eVLR,
  // patch header and COPC info VLR. Returns total bytes written (or -1 on error).
  I64 close();

  // Total bytes written so far (for compatibility with LASio's bookkeeping).
  I64 tell();

  const std::string& last_error() const { return error_msg; }
  bool is_poisoned() const { return poisoned; }

private:
  // Copy & transform the caller's header to the target COPC layout (PDRF 6+,
  // LAS 1.4, COPC VLR placeholders). Sets this->copc_header and this->point.
  bool prepare_copc_header(const LASheader* source_header);

  // Drive the finalization sequence (called from close()).
  bool finalize_and_write();

  void fail(const std::string& msg);

private:
  // Transformed header handed to LASwriterLAS. Owned by this writer.
  LASheader* copc_header = nullptr;
  // Point object used for per-call format conversion to the target PDRF.
  LASpoint* point = nullptr;
  // LAZ writer we wrap.
  LASwriterLAS* writer_las = nullptr;

  // Reusable per-call scratch (sized once at open() to target PDRF length).
  // Avoids a heap allocation on every write_point().
  std::vector<U8> write_scratch;

  COPChierarchy* hierarchy = nullptr;
  COPCspill* spill = nullptr;

  // Configuration
  I32 copc_depth = -1;
  I32 copc_density = 256;
  I32 max_points_per_octant = 100000; // used to estimate auto max_depth
  I32 min_points_per_chunk = 100;

  // Stats accumulated during write_point
  F64 gpstime_minimum = 0.0;
  F64 gpstime_maximum = 0.0;
  // Actual data bbox seen during intake. Compared against the declared
  // octree bbox at close() to warn when the input header is much looser
  // than the data (skewed octree structure — see "loose bbox" pitfall).
  F64 data_min_x = 0.0, data_max_x = 0.0;
  F64 data_min_y = 0.0, data_max_y = 0.0;
  F64 data_min_z = 0.0, data_max_z = 0.0;
  bool have_any_point = false;

  std::string output_path;
  bool poisoned = false;
  std::string error_msg;
  bool closed = false;
};

#endif
