#include <Profiler.h>
#include <openmp.h>

Profiler::Profiler()
{
  t0 = std::chrono::high_resolution_clock::now();
  start = 0;
  end = 0;
}

float Profiler::elapsed() const
{
  auto time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time - t0);
  return (float)duration.count()/1000.0f;
}

void Profiler::tic()
{
  start = elapsed();
}

void Profiler::toc()
{
  end = elapsed();
}

void Profiler::insert(const std::string& name)
{
  Profile pr(name, start, end, omp_get_thread_num());
  profiles.push_back(pr);
}

void Profiler::write(const std::string& path) const
{
  if (path.empty()) return;
  FILE* fp = fopen(path.c_str(), "w");
  if (fp == NULL) return;
  fprintf(fp, "name, start, end, thread\n");
  for (const auto& profile : profiles) fprintf(fp, "%s, %.2f, %.2f, %d\n", profile.name.c_str(), profile.start, profile.end, profile.thread);
  fclose(fp);
}