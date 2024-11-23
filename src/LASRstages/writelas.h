#ifndef LASRWRITELAS_H
#define LASRWRITELAS_H

#include "Stage.h"

class LASwriter;
class LASheader;
class LASpoint;
//class LASindex;
//class LASquadtree;

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

private:
  AttributeHandler intensity;
  AttributeHandler returnnumber;
  AttributeHandler numberofreturns;
  AttributeHandler userdata;
  AttributeHandler psid;
  AttributeHandler classification;
  AttributeHandler scanangle;
  AttributeHandler gpstime;
  AttributeHandler scannerchannel;
  AttributeHandler red;
  AttributeHandler green;
  AttributeHandler blue;
  AttributeHandler nir;

  bool keep_buffer;
  LASpoint* point;
  LASwriter* laswriter;
  LASheader* lasheader;
  std::vector<AttributeHandler> core_accessors;
};

#endif
