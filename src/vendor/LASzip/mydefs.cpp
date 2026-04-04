/*
===============================================================================

  FILE:  mydefs.cpp

  CONTENTS:

    see corresponding header file

  PROGRAMMERS:

    martin.isenburg@rapidlasso.com  -  http://rapidlasso.com

  COPYRIGHT:

    (c) 2011-2019, martin isenburg, rapidlasso - fast tools to catch reality

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the LICENSE.txt file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  CHANGE HISTORY:

    see corresponding header file

===============================================================================
*/
#include "mydefs.hpp"

#if defined(_MSC_VER)
#include <windows.h>
wchar_t* UTF8toUTF16(const char* utf8)
{
  wchar_t* utf16 = 0;
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
  if (len > 0)
  {
    utf16 = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, len);
  }
  return utf16;
}
#endif

#include <string.h>

bool is_remote_path(const char* path)
{
  if (!path) return false;

  // Group 1: The HTTP family
  if (path[0] == 'h')
  {
    return
      strncmp(path, "http://", 7) == 0 ||
      strncmp(path, "https://", 8) == 0;
  }

  // Group 2: The GDAL VSI family
  // Fast check: Must start with '/' and contain "vsi"
  if (path[0] == '/' && strncmp(path, "/vsi", 4) == 0)
  {
    const char* sub = path + 4;

    return
      strncmp(sub, "curl/", 5) == 0  ||
      strncmp(sub, "s3/", 3) == 0    ||
      strncmp(sub, "gs/", 3) == 0    ||
      strncmp(sub, "az/", 3) == 0    ||
      strncmp(sub, "adls/", 5) == 0  ||
      strncmp(sub, "oss/", 4) == 0   ||
      strncmp(sub, "swift/", 6) == 0;
  }

  return false;
}

#ifndef USING_R
#include <cstdarg>
#include <cstdio>
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
