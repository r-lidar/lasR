#include "summary.h"
#include "openmp.h"

#include <iterator>

LASRsummary::LASRsummary()
{
  npoints = 0;
  nsingle = 0;
  nwithheld = 0;
  nsynthetic = 0;
  //npoints_per_sdf = {0,0};
}

bool LASRsummary::set_parameters(const nlohmann::json& stage)
{
  zwbin = stage.value("zwbin", 2.0);
  iwbin = stage.value("iwbin", 50.0);

  std::vector<std::string> metrics;
  if (stage.contains("metrics")) metrics = get_vector<std::string>(stage.at("metrics"));

  if (!metrics_engine.parse(metrics, false)) return false;

  return true;
}

bool LASRsummary::process(LASpoint*& p)
{
  if (p->get_withheld_flag() != 0) return true;
  if (lasfilter.filter(p)) return true;
  if (p->inside_buffer(xmin, ymin, xmax, ymax, circular)) return true; // avoid counting buffer points

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

  if (metrics_engine.active())  cloud.push_back(PointLAS(p));

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

  if (metrics_engine.active())
  {
    for (int i = 0 ; i < metrics_engine.size() ; i++)
    {
      const std::string& name = metrics_engine.get_name(i);
      float val = metrics_engine.get_metric(i, cloud);
      metrics[name].push_back(val);
    }

    cloud.clear();
  }

  return true;
}

void LASRsummary::merge(const Stage* other)
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

  if (metrics_engine.active())
  {
    for (const auto& pair : o->metrics)
    {
      const std::string& key = pair.first;
      const std::vector<float>& vec = pair.second;
      metrics[key].insert(metrics[key].end(), vec.begin(), vec.end());
    }
  }
}

void LASRsummary::sort(const std::vector<int>& order)
{
  for (auto& pair : metrics)
  {
    std::vector<float> ordered(pair.second.size());
    for (size_t i = 0 ; i < pair.second.size() ; i++) ordered[order[i]] = pair.second[i];
    pair.second.swap(ordered);
  }
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
  bool use_metrics = metrics_engine.active();

  int n = 10;
  if (use_metrics) n++;

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
  SET_STRING_ELT(names, 8, Rf_mkChar("crs"));
  SET_STRING_ELT(names, 9, Rf_mkChar("epsg"));
  if (use_metrics) SET_STRING_ELT(names, 10, Rf_mkChar("metrics"));
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

  // CRS

  std::string wkt = crs.get_wkt();
  SEXP R_wkt = PROTECT(Rf_allocVector(STRSXP, 1)); nsexpprotected++;
  SET_STRING_ELT(R_wkt, 0, Rf_mkCharLen(wkt.c_str(), wkt.length()));

  SEXP R_epsg = PROTECT(Rf_ScalarInteger(crs.get_epsg())); nsexpprotected++;

  // Assign the elements to the list
  SET_VECTOR_ELT(list, 0, R_npoints);
  SET_VECTOR_ELT(list, 1, R_nsingle);
  SET_VECTOR_ELT(list, 2, R_nwithheld);
  SET_VECTOR_ELT(list, 3, R_nsynthetic);
  SET_VECTOR_ELT(list, 4, R_npoints_per_return);
  SET_VECTOR_ELT(list, 5, R_npoints_per_class);
  SET_VECTOR_ELT(list, 6, R_zhistogram);
  SET_VECTOR_ELT(list, 7, R_ihistogram);
  SET_VECTOR_ELT(list, 8, R_wkt);
  SET_VECTOR_ELT(list, 9, R_epsg);

  // Metrics

  if (use_metrics)
  {
    int ncols = metrics.size();
    int nrows = metrics.begin()->second.size();

    SEXP df = PROTECT(Rf_allocVector(VECSXP, ncols)); nsexpprotected++;
    SEXP colnames = PROTECT(Rf_allocVector(STRSXP, ncols)); nsexpprotected++;

    int colIdx = 0;
    for (const auto& kv : metrics)
    {
      const std::string& name = kv.first;
      const std::vector<float>& data = kv.second;

      SEXP vec = PROTECT(Rf_allocVector(REALSXP, nrows)); nsexpprotected++;
      for (int i = 0; i < nrows; ++i) {  REAL(vec)[i] = data[i]; }

      SET_STRING_ELT(colnames, colIdx, Rf_mkChar(name.c_str()));
      SET_VECTOR_ELT(df, colIdx, vec);

      colIdx++;
    }

    Rf_setAttrib(df, R_ClassSymbol, Rf_ScalarString(Rf_mkChar("data.frame")));
    Rf_setAttrib(df, R_NamesSymbol, colnames);

    // Name the rows (see https://stackoverflow.com/questions/37069149/creating-a-r-data-frame-in-c-c)
    SEXP rnms = PROTECT(Rf_allocVector(INTSXP, 2)); nsexpprotected++;
    INTEGER(rnms)[0] = NA_INTEGER;
    INTEGER(rnms)[1] = -nrows;
    Rf_setAttrib(df, R_RowNamesSymbol, rnms);

    SET_VECTOR_ELT(list, 10, df);
  }

  return list;
}

#endif



