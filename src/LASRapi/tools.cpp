#include "lasfilter.hpp"
#include "lastransform.hpp"
#include "lasreader.hpp"
#include <string>
#include <stdexcept>

namespace api
{

void lasfilterusage()
{
  LASfilter filter;
  filter.usage();
  return;
}

void lastransformusage()
{
  LAStransform transform;
  transform.usage();
  return;
}

bool is_indexed(std::string file)
{
  const char* cfile = file.c_str();
  LASreadOpener lasreadopener;
  lasreadopener.set_file_name(cfile);
  LASreader* lasreader = lasreadopener.open();

  if (!lasreader)
    throw std::runtime_error("LASlib internal error");

  bool indexed = lasreader->get_copcindex() || lasreader->get_index();

  lasreader->close();
  delete lasreader;

  return indexed;
}

}

#include <iostream>

#ifdef _WIN32
#include <windows.h>

namespace api
{
unsigned long long getAvailableRAM()
{
  MEMORYSTATUSEX memoryStatus;
  memoryStatus.dwLength = sizeof(memoryStatus);
  GlobalMemoryStatusEx(&memoryStatus);
  return memoryStatus.ullAvailPhys / 1e6;
}

unsigned long long getTotalRAM()
{
  MEMORYSTATUSEX memoryStatus;
  memoryStatus.dwLength = sizeof(memoryStatus);
  GlobalMemoryStatusEx(&memoryStatus);
  return memoryStatus.ullTotalPhys / 1e6;
}
}

#elif __linux__
#include <fstream>
#include <string>
#include <sstream>

namespace api
{
unsigned long long getAvailableRAM()
{
  std::ifstream meminfo("/proc/meminfo");
  std::string line;
  unsigned long long availableRAM = 0;
  while (std::getline(meminfo, line))
  {
    if (line.find("MemAvailable:") == 0)
    {
      std::istringstream iss(line);
      std::string key;
      unsigned long long value;
      std::string unit;
      iss >> key >> value >> unit;
      availableRAM = value / 1e3;  // kB
      break;
    }
  }
  return availableRAM;
}

unsigned long long getTotalRAM()
{
  std::ifstream meminfo("/proc/meminfo");
  std::string line;
  unsigned long long totalRAM = 0;
  while (std::getline(meminfo, line)) {
    if (line.find("MemTotal:") == 0) {
      std::istringstream iss(line);
      std::string key;
      unsigned long long value;
      std::string unit;
      iss >> key >> value >> unit;
      totalRAM = value / 1e3;  // convert from kB to bytes
      break;
    }
  }
  return totalRAM;
}
}

#elif __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_host.h>

namespace api
{
unsigned long long getTotalRAM()
{
  int mib[] = {CTL_HW, HW_MEMSIZE};
  uint64_t memory;
  size_t length = sizeof(memory);
  sysctl(mib, 2, &memory, &length, NULL, 0);
  return memory / 1e6;
}

unsigned long long getAvailableRAM()
{
  vm_size_t page_size;
  vm_statistics64_data_t vm_stat;
  mach_port_t mach_port = mach_host_self();
  mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);
  
  if (host_page_size(mach_port, &page_size) != KERN_SUCCESS) {
    // Fallback to total RAM if we can't get available
    return getTotalRAM();
  }
  
  if (host_statistics64(mach_port, HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size) != KERN_SUCCESS) {
    // Fallback to total RAM if we can't get statistics
    return getTotalRAM();
  }
  
  // Calculate available memory: free + inactive + purgeable pages
  unsigned long long available = (vm_stat.free_count + vm_stat.inactive_count + vm_stat.purgeable_count) * page_size;
  
  // Return in MB
  return available / 1e6;
}
}

#else
#error "Unsupported platform"
#endif