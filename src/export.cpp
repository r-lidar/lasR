#ifdef USING_R

#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

SEXP process(SEXP);
SEXP lasfilterusage();
SEXP lastransformusage();
SEXP get_pipeline_info(SEXP);
SEXP get_address(SEXP);
SEXP is_indexed(SEXP);
SEXP get_address(SEXP);
int available_threads();
bool has_omp_support();


extern "C" SEXP C_process(SEXP args) { return process(args); }
extern "C" SEXP C_lasfilterusage() { return lasfilterusage(); }
extern "C" SEXP C_lastransformusage() { return lastransformusage(); }
extern "C" SEXP C_get_pipeline_info(SEXP pipeline) { return get_pipeline_info(pipeline); }
extern "C" SEXP C_available_threads() { return Rf_ScalarInteger(available_threads()); }
extern "C" SEXP C_has_omp_support() { return Rf_ScalarLogical(has_omp_support()); }
extern "C" SEXP C_is_indexed(SEXP file) { return is_indexed(file); }
extern "C" SEXP C_address(SEXP x) { return get_address(x); }

static const R_CallMethodDef CallEntries[] = {
  {"C_lasfilterusage", (DL_FUNC) &C_lasfilterusage, 0},
  {"C_lastransformusage", (DL_FUNC) &C_lastransformusage, 0},
  {"C_get_pipeline_info", (DL_FUNC) &C_get_pipeline_info, 1},
  {"C_process", (DL_FUNC) &C_process, 1},
  {"C_available_threads", (DL_FUNC) &C_available_threads, 0},
  {"C_has_omp_support", (DL_FUNC) &C_has_omp_support, 0},
  {"C_is_indexed", (DL_FUNC) &C_is_indexed, 1},
  {"C_address", (DL_FUNC) &C_address, 1},
  {NULL, NULL, 0}
};

extern "C" void attribute_visible R_init_lasR(DllInfo *dll)
{
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}

SEXP get_address(SEXP obj)
{
  char address[20];
  snprintf(address, sizeof(address), "%p", (void*)obj);
  return mkString(address);
}

#endif

