#ifndef RCOMPATIBLE_H
#define RCOMPATIBLE_H

#ifdef USING_R
#define R_NO_REMAP 1
#include <R_ext/Print.h>
#include <R_ext/Error.h>

// Thread safe prints that are using Rprintf and REprintf
void print(const char *format, ...);
void eprint(const char *format, ...);
void warning(const char *format, ...);

#endif
#endif