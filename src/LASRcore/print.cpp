#include <stdarg.h>
#include <stdio.h>
#include "openmp.h"

#ifdef USING_R
#define R_NO_REMAP 1
#include <R_ext/Print.h>
#include <R_ext/Error.h>

void print(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  #pragma omp critical (Rprint)
  {
    Rprintf("%s", buffer);
  }
}

void eprint(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  #pragma omp critical (Rprint)
  {
    REprintf("ERROR: %s", buffer);
  }
}

void warning(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  #pragma omp critical (Rprint)
  {
    REprintf("WARNING: %s", buffer); // Print formatted string
    //Rf_warning()
  }
}
#else

void print(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  printf("%s", buffer); // Print formatted string
}

void eprint(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  fprintf(stderr, "ERROR: %s", buffer); // Print formatted string
}

void warning(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  fprintf(stderr, "WARNING: %s", buffer); // Print formatted string
}

#endif