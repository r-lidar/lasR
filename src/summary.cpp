#include "summary.h"
#include "openmp.h"

#include <iterator>

LASRsummary::LASRsummary(double xmin, double ymin, double xmax, double ymax, double zwbin, double iwbin)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  //this->ofile = ofile;

  this->zwbin = zwbin;
  this->iwbin = iwbin;

  npoints = 0;
  nsingle = 0;
  nwithheld = 0;
  nsynthetic = 0;
  //npoints_per_sdf = {0,0};
}

bool LASRsummary::process(LASpoint*& p)
{
  if (lasfilter.filter(p)) return true;
  if (p->inside_buffer(xmin, ymin, xmax, ymax, circular)) return true; // avoid counting buffer points

  #pragma omp critical
  {
  npoints++;
  npoints_per_return[p->get_return_number()]++;
  npoints_per_class[p->get_classification()]++;

  if (p->get_number_of_returns() == 1) nsingle++;
  if (p->get_withheld_flag() == 1) nwithheld++;

  //(p->get_scan_direction_flag() == 0) ? npoints_per_sdf.first++ : npoints_per_sdf.second++;

  int z = (int) std::round(p->get_z() / zwbin ) * zwbin;
  int i = (int) std::round(p->get_intensity() / iwbin) * iwbin;
  (zhistogram.find(z) == zhistogram.end()) ?  zhistogram[z] = 1 : zhistogram[z]++;
  (ihistogram.find(i) == ihistogram.end()) ?  ihistogram[i] = 1 : ihistogram[i]++;
  }

  return true;
}

bool LASRsummary::process(LAS*& las)
{
  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    process(p);
  }

  return true;
}

void LASRsummary::merge(const LASRalgorithm* other)
{
  const LASRsummary* o = dynamic_cast<const LASRsummary*>(other);

  npoints += o->npoints;
  nsingle += o->nsingle;
  nwithheld += o->nwithheld;
  nsynthetic += o->nsynthetic;

  merge_maps(npoints_per_return, o->npoints_per_return);
  merge_maps(npoints_per_class, o->npoints_per_class);
  merge_maps(npoints_per_sdf, o->npoints_per_return);
  merge_maps(zhistogram, o->zhistogram);
  merge_maps(ihistogram, o->ihistogram);
}

void LASRsummary::merge_maps(std::map<int, uint64_t>& map1, const std::map<int, uint64_t>& map2)
{
  for (const auto& pair : map2)
  {
    int key = pair.first;
    int value = pair.second;
    map1[key] += value;
  }
}

#ifdef USING_R

SEXP LASRsummary::to_R()
{
  int n = 8;

  // Create a list
  SEXP list = PROTECT(Rf_allocVector(VECSXP, n)); nsexpprotected++;

  // Create a character vector for the names
  SEXP names = PROTECT(Rf_allocVector(STRSXP, n)); nsexpprotected++;
  SET_STRING_ELT(names, 0, Rf_mkChar("npoints"));
  SET_STRING_ELT(names, 1, Rf_mkChar("nsingle"));
  SET_STRING_ELT(names, 2, Rf_mkChar("nwithheld"));
  SET_STRING_ELT(names, 3, Rf_mkChar("nsynthetic"));
  SET_STRING_ELT(names, 4, Rf_mkChar("npoints_per_return"));
  SET_STRING_ELT(names, 5, Rf_mkChar("npoints_per_class"));
  SET_STRING_ELT(names, 6, Rf_mkChar("z_histogram"));
  SET_STRING_ELT(names, 7, Rf_mkChar("i_histogram"));
  Rf_setAttrib(list, R_NamesSymbol, names);

  // BASIC STATS

  SEXP R_npoints = PROTECT(Rf_ScalarReal((double)npoints)); nsexpprotected++;
  SEXP R_nsingle = PROTECT(Rf_ScalarReal((double)nsingle)); nsexpprotected++;
  SEXP R_nwithheld = PROTECT(Rf_ScalarReal((double)nwithheld)); nsexpprotected++;
  SEXP R_nsynthetic = PROTECT(Rf_ScalarReal((double)nsynthetic)); nsexpprotected++;

  // NPOINTS PER <X>

  SEXP R_npoints_per_return = PROTECT(Rf_allocVector(REALSXP, npoints_per_return.size())); nsexpprotected++;
  SEXP R_npoints_per_return_names = PROTECT(Rf_allocVector(STRSXP, npoints_per_return.size())); nsexpprotected++;
  for (size_t i = 0 ; i < npoints_per_return.size() ; i++)
  {
    auto it = npoints_per_return.begin();
    std::advance(it, i);
    std::string num = std::to_string(it->first);
    REAL(R_npoints_per_return)[i] = (double)it->second;
    SET_STRING_ELT(R_npoints_per_return_names, i, Rf_mkChar(num.c_str()));
  }
  Rf_setAttrib(R_npoints_per_return, R_NamesSymbol, R_npoints_per_return_names);

  SEXP R_npoints_per_class = PROTECT(Rf_allocVector(REALSXP, npoints_per_class.size())); nsexpprotected++;
  SEXP R_npoints_per_class_names = PROTECT(Rf_allocVector(STRSXP, npoints_per_class.size())); nsexpprotected++;
  for (size_t i = 0 ; i < npoints_per_class.size() ; i++)
  {
    auto it = npoints_per_class.begin();
    std::advance(it, i);
    std::string num = std::to_string(it->first);
    REAL(R_npoints_per_class)[i] = (double)it->second;
    SET_STRING_ELT(R_npoints_per_class_names, i, Rf_mkChar(num.c_str()));
  }
  Rf_setAttrib(R_npoints_per_class, R_NamesSymbol, R_npoints_per_class_names);

  // HISTOGRAMS

  int hmin, hmax, nbins;

  if (zhistogram.size() > 1)
  {
    hmin = zhistogram.begin()->first;
    hmax = (--zhistogram.end())->first;
    nbins = (hmax-hmin)/2+1;
  }
  else
  {
    hmin = zhistogram.begin()->first;
    hmax = hmin;
    nbins = 1;
  }
  SEXP R_zhistogram = PROTECT(Rf_allocVector(REALSXP, nbins)); nsexpprotected++;
  SEXP R_zhistogram_names = PROTECT(Rf_allocVector(STRSXP, nbins)); nsexpprotected++;
  for (auto i = 0 ; i < nbins ; i++)
  {
    int key = hmin + i*zwbin;
    std::string num = std::to_string(key);
    SET_STRING_ELT(R_zhistogram_names, i, Rf_mkChar(num.c_str()));
    if (zhistogram.find(key) != zhistogram.end())
      REAL(R_zhistogram)[i] = (double)zhistogram[key];
    else
      REAL(R_zhistogram)[i] = 0;
  }
  Rf_setAttrib(R_zhistogram, R_NamesSymbol, R_zhistogram_names);

  if (ihistogram.size() > 1)
  {
    hmin = ihistogram.begin()->first;
    hmax = (--ihistogram.end())->first;
    nbins = (hmax-hmin)/iwbin+1;
  }
  else
  {
    hmin = ihistogram.begin()->first;
    hmax = hmin;
    nbins = 1;
  }
  SEXP R_ihistogram = PROTECT(Rf_allocVector(REALSXP, nbins)); nsexpprotected++;
  SEXP R_ihistogram_names = PROTECT(Rf_allocVector(STRSXP, nbins)); nsexpprotected++;
  for (auto i = 0 ; i < nbins ; i++)
  {
    int key = hmin + i*iwbin;
    std::string num = std::to_string(key);
    SET_STRING_ELT(R_ihistogram_names, i, Rf_mkChar(num.c_str()));
    if (ihistogram.find(key) != ihistogram.end())
      REAL(R_ihistogram)[i] = (double)ihistogram[key];
    else
      REAL(R_ihistogram)[i] = 0;
  }
  Rf_setAttrib(R_ihistogram, R_NamesSymbol, R_ihistogram_names);

  // Assign the elements to the list
  SET_VECTOR_ELT(list, 0, R_npoints);
  SET_VECTOR_ELT(list, 1, R_nsingle);
  SET_VECTOR_ELT(list, 2, R_nwithheld);
  SET_VECTOR_ELT(list, 3, R_nsynthetic);
  SET_VECTOR_ELT(list, 4, R_npoints_per_return);
  SET_VECTOR_ELT(list, 5, R_npoints_per_class);
  SET_VECTOR_ELT(list, 6, R_zhistogram);
  SET_VECTOR_ELT(list, 7, R_ihistogram);

  return list;
}

#endif



