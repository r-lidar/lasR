#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_num_threads() 1
#define omp_get_thread_num() 0
#define omp_get_max_threads() 1
#define omp_get_thread_limit() 1
#define omp_set_max_active_levels(x)
#endif

int available_threads();
bool has_omp_support();