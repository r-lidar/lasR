#include <iostream>

#ifdef _WIN32
#include <windows.h>

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
  return memoryStatus.ullTotalPhys/1e6;
}

#elif __linux__
#include <fstream>
#include <string>
#include <sstream>

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

#elif __APPLE__
#include <sys/sysctl.h>

unsigned long long getAvailableRAM()
{
  int mib[] = {CTL_HW, HW_MEMSIZE};
  uint64_t memory;
  size_t length = sizeof(memory);
  sysctl(mib, 2, &memory, &length, NULL, 0);
  return memory / 1e6;
}

unsigned long long getTotalRAM()
{
  int mib[] = {CTL_HW, HW_MEMSIZE};
  uint64_t memory;
  size_t length = sizeof(memory);
  sysctl(mib, 2, &memory, &length, NULL, 0);
  return memory;
}

#else
#error "Unsupported platform"
#endif
