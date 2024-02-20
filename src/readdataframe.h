#ifndef LASRREADATAFRAME_H
#define LASRREADATAFRAME_H

#include "lasralgorithm.h"

class LASheader;

class LASRdataframereader: public LASRalgorithm
{
public:
  LASRdataframereader(double xmin, double ymin, double xmax, double ymax, const SEXP dataframe, const std::vector<double>& accuracy, const std::string& wkt);
  bool set_chunk(const Chunk& chunk) override;
  bool process(LASheader*& header) override;
  bool process(LASpoint*& point) override;
  bool process(LAS*& las) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "reader_dataframe"; }

  // multi-threading
  bool is_parallelizable() const override { return false; };
  LASRdataframereader* clone() const override { return new LASRdataframereader(*this); };

private:
  int get_point_data_record_length(int point_data_format) const;
  int get_header_size(int minor_version) const;
  int guess_point_data_format() const;
  double guess_accuracy(const SEXP x) const;


private:
  bool has_gps;
  bool has_rgb;
  bool has_nir;
  bool is_extended;
  int current_point;
  int num_extrabytes;
  int npoints;
  double scale[3];
  double offset[3];
  std::vector<int> col_names;
  std::string wkt;
  SEXP dataframe;

  LASheader lasheader;
  LASpoint laspoint;

  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, SAR, PSID, R, G, B, NIR, CHAN, BUFF};
};

#endif
