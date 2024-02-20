#ifndef LASRWRITELAS_H
#define LASRWRITELAS_H

#include "lasralgorithm.h"

class LASwriter;
class LASheader;
//class LASindex;
//class LASquadtree;

class LASRlaswriter: public LASRalgorithmWriter
{
public:
  LASRlaswriter(double xmin, double ymin, double xmax, double ymax, bool keep_buffer);
  ~LASRlaswriter();
  void set_header(LASheader*& header) override;
  void set_input_file_name(std::string file) override;
  void set_output_file(std::string file) override;
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; };
  void clear(bool last) override;
  std::string get_name() const override { return "write_las"; }

  // multi-threading
  bool is_parallelizable() const override { return merged == false; };
  LASRlaswriter* clone() const override { return new LASRlaswriter(*this); };
  void merge(const LASRalgorithm* other) override;

private:
  bool keep_buffer;
  LASwriter* laswriter;
  LASheader* lasheader;
};

#endif
