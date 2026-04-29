#ifndef LASRWRITELAS_H
#define LASRWRITELAS_H

#include "Stage.h"

class LASio;

class LASRlaswriter: public StageWriter
{
public:
  LASRlaswriter();
  ~LASRlaswriter() noexcept;
  bool set_chunk(Chunk& chunk) override;
  bool set_header(Header*& header) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  bool process(Point*& p) override;
  bool process(PointCloud*& las) override;
  bool is_streamable() const override { return true; };
  void clear(bool last) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "write_las"; }

  // multi-threading
  bool is_parallelizable() const override { return merged == false; };
  LASRlaswriter* clone() const override { return new LASRlaswriter(*this); };

private:
  void clean_copc_ext(std::string& path);

  bool keep_buffer;
  bool experimental_writer;
  short copc_density;
  short copc_depth;
  // Auto-mode-only cap on adaptive depth bumping past the heuristic.
  // -1 = no extra cap (writer's HARD_DEPTH_LIMIT). 0 = "compact mode":
  // never bump past the heuristic depth.
  short copc_max_extra_depth;
  // -1 = use the writer's default (100k). >0 = user-supplied cap.
  // Surfaced as max_points_per_chunk in the R API; wired through to
  // COPCwriter::set_max_points_per_octant.
  int copc_max_points_per_chunk;
  unsigned char version_minor;
  unsigned char point_format;
  std::vector<AttributeAccessor> core_accessors;

  LASio* lasio;
};

#endif
