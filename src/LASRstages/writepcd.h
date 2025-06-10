#ifndef LASRWRITEPCD_H
#define LASRWRITEPCD_H

#include "Stage.h"

class PCDio;

class LASRpcdwriter: public StageWriter
{
public:
  LASRpcdwriter();
  ~LASRpcdwriter();
  bool set_chunk(Chunk& chunk) override;
  void set_header(Header*& header) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  bool process(Point*& p) override;
  bool process(PointCloud*& las) override;
  bool is_streamable() const override { return false; };
  void clear(bool last) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "write_pcd"; }

  // multi-threading
  bool is_parallelizable() const override { return merged == false; };
  LASRpcdwriter* clone() const override { return new LASRpcdwriter(*this); };

private:
  bool binary;
  PCDio* pcdio;
};

#endif
