#ifndef LASRREADATAFRAME_H
#define LASRREADATAFRAME_H

#include "lasralgorithm.h"

class LASheader;

class LASRdataframereader: public LASRalgorithm
{
public:
  LASRdataframereader(double xmin, double ymin, double xmax, double ymax, SEXP dataframe, const std::vector<double>& accuracy);
  bool set_chunk(const Chunk& chunk) override;
  bool process(LASheader*& header) override;
  bool process(LASpoint*& point) override;
  bool process(LAS*& las) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  bool is_mergable() const override {return true; };
  void set_filter(std::string filter) override { this->filter = filter; };
  std::string get_name() const override { return "reader_dataframe"; }

private:
  int get_point_data_record_length(int point_data_format);
  int get_header_size(int minor_version);
  int guess_point_data_format();
  double guess_accuracy(SEXP x);


private:
  bool has_gps;
  bool has_rgb;
  bool has_nir;
  bool has_intensity;
  bool is_extended;
  int current_point;
  int num_extrabytes;
  int npoints;
  double scale[3];
  double offset[3];
  std::vector<int> col_names;
  std::string filter;
  SEXP dataframe;

  LASheader lasheader;
  LASpoint laspoint;

  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, SAR, PSID, R, G, B, NIR, CHAN, BUFF};
};

#endif
