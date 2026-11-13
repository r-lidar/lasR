#ifndef LASRTRANSFORMCRS_H
#define LASRTRANSFORMCRS_H

#include "Stage.h"
#include "CRS.h"

#include <cmath>
#include <memory>

// Reprojects X and Y from the CRS of the point cloud to a target CRS. Z is not modified.
// Like transform_with() with a matrix, this stage moves the points, so it also updates the
// extent and the chunk handed to the next stages.
class LASRtransformcrs : public Stage
{
public:
  LASRtransformcrs() = default;
  LASRtransformcrs(const LASRtransformcrs& other);
  bool set_parameters(const nlohmann::json&) override;
  void set_crs(const CRS& crs) override;
  void get_extent(double& xmin, double& ymin, double& xmax, double& ymax) override;
  bool set_chunk(Chunk& chunk) override;
  bool process(PointCloud*& las) override;
  std::string get_name() const override { return "transform_crs"; }

  // multi-threading
  LASRtransformcrs* clone() const override { return new LASRtransformcrs(*this); }

private:
  bool reproject_bbox(double& xmin, double& ymin, double& xmax, double& ymax) const;

private:
  struct Deleter { void operator()(OGRCoordinateTransformation* t) const { OGRCoordinateTransformation::DestroyCT(t); } };

  CRS source_crs;
  CRS target_crs;
  std::unique_ptr<OGRCoordinateTransformation, Deleter> transform; // null if it cannot be built
  double xoffset = NAN; // offsets of the reprojected coordinates: the centre of the coverage
  double yoffset = NAN;
};

#endif
