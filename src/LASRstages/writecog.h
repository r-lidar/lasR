#ifndef WRITECOG_H
#define WRITECOG_H

#include "Stage.h"

// Converts the raster produced by another stage into a Cloud Optimized GeoTIFF. A COG cannot be
// written chunk by chunk. This stage does nothing while processing the chunks; it copies the
// complete raster into a COG in finalize(), once every chunk has been written.
class LASRcogwriter: public StageWriter
{
public:
  LASRcogwriter();
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  bool set_parameters(const nlohmann::json&) override;
  bool set_output_file(const std::string& file) override;
  bool finalize() override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "write_cog"; }

  // multi-threading
  LASRcogwriter* clone() const override { return new LASRcogwriter(*this); };

private:
  StageRaster* get_source() const;

private:
  std::string resampling;
};

#endif
