#ifndef EPT_PARTITION_GATE_H
#define EPT_PARTITION_GATE_H

#include "FileCollection.h"

namespace api_internal {

// Predicate matching the gate in execute.cpp's auto-partition hook.
// Returns true iff the engine should call FileCollection::partition_ept
// before reading the chunk count.
inline bool should_auto_partition_ept(PathType format,
                                      bool is_parallelizable,
                                      bool use_rcapi,
                                      int ncpu_outer_loop)
{
  return is_parallelizable
      && !use_rcapi
      && ncpu_outer_loop > 1
      && format == EPTFILE;
}

}  // namespace api_internal

#endif
