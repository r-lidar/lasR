#include "error.h"

// Definition of global variables
int last_error_code = 0;
std::string last_error;

#ifdef USING_R
SEXP make_R_error(const char* message)
{
  SEXP res = PROTECT(Rf_allocVector(VECSXP, 2));

  SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
  SET_STRING_ELT(names, 0, Rf_mkChar("message"));
  SET_STRING_ELT(names, 1, Rf_mkChar("call"));
  Rf_setAttrib(res, R_NamesSymbol, names);

  SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
  SET_STRING_ELT(classes, 0, Rf_mkChar("C++Error"));
  SET_STRING_ELT(classes, 1, Rf_mkChar("error"));
  SET_STRING_ELT(classes, 2, Rf_mkChar("condition"));
  Rf_setAttrib(res, R_ClassSymbol, classes);

  SEXP R_err = PROTECT(Rf_allocVector(STRSXP, 1));
  SEXP R_msg = PROTECT(Rf_mkChar(message));
  SET_STRING_ELT(R_err, 0, R_msg);

  SET_VECTOR_ELT(res, 0, R_err);
  SET_VECTOR_ELT(res, 1, R_NilValue);

  UNPROTECT(5);
  return res;
}
#endif