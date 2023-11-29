#include <R.h>
#include <Rinternals.h>
#include <stdlib.h> // for NULL
#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

SEXP process(SEXP, SEXP, SEXP, SEXP, SEXP, SEXP);
SEXP lasfilterusage();
SEXP lastransformusage();
int available_threads();

extern "C" SEXP C_process(SEXP pipeline, SEXP ifiles, SEXP buffer, SEXP progrss, SEXP ncpu, SEXP verbose) { return process(pipeline, ifiles, buffer, progrss, ncpu, verbose); }
extern "C" SEXP C_lasfilterusage() { return lasfilterusage(); }
extern "C" SEXP C_lastransformusage() { return lastransformusage(); }
extern "C" SEXP C_available_threads() { return Rf_ScalarInteger(available_threads()); }

static const R_CallMethodDef CallEntries[] = {
  {"C_lasfilterusage", (DL_FUNC) &C_lasfilterusage, 0},
  {"C_lastransformusage", (DL_FUNC) &C_lastransformusage, 0},
  {"C_process", (DL_FUNC) &C_process, 6},
  {"C_available_threads", (DL_FUNC) &C_available_threads, 0},
  {NULL, NULL, 0}
};

extern "C" void attribute_visible R_init_lasR(DllInfo *dll)
{
  R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
  R_useDynamicSymbols(dll, FALSE);
  R_forceSymbols(dll, TRUE);
}

