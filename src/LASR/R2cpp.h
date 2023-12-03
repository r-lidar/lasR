#ifndef R2CPP_H
#define R2CPP_H

#ifdef USING_R
#include <R.h>
#include <Rinternals.h>
#include <vector>
#include <string>

bool contains_element(SEXP list, const char *str);
SEXP get_element(SEXP list, const char *str);
bool get_element_as_bool(SEXP list, const char *str);
int get_element_as_int(SEXP list, const char *str);
double get_element_as_double(SEXP list, const char *str);
std::string get_element_as_string(SEXP list, const char *str);
std::vector<int> get_element_as_vint(SEXP list, const char *str);
std::vector<double> get_element_as_vdouble(SEXP list, const char *str);
std::vector<std::string> get_element_as_vstring(SEXP list, const char *str);

#else
#pragma message("R2cpp skipped: cannot be compiled without R")
#endif

#endif