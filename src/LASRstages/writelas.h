#ifndef LASRWRITELAS_H
#define LASRWRITELAS_H

#include "Stage.h"

class LASlibInterface;

class LASRlaswriter: public StageWriter
{
public:
  LASRlaswriter();
  ~LASRlaswriter();
  bool set_chunk(Chunk& chunk) override;
  void set_header(Header*& header) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  bool process(Point*& p) override;
  bool process(LAS*& las) override;
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
  std::vector<AttributeAccessor> core_accessors;

  LASlibInterface* laslibinterface;
};

#endif
