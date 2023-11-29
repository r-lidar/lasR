#ifndef CHMPREP_H
#define CHMPREP_H

namespace st_onge
  {
  float *chm_prep(const float *geom, int snlin, int sncol, int lap_size, float thr_cav, float thr_spk, int med_size, int dil_radius, float nodata);
  float* prepare_filter_elements(int);
  void prepare_files();
  unsigned char * find_holes(int, int, int, int, int, int, int, float, float, int, float*, float*);
  float * interpolate(int, int, int, int, int, int, float *, unsigned char *);
  float * median_filter(int, int, int, int, int, int, int, float *, unsigned char *);
  float get_median(int, float *);
}

#endif
