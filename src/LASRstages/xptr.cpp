#include "xptr.h"

#include <Rinternals.h>
#include <R.h>

// Finalizer function for the external pointer
extern "C" void las_finalizer(SEXP extPtr)
{
  PointCloud* las = static_cast<PointCloud*>(R_ExternalPtrAddr(extPtr));
  if (las)
  {
    delete las; // Free the memory
    R_ClearExternalPtr(extPtr); // Clear the external pointer
    Rprintf("Point cloud destructed.\n");
  }
}

bool LASRxptr::process(PointCloud*& las)
{
  this->las = las;
  return true;
};

SEXP LASRxptr::to_R()
{
  SEXP extPtr = R_MakeExternalPtr(las, R_NilValue, R_NilValue);
  R_RegisterCFinalizer(extPtr, las_finalizer);
  return extPtr;
};

void LASRxptr::merge(const Stage* other)
{
  const LASRxptr* o = dynamic_cast<const LASRxptr*>(other);
  las = o->las;
}
