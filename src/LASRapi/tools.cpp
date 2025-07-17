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