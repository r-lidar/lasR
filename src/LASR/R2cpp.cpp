#include "R2cpp.h"

#ifdef USING_R

bool contains_element(SEXP list, const char *str)
{
  SEXP names = Rf_getAttrib(list, R_NamesSymbol);
  for (int i = 0 ; i < Rf_length(list) ; i++) { if (strcmp(CHAR(STRING_ELT(names, i)), str) == 0) return true; }
  return false;
}

SEXP get_element(SEXP list, const char *str)
{
  SEXP elmt = R_NilValue;
  SEXP names = Rf_getAttrib(list, R_NamesSymbol);
  for (int i = 0 ; i < Rf_length(list) ; i++) { if(strcmp(CHAR(STRING_ELT(names, i)), str) == 0)  { elmt = VECTOR_ELT(list, i); break; }}
  if (Rf_isNull(elmt)) error("element '%s' not found", str);
  return elmt;
}

bool get_element_as_bool(SEXP list, const char *str)
{
  return LOGICAL(get_element(list, str))[0] != 0;
}

int get_element_as_int(SEXP list, const char *str)
{
  return INTEGER(get_element(list, str))[0];
}

double get_element_as_double(SEXP list, const char *str)
{
  return REAL(get_element(list, str))[0];
}

std::string get_element_as_string(SEXP list, const char *str)
{
  return std::string(CHAR(STRING_ELT(get_element(list, str), 0)));
}

std::vector<int> get_element_as_vint(SEXP list, const char *str)
{
  SEXP res = get_element(list, str);
  std::vector<int> ans(Rf_length(res));
  for (int i = 0 ; i < Rf_length(res) ; ++i) ans[i] = INTEGER(res)[i];
  return ans;
}

std::vector<double> get_element_as_vdouble(SEXP list, const char *str)
{
  SEXP res = get_element(list, str);
  std::vector<double> ans(Rf_length(res));
  for (int i = 0 ; i < Rf_length(res) ; ++i) ans[i] = REAL(res)[i];
  return ans;
}

std::vector<std::string> get_element_as_vstring(SEXP list, const char *str)
{
  SEXP res = get_element(list, str);
  std::vector<std::string> ans(Rf_length(res));
  for (int i = 0 ; i < Rf_length(res) ; ++i) ans[i] = CHAR(STRING_ELT(res, i));
  return ans;
}

#endif