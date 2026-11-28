#include "writecog.h"

#include <algorithm>
#include <cctype>

LASRcogwriter::LASRcogwriter()
{
  // There is a single output file for the whole collection. This makes merge() and sort()
  // keep a single file name instead of one per chunk.
  merged = true;
}

bool LASRcogwriter::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  StageRaster* p = dynamic_cast<StageRaster*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'write_cog'. Expecting a raster stage";
    return false;
  }

  return true;
}

bool LASRcogwriter::set_parameters(const nlohmann::json& stage)
{
  // Output file. It is also given to set_output_file() later, but checking it here reports
  // invalid file names when the pipeline is parsed, before any file is created
  std::string file = stage.value("output", "");

  if (file.empty())
  {
    last_error = "a file name is required";
    return false;
  }

  if (file.find('*') != std::string::npos)
  {
    last_error = "wildcard '*' is not supported in the file name";
    return false;
  }

  size_t pos = file.find_last_of('.');
  std::string ext = (pos == std::string::npos) ? "" : file.substr(pos + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  if (ext != "tif" && ext != "tiff")
  {
    last_error = "the file must be .tif or .tiff";
    return false;
  }

  // Resampling method for the overviews
  static const std::vector<std::string> choices = {"nearest", "bilinear", "cubic", "cubicspline", "lanczos", "average", "mode"};
  std::string method = stage.value("resampling", "bilinear");
  std::transform(method.begin(), method.end(), method.begin(), [](unsigned char c) { return std::tolower(c); });
  if (std::find(choices.begin(), choices.end(), method) == choices.end())
  {
    last_error = "invalid resampling '" + method + "'. Available options are 'nearest', 'bilinear', 'cubic', 'cubicspline', 'lanczos', 'average', 'mode'";
    return false;
  }

  std::transform(method.begin(), method.end(), method.begin(), [](unsigned char c) { return std::toupper(c); });
  resampling = method;

  return true;
}

// Called after the connected raster stage got its own output file. It is the earliest point
// where we know whether the source raster is a single file on disk
bool LASRcogwriter::set_output_file(const std::string& file)
{
  StageRaster* p = get_source();
  if (!p) return false; // # nocov

  if (!p->is_merged())
  {
    if (p->get_template_filename().empty())
      last_error = "the source raster is not written on disk (ofile = \"\"). A COG can only be made from a raster file";
    else
      last_error = "the source raster is written in one file per chunk (ofile with a wildcard *). This is not supported yet";
    return false;
  }

  if (Raster::same_file(p->get_template_filename(), file))
  {
    last_error = "the COG file must be different from the source raster file: " + file;
    return false;
  }

  ofile = file;
  written = {file};
  return true;
}

bool LASRcogwriter::finalize()
{
  StageRaster* p = get_source();
  if (!p) return false; // # nocov

  return p->get_raster().to_cog(ofile, resampling);
}

StageRaster* LASRcogwriter::get_source() const
{
  if (connections.empty())
  {
    last_error = "Unitialized pointer to Stage"; // # nocov
    return nullptr; // # nocov
  }

  StageRaster* p = dynamic_cast<StageRaster*>(connections.begin()->second);
  if (!p)
  {
    last_error = "Invalid pointer dynamic cast. Expecting a pointer to StageRaster"; // # nocov
    return nullptr; // # nocov
  }

  return p;
}
