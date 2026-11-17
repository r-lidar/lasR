#include "openmp.h"

#include <thread>

int available_threads()
{
#ifndef _OPENMP
  int hw = (int)std::thread::hardware_concurrency();
  return (hw > 0) ? hw : 1;
#endif
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