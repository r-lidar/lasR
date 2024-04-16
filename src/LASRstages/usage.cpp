#include <Rinternals.h>

#include "macros.h"
#include "lasfilter.hpp"
#include "lastransform.hpp"
#include "lasreader.hpp"

SEXP lasfilterusage()
{
  LASfilter filter;
  filter.usage();
  return R_NilValue;
}

SEXP lastransformusage()
{
  LAStransform transform;
  transform.usage();
  return R_NilValue;
}

SEXP is_indexed(SEXP sexpfile)
{
  const char* file = CHAR(STRING_ELT(sexpfile, 0));
  LASreadOpener lasreadopener;
  lasreadopener.set_file_name(file);
  LASreader* lasreader = lasreadopener.open();

  if (!lasreader)
  {
    // # nocov start
    SEXP res = PROTECT(Rf_allocVector(VECSXP, 2)) ;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(names, 0, Rf_mkChar("message")) ;
    SET_STRING_ELT(names, 1, Rf_mkChar("call")) ;
    Rf_setAttrib(res, R_NamesSymbol, names);

    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar("C++Error"));
    SET_STRING_ELT(classes, 1, Rf_mkChar("error"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("condition"));
    Rf_setAttrib(res, R_ClassSymbol, classes);

    SEXP R_err = PROTECT(Rf_allocVector(STRSXP, 1));
    SEXP R_msg = PROTECT(Rf_mkChar("LASlib internal error"));
    SET_STRING_ELT(R_err, 0, R_msg);

    SET_VECTOR_ELT(res, 0, R_err);
    SET_VECTOR_ELT(res, 1, R_NilValue);

    UNPROTECT(5);
    return res;
    // # nocov end
  }

  bool indexed = lasreader->get_copcindex() || lasreader->get_index();

  lasreader->close();
  delete lasreader;

  return Rf_ScalarLogical(indexed);
}