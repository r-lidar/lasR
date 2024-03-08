#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

SEXP process(SEXP, SEXP);
SEXP lasfilterusage();
SEXP lastransformusage();
SEXP get_pipeline_info(SEXP);
SEXP is_indexed(SEXP sexpfile);
int available_threads();
bool has_omp_support();


extern "C" SEXP C_process(SEXP pipeline, SEXP args) { return process(pipeline, args); }
extern "C" SEXP C_lasfilterusage() { return lasfilterusage(); }
extern "C" SEXP C_lastransformusage() { return lastransformusage(); }
extern "C" SEXP C_get_pipeline_info(SEXP pipeline) { return get_pipeline_info(pipeline); }
extern "C" SEXP C_available_threads() { return Rf_ScalarInteger(available_threads()); }
extern "C" SEXP C_has_omp_support() { return Rf_ScalarLogical(has_omp_support()); }
extern "C" SEXP C_is_indexed(SEXP file) { return is_indexed(file); }

static const R_CallMethodDef CallEntries[] = {
  {"C_lasfilterusage", (DL_FUNC) &C_lasfilterusage, 0},
  {"C_lastransformusage", (DL_FUNC) &C_lastransformusage, 0},
  {"C_get_pipeline_info", (DL_FUNC) &C_get_pipeline_info, 1},
  {"C_process", (DL_FUNC) &C_process, 2},
  {"C_available_threads", (DL_FUNC) &C_available_threads, 0},
  {"C_has_omp_support", (DL_FUNC) &C_has_omp_support, 0},
  {"C_is_indexed", (DL_FUNC) &C_is_indexed, 1},
  {NULL, NULL, 0}
};

extern "C" void attribute_visible R_init_lasR(DllInfo *dll)
{
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}

