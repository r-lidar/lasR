#include <stdarg.h>
#include <stdio.h>
#include "openmp.h"

#ifdef USING_R
#define R_NO_REMAP 1
#include <R_ext/Print.h>
#include <R_ext/Error.h>

#include <string>
#include <vector>

#define MESSAGELVL 0
#define WARNINGLVL 1
#define ERRORLVL 2

// Global vector to store messages
std::vector<std::pair<int, std::string>> message_queue;

// Function to print and clear the queue (only called by thread 0)
void print_queue()
{
  for (const auto &message : message_queue)
  {
    switch (message.first)
    {
    case MESSAGELVL:
      Rprintf("%s", message.second.c_str());
      break;
    case WARNINGLVL:
      REprintf("WARNING: %s", message.second.c_str());
      break;
    case ERRORLVL:
      REprintf("ERROR: %s", message.second.c_str());
      break;
    default:
      REprintf("%s", message.second.c_str());
    break;
    }
  }

  message_queue.clear();
}

// Function to add a message to the queue
void thread_safe_print(int level, const char *buffer)
{
  #pragma omp critical (queue_mutex)
  {
    message_queue.push_back({level, buffer});
  }

  if (omp_get_thread_num() == 0)
  {
    #pragma omp critical (Rprint)
    {
      print_queue();
    }
  }
}

// Print function that multiple threads can call
void print(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  thread_safe_print(MESSAGELVL, buffer);
}

void warning(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  thread_safe_print(WARNINGLVL, buffer);
}

void eprint(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  thread_safe_print(ERRORLVL, buffer);
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

void log(FILE *fp, bool verbose, const char *format, ...)
{
  va_list args;
  va_start(args, format);

  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  // Print to console if verbose
  if (verbose) {
    print("%s", buffer);
  }

  // Append to file safely
  #pragma omp critical
  {
    if (fp != NULL) {
      fprintf(fp, "%s", buffer);
      fflush(fp);
    }
  }

  va_end(args);
}