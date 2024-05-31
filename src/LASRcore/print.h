#ifndef PRINT_H
#define PRINT_H

// Thread safe prints that are using Rprintf and REprintf if compiled with R
void print(const char *format, ...);
void eprint(const char *format, ...);
void warning(const char *format, ...);

#endif