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
  if (Rf_isNull(elmt)) throw std::string("element '") + str +  "' not found"; // # nocov
  return elmt;
}

SEXP string_address_to_sexp(const std::string& addr)
{
  uintptr_t ptr = strtoull(addr.c_str(), NULL, 16);
  SEXP s = (SEXP)ptr;
  return s;
}

bool get_element_as_bool(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  switch (TYPEOF(elem))
  {
    case LGLSXP: return LOGICAL(elem)[0] != 0; break;
    case REALSXP: return REAL(elem)[0] != 0; break; // # nocov
    case INTSXP: return INTEGER(elem)[0] != 0; break; // # nocov
    default: throw std::string(str) + " must be a bool or something interpretable as a logical value"; break; // # nocov
  }
}

int get_element_as_int(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  switch (TYPEOF(elem))
  {
  case LGLSXP: return (int)LOGICAL(elem)[0]; break; // # nocov
  case REALSXP: return (int)REAL(elem)[0]; break;
  case INTSXP: return INTEGER(elem)[0]; break;
  default: throw std::string(str) + " must be an integer or something interpretable as an integer value"; break; // # nocov
  }
}

double get_element_as_double(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  switch (TYPEOF(elem))
  {
  case LGLSXP: return (double)LOGICAL(elem)[0]; break; // # nocov
  case REALSXP: return REAL(elem)[0]; break;
  case INTSXP: return (double)INTEGER(elem)[0]; break;
  default: throw std::string(str) + " must be a numeric or something interpretable as a numeric value"; break; // # nocov
  }
}

std::string get_element_as_string(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  switch (TYPEOF(elem))
  {
  case STRSXP: return std::string(CHAR(STRING_ELT(get_element(list, str), 0))); break;
  default: throw std::string(str) + " must be a string"; // # nocov
  }
}

std::vector<int> get_element_as_vint(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  int n = Rf_length(elem);
  std::vector<int> ans(n);

  switch (TYPEOF(elem))
  {
  case LGLSXP: for (int i=0;i<n;++i) ans[i] = (int)LOGICAL(elem)[i]; break; // # nocov
  case REALSXP: for (int i=0;i<n;++i) ans[i] = (int)REAL(elem)[i]; break;
  case INTSXP: for (int i=0;i<n;++i) ans[i] = INTEGER(elem)[i]; break;
  default: throw std::string(str) + " must be interger or something interpretable as a integer values"; break; // # nocov
  }

  return ans;
}

std::vector<double> get_element_as_vdouble(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  int n = Rf_length(elem);
  std::vector<double> ans(n);

  switch (TYPEOF(elem))
  {
  case LGLSXP: for (int i=0;i<n;++i) ans[i] = (double)LOGICAL(elem)[i]; break; // # nocov
  case REALSXP: for (int i=0;i<n;++i) ans[i] = REAL(elem)[i]; break;
  case INTSXP: for (int i=0;i<n;++i) ans[i] = (double)INTEGER(elem)[i]; break;
  default: throw std::string(str) + " must be a numeric or something interpretable as a numeric value"; break; // # nocov
  }

  return ans;
}

std::vector<std::string> get_element_as_vstring(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  int n = Rf_length(elem);
  std::vector<std::string> ans(n);

  switch (TYPEOF(elem))
  {
  case STRSXP: for (int i=0;i<n;++i) ans[i] = CHAR(STRING_ELT(elem, i)); break;
  default: throw std::string(str) + " must be a character vector"; break; // # nocov
  }

  return ans;
}

std::vector<bool> get_element_as_vbool(SEXP list, const char *str)
{
  SEXP elem = get_element(list, str);
  int n = Rf_length(elem);
  std::vector<bool> ans(n);

  switch (TYPEOF(elem))
  {
  case LGLSXP: for (int i=0;i<n;++i) ans[i] = LOGICAL(elem)[i]; break;
  case REALSXP: for (int i=0;i<n;++i) ans[i] = REAL(elem)[i] > 0; break; // # nocov
  case INTSXP: for (int i=0;i<n;++i) ans[i] = INTEGER(elem)[i] > 0; break; // # nocov
  default: throw std::string(str) + " must be a logical or something interpretable as a logical value"; break; // # nocov
  }

  return ans;
}

#endif