#include "Progress.h"

#include "openmp.h"
#include "macros.h"
#include "Rcompatibility.h"

#include <inttypes.h>
#include <string>

#ifdef USING_R
#include <R.h> // for R_FlushConsole
#endif

// Called only once in the processor function
Progress::Progress()
{
  percentage = 0.0f;
  current = 0;
  ntotal = 0;
  prefix = "";
  display = false;
  prev = -1.0f;
  sub = 0;
};

// Called only once in the processor function
Progress::~Progress()
{
  if (sub) delete sub;
}

// Operator ++ is called only within stages. The main progress bar is using update() and MUST NOT
// call ++. It is thread safe: only the thread 0 can call the operator ++
Progress& Progress::operator++(int)
{
  if (omp_get_thread_num() != 0)
    return *this;

  if (sub != nullptr)
  {
    (*sub)++;
  }
  else
  {
    this->current++;
    this->compute_percentage();
  }

  return *this;
};

// Called only once in the processor function
void Progress::create_subprocess()
{
  if (sub) delete sub;
  sub = new Progress();
  sub->set_display(this->display);
}

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
void Progress::update(uint64_t current, bool main)
{
  if (main)
  {
    #pragma omp critical (progress)
    {
      this->current = current;
      this->compute_percentage();
    }

    return;
  }

  if (omp_get_thread_num() != 0)
    return;

  if (sub != nullptr)
    sub->update(current);
  else
  {
    this->current = current;
    this->compute_percentage();
  }
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

// Called by every stage and can be applied only by thread 0. It is also called by the processor
// outside open mp region
void Progress::done(bool main)
{
  if (omp_get_thread_num() != 0)
    return;

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

void Progress::pause()
{
  if (sub) sub->prev = -1.0f; // force print
  this->prev = -1.0f; // force print
  this->show(false);
}

// # nocov start
void Progress::show(bool flush)
{
  if (omp_get_thread_num() != 0)
    return;

  if (display && must_show())
  {
    if (ntotal > 0)
    {
      this->prev = this->percentage;
      int completed = (int)(15 * percentage);
      int remaining = (int)(15 - completed);
      print("%s: [%.*s%*s] %.0lf%%", this->prefix.c_str(), completed, PROGRESSSYM, remaining, "", percentage * 100);
    }
    else
    {
      print("%s: %s ", this->prefix.c_str(), "no progress");
    }

    if (sub)
    {
      print(" | ");
      sub->show(false);
      print(" (%d threads)",  omp_get_num_threads());
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