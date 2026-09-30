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
//
// Pipelines that write one file per chunk (an output path templated with
// '*') are never partitioned: partitioning would change the number and the
// names of the files they write with the parallel strategy, and the files
// would no longer map to the user's AOIs.
inline bool should_auto_partition_ept(PathType format,
                                      bool is_parallelizable,
                                      bool use_rcapi,
                                      int ncpu_outer_loop,
                                      bool writes_per_chunk_files)
{
  return is_parallelizable
      && !use_rcapi
      && !writes_per_chunk_files
      && ncpu_outer_loop > 1
      && format == EPTFILE;
}

}  // namespace api_internal

#endif
