#ifndef PROGRESS_H
#define PROGRESS_H

#include <string>
#include <cstdint>

#define PROGRESSSYM "=================================================="

class Progress
{
public:
  Progress();
  ~Progress();
  Progress &operator++(int);
  void create_subprocess();
  void set_prefix(std::string prefix);
  void update(uint64_t current, bool main = false);
  void reset();
  void set_display(bool display);
  void set_total(uint64_t nmax);
  void set_ncpu(int ncpu);
  void set_async_message_file(std::string& file);
  void done(bool main = false);
  void show(bool flush = true);

#ifdef USING_R
  bool check_interrupt(bool force = false);
  static bool interrupted() { return user_interrupt_event; };
  void disable_check_interrupt();
#else
  static bool interrupted() { return false; };
#endif

private:
  bool must_show();
  void compute_percentage();

#ifdef USING_R
  // Handle user interrupt event
  static void checkInterruptFn(void*);
  static bool checkUserInterrupt();
  bool check_interrupt_enabled;
  unsigned int interrupt_counter;
  static bool user_interrupt_event;
#endif

  // main
  float percentage;
  float prev;
  uint64_t current;
  uint64_t ntotal;
  std::string prefix;

  // sub process
  Progress* sub;

  // api communication
  bool use_async_api;
  std::string async_communication_file;


  bool display;
  int ncpu;
};

#endif
