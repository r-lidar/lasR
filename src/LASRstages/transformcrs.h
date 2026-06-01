#ifndef LASRTRANSFORMCRS_H
#define LASRTRANSFORMCRS_H

#include "Stage.h"
#include "CRS.h"

class OGRCoordinateTransformation;

// Reproject the point cloud from its current CRS to a user-defined target CRS.
//
// Unlike 'set_crs' (which only relabels the CRS without touching the coordinates),
// 'transform_crs' actually reprojects every point using PROJ/GDAL. It rewrites the
// X/Y/Z coordinates, picks appropriate scale factors and offsets for the target CRS,
// updates the bounding box and tags the data (and every downstream stage/writer) with
// the target CRS.
class LASRtransformcrs : public Stage
{
public:
  LASRtransformcrs();
  LASRtransformcrs(const LASRtransformcrs& other);
  ~LASRtransformcrs();

  bool set_parameters(const nlohmann::json&) override;
  bool process(PointCloud*& las) override;

  // The incoming CRS is the source of the reprojection. We capture it and expose the
  // target CRS to the next stages through get_crs().
  void set_crs(const CRS& crs) override;

  // Reprojection changes coordinates, so the coverage/chunk extents passed to the
  // following stages must be expressed in the target CRS.
  bool set_chunk(Chunk& chunk) override;
  void get_extent(double& xmin, double& ymin, double& xmax, double& ymax) override;

  std::string get_name() const override { return "transform_crs"; }

  // multi-threading: each clone must own its own (non thread-safe) transform object
  LASRtransformcrs* clone() const override { return new LASRtransformcrs(*this); }

private:
  bool build_transform();
  bool transform_bbox(double& xmin, double& ymin, double& xmax, double& ymax);

private:
  CRS source_crs;
  CRS target_crs;
  OGRCoordinateTransformation* transform;
};

#endif
