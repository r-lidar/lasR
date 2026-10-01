#include <stdarg.h>
#include <stdio.h>
#include "openmp.h"

#ifdef USING_R
#define R_NO_REMAP 1
#include <R_ext/Print.h>
#include <R_ext/Error.h>

#include <string>
#include <thread>
#include <vector>

#define MESSAGELVL 0
#define WARNINGLVL 1
#define ERRORLVL 2

// Global vector to store messages
std::vector<std::pair<int, std::string>> message_queue;

// Thread id captured at library load = the main R thread. Used to decide who
// may touch R's C API (Rprintf/REprintf). omp_get_thread_num() is unsafe for
// this check: it returns 0 for std::async / background threads that are not in
// any OpenMP team, which would let them call into R off the main thread.
static const std::thread::id g_main_thread_id = std::this_thread::get_id();

// Function to print and clear the queue (only called by the main thread, under
// the queue_mutex critical)
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
      REprintf("\033[38;5;208mWARNING: %s\033[0m", message.second.c_str());
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
  // A SINGLE lock guards both the push and the drain, so the global queue is
  // never mutated concurrently. Only the main R thread drains it (i.e. touches
  // R's C API). Previously push used critical(queue_mutex) and drain used
  // critical(Rprint) -- two different locks on the same std::vector -- so a
  // worker's push_back could run concurrently with the main thread's
  // clear()/iterate, corrupting the heap under load (segfault / double free
  // observed on large concurrent EPT reads).
  #pragma omp critical (queue_mutex)
  {
    message_queue.push_back({level, buffer});
    if (std::this_thread::get_id() == g_main_thread_id)
      print_queue();
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

  fprintf(stderr, "\033[38;5;208mWARNING: %s\033[0m", buffer); // Print formatted string
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