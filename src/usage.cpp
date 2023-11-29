#include <Rinternals.h>

#include "macros.h"
#include "lasfilter.hpp"
#include "lastransform.hpp"

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