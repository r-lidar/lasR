#ifndef EPT_PARTITION_GATE_H
#define EPT_PARTITION_GATE_H

#include "FileCollection.h"

namespace api_internal {

// Default EPT auto-partition target when LASR_EPT_PARTITIONS is unset.
// Fixed at 32 (not 4×worker count): benchmarks on USGS 3DEP AOIs showed
// 4×16→64 partitions creates too many small chunks (~100) and loses to
// 8 workers at ~36 chunks; capping the target at 32 keeps chunk count stable
// while concurrent_files(16) still wins on large AOIs.
constexpr int default_ept_auto_partitions = 32;

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
