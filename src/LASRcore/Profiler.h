#ifndef PROFILER_H
#define PROFILER_H

#include <vector>
#include <chrono>
#include <string>

struct Profile
{
  Profile() : start(0), end(0), thread(0) {};
  Profile(std::string name, float start, float end, int thread) : name(name), start(start), end(end), thread(thread) {};
  std::string name;
  float start;
  float end;
  int thread;
};

struct Profiler
{
  Profiler();
  void tic();
  void toc();
  float elapsed() const;
  void insert(const std::string& name);
  void write(const std::string& path) const;

  std::chrono::time_point<std::chrono::high_resolution_clock> t0;
  float start;
  float end;
  std::vector<Profile> profiles;
};

#endif