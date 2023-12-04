#ifndef RCOMPATIBLE_H
#define RCOMPATIBLE_H

#ifdef USING_R
#define R_NO_REMAP 1
#include <R_ext/Print.h>
#include <R_ext/Error.h>
#define print Rprintf
#define eprint REprintf
#define warning Rf_warning
#else
#include <cstdio>
#define print(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#define eprint(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#define warning(fmt, ...) fprintf(stderr, "WARNING: " fmt "\n", ##__VA_ARGS__)
#endif

#endif