#ifndef ERROR_H
#define ERROR_H

#include <string>
extern int last_error_code;
extern std::string last_error;

#if defined(USING_R) && USING_R != 0
#define R_NO_REMAP 1
#include <Rinternals.h>
SEXP make_R_error(const char* message);
#endif

#endif
