#ifndef LASRREADATAFRAME_H
#define LASRREADATAFRAME_H

#ifdef USING_R

#include "Stage.h"

class LASheader;

class LASRdataframereader: public Stage
{
public:
  LASRdataframereader() = default;
  LASRdataframereader(const LASRdataframereader& other);
  bool set_chunk(Chunk& chunk) override;
  bool process(Header*& header) override;
  bool process(Point*& point) override;
  bool process(PointCloud*& las) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "reader_dataframe"; }
  bool use_rcapi() const override { return true; };
  void clear(bool) override;

  // multi-threading
  LASRdataframereader* clone() const override { return new LASRdataframereader(*this); };

private:
  double guess_accuracy(const SEXP x) const;


private:
  int current_point;
  int npoints;
  double scale[3];
  double offset[3];
  std::vector<AttributeAccessor> accessors;
  std::string wkt;
  SEXP dataframe;

  Header* header; // own in streaming mode
  bool streaming;

  Point point;

  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, SAR, PSID, R, G, B, NIR, CHAN, BUFF};
};

#else
#pragma message("LASRdataframereader skipped: cannot be compiled without R")
#endif

#endif
