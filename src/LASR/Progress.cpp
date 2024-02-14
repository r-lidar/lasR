#include "Progress.h"

#include "openmp.h"
#include "macros.h"
#include "Rcompatibility.h"

#include <inttypes.h>
#include <string>

#ifdef USING_R
#include <R.h> // for R_FlushConsole
#endif

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

Progress::Progress(uint64_t ntotal)
{
  percentage = 0;
  current = 0;
  this->ntotal = ntotal;
  prefix = "";
  display = false;
  prev = -1.0f;
  sub = 0;
};

Progress::~Progress()
{
  if (sub) delete sub;
}

Progress& Progress::operator++(int)
{
  if (sub)
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

void Progress::create_subprocess()
{
  if (sub) delete sub;
  sub = new Progress();
  sub->set_display(this->display);
}

void Progress::set_prefix(std::string prefix)
{
  if (sub)
    sub->set_prefix(prefix);
  else
    this->prefix = prefix;
};

void Progress::update(uint64_t current, bool main)
{
  if (main == false && sub)
  {
    sub->update(current);
  }
  else
  {
    this->current = current;
    this->compute_percentage();
  }
};

void Progress::reset()
{
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

void Progress::set_display(bool display)
{
  this->display = display;
  if (sub) sub->set_display(display);
}

void Progress::set_total(uint64_t nmax)
{
  if (sub)
    sub->set_total(nmax);
  else
    this->ntotal = nmax;
}

float Progress::get_progress()
{
  if (sub)
    return sub->get_progress();
  else
    return this->percentage*100.0f;
}

void Progress::done(bool main)
{
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