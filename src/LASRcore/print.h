#ifndef PRINT_H
#define PRINT_H

#include <stdio.h>

// Thread safe prints that are using Rprintf and REprintf if compiled with R
void print(const char *format, ...);
void eprint(const char *format, ...);
void warning(const char *format, ...);
void log(FILE *fp, bool verbose, const char *format, ...);

#endif