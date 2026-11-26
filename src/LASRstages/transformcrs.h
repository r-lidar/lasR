#ifndef LASRTRANSFORMCRS_H
#define LASRTRANSFORMCRS_H

#include "Stage.h"
#include "CRS.h"

#include <cmath>
#include <memory>
#include <vector>

// Reprojects X and Y from the CRS of the point cloud to a target CRS. Z is not modified.
// Like transform_with() with a matrix, this stage moves the points, so it also updates the
// extent and the chunk handed to the next stages. Files in different CRS are each reprojected
// from their own CRS.
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
  struct Deleter { void operator()(OGRCoordinateTransformation* t) const { OGRCoordinateTransformation::DestroyCT(t); } };
  using Transformation = std::unique_ptr<OGRCoordinateTransformation, Deleter>;
  struct Source { CRS crs; Transformation transform; }; // transform is null if it cannot be built

  Transformation create_transformation(const CRS& source) const;
  bool reproject_bbox(OGRCoordinateTransformation* t, double& xmin, double& ymin, double& xmax, double& ymax) const;
  bool reproject_files(double& xmin, double& ymin, double& xmax, double& ymax) const;

private:
  // One transformation per CRS of the points: the CRS received when the pipeline is parsed and,
  // if the files are in different CRS, the CRS of the other files.
  std::vector<Source> sources;
  CRS source_crs; // CRS of the current chunk
  CRS target_crs;
  OGRCoordinateTransformation* transform = nullptr; // the one of the current chunk, in sources
  double xoffset = NAN; // offsets of the reprojected coordinates: the centre of the coverage
  double yoffset = NAN;
};

#endif
