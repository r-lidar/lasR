#ifdef USING_R

#include <Rinternals.h>

#include <thread>

class PointCloud;

void sdl_loop(PointCloud* las);

bool running = false;
std::thread sdl_thread;

SEXP plot_pointcloud(SEXP xptr, bool detach)
{
  PointCloud* las = static_cast<PointCloud*>(R_ExternalPtrAddr(xptr));
  if (!las)
  {
    Rf_error("Invalid pointer");
    return R_NilValue;
  }

  if (detach)
  {
    sdl_thread = std::thread(sdl_loop, las);
    sdl_thread.detach();
    running = true;
  }
  else
  {
    sdl_loop(las);
    running = false;
  }

  return R_NilValue;
}

#endif