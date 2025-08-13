#include "openmp.h"

int available_threads()
{
  int max = omp_get_max_threads();
  int lim = omp_get_thread_limit();
  int n = (max < lim) ? max : lim;
  return n;
}

bool has_omp_support()
{
#ifdef _OPENMP
  return true;
#else
  return false;
#endif
}