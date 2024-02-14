#ifndef PROGRESS_H
#define PROGRESS_H

#include <string>

#define PROGRESSSYM "=================================================="

class Progress
{
public:
  Progress();
  Progress(uint64_t ntotal);
  ~Progress();
  Progress &operator++(int);
  void create_subprocess();
  void set_prefix(std::string prefix);
  void update(uint64_t current, bool main = false);
  void reset();
  void set_display(bool display);
  void set_total(uint64_t nmax);
  float get_progress();
  void done(bool main = false);
  void pause();
  void show(bool flush = true);

private:
  bool must_show();
  void compute_percentage();

private:
  bool display;

  // main
  float percentage;
  float prev;
  uint64_t current;
  uint64_t ntotal;
  std::string prefix;

  // sub process
  Progress* sub;
};

#endif