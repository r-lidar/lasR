#include "Progress.h"

#include "openmp.h"
#include "macros.h"
#include "print.h"

#include <inttypes.h>
#include <string>

#ifdef USING_R
#include <R.h> // for R_FlushConsole
#include <Rinternals.h> // for R_ToplevelExec
#endif

#ifdef USING_R
bool Progress::user_interrupt_event = false;
#endif

// Called only once in the processor() function
Progress::Progress()
{
  percentage = 0.0f;
  current = 0;
  ntotal = 0;
  prefix = "";
  display = false;
  prev = -1.0f;
  sub = 0;
  ncpu = 1;
  use_async_api = false;

#ifdef USING_R
  interrupt_counter = 0;
  user_interrupt_event = false;
  check_interrupt_enabled = true;
#endif
};

// Called only once in the processor() function.
Progress::~Progress()
{
  if (sub) delete sub;
}

// Called only once in the processor() function
void Progress::create_subprocess()
{
  if (sub) delete sub;
  sub = new Progress();
  sub->set_display(this->display);
}

// Operator ++ is called only within stages. The main progress bar is using update() and MUST NOT
// call ++ operator. It is thread safe: only the thread 0 can call the operator ++
// Each call to ++ checks if there is a interrupt event pending so there is no need to explicitly call
// Progress::check_interrupt() this is automatically done in thread 0 when using the progress bar.
Progress& Progress::operator++(int)
{
  if (omp_get_thread_num() != 0) return *this;

  if (sub != nullptr)
  {
    (*sub)++;
  }
  else
  {
    this->current++;
    this->compute_percentage();

    #ifdef USING_R
    check_interrupt();
    #endif
  }

  return *this;
};

// Called by every stage and the main progress bar before to create a sub progress bar. When called
// it applies to the sub-progress this is why is is called before to create a sub progress in the
// processor
void Progress::set_prefix(std::string prefix)
{
  if (omp_get_thread_num() != 0)
    return;

  if (sub)
    sub->set_prefix(prefix);
  else
    this->prefix = prefix;
};

// Can be called either by the main progress or sub progress. Like other functions by default it
// applies to the sub progress. If we are trying to update the main progress we can do it for each thread in
// a critical region. If we are trying to update in a stage we can do it only in thread 0
// Each call to updated() checks if there is a interrupt event pending so there is no need to explicitly call
// Progress::check_interrupt() this is automatically done in thread 0 when using the progress bar.
void Progress::update(uint64_t current, bool main)
{
  if (main)
  {
    #pragma omp critical (progress)
    {
      this->current = current;
      this->compute_percentage();
    }

    #ifdef USING_R
    check_interrupt(true);
    #endif

    return;
  }

  if (omp_get_thread_num() != 0)
    return;

  if (sub != nullptr)
  {
    sub->update(current);
  }
  else
  {
    this->current = current;
    this->compute_percentage();
  }

  #ifdef USING_R
  check_interrupt();
  #endif
};

// Called by every stage and can be applied only by thread 0
void Progress::reset()
{
  if (omp_get_thread_num() != 0)
    return;

  if (sub)
  {
    sub->reset();
  }
  else
  {
    this->percentage = 0.0f;
    this->prev = -1.0f;
    this->current = 0;
    this->ntotal = 0;
    this->ncpu = 1;

    #ifdef USING_R
    this->interrupt_counter = 0;
    #endif
  }
}

// Called only once in the processor function
void Progress::set_display(bool display)
{
  this->display = display;
  if (sub) sub->set_display(display);
}


// Called by every stage and can be applied only by thread 0
void Progress::set_total(uint64_t nmax)
{
  if (omp_get_thread_num() != 0)
    return;

  if (sub)
    sub->set_total(nmax);
  else
    this->ntotal = nmax;
}

void Progress::set_ncpu(int n)
{
  if (sub)
    sub->ncpu = n;
  else
    this->ncpu = n;
}

void Progress::set_async_message_file(const std::string& file)
{
  if (file.size() > 0)
  {
    use_async_api = true;
    async_communication_file = file;
  }
  else
  {
    use_async_api = false;
    async_communication_file = "";
  }
}

// Called by every stage and can be applied only by thread 0. It is also called by the processor
// outside open mp region
void Progress::done(bool main)
{
  if (omp_get_thread_num() != 0) return;

  if (sub)
  {
    sub->current = ntotal;
    sub->percentage = 1.0f;
    sub->prev = 0;
  }

  if (main)
  {
    this->current = ntotal;
    this->percentage = 1.0f;
    this->prev = 0;

    if (sub)
    {
      delete sub;
      sub = 0;
    }
  }

  this->show();

  if (main)
  {
    this->reset();
    if (display) print( "\n");
  }

  if (sub) sub->reset();
}

// # nocov start
void Progress::show(bool flush)
{
  if (omp_get_thread_num() != 0) return;

  if (display && must_show())
  {
    if (use_async_api)
    {
      FILE* file = fopen(async_communication_file.c_str(), "w");
      if (file == NULL) return;
      fprintf(file, "%lf", this->percentage);
      fclose(file);
      return;
    }

    if (ntotal > 0)
    {
      this->prev = this->percentage;
      int completed = (int)(10 * percentage);
      int remaining = (int)(10 - completed);
      print("%s: [%.*s%*s] %.0lf%% (%d threads)", this->prefix.c_str(), completed, PROGRESSSYM, remaining, "", percentage * 100, ncpu);
    }
    else
    {
      print("%s: %s ", this->prefix.c_str(), "no progress");
    }

    if (sub)
    {
      print(" | ");
      sub->show(false);

      #ifdef USING_R
      if (user_interrupt_event)
        print(" (Interrupt signal detected: stopping asap)");
      #endif
    }

    if (flush)
    {
      print("%*s\r", 20, "");

      #ifdef USING_R
      R_FlushConsole();
      #else
      fflush(stdout);
      #endif
    }
  }
}

bool Progress::must_show()
{
  bool b = (this->percentage - this->prev) >= 0.01f;
  if (sub) b = b || sub->must_show();
  return b;
}
// # nocov end

void Progress::compute_percentage()
{
  if (sub) sub->compute_percentage();
  this->percentage = (float)((double)this->current / (double)this->ntotal);
  if (this->percentage > 1.0f) this->percentage = 1.0f;
}

#ifdef USING_R
bool Progress::check_interrupt(bool force)
{
  // Do no check interrupt if an event has already been caught
  if (!check_interrupt_enabled || user_interrupt_event)
    return false;

  // Only thread 0 can check to prevent data R
  if (omp_get_thread_num() != 0)
    return false; // # nocov

  // Check every 1024 iterations
  interrupt_counter++;
  if (interrupt_counter % 1024 == 0 || force)
  {
    if (checkUserInterrupt())
    {
      user_interrupt_event = true;
      show();
    }
  }

  return user_interrupt_event;
}

void Progress::checkInterruptFn(void* /*dummy*/)
{
  R_CheckUserInterrupt();
}

// Check for interrupts and throw the sentinel exception if one is pending
bool Progress::checkUserInterrupt()
{
  if (R_ToplevelExec(checkInterruptFn, NULL) == FALSE)
    return true;
  else
    return false;
}

void Progress::disable_check_interrupt()
{
  if (sub)
    sub->check_interrupt_enabled = false;
  else
    check_interrupt_enabled = false;
}
#endif