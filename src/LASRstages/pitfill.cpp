#include "pitfill.h"
#include "geophoton/chm_prep.h"

bool LASRpitfill::set_parameters(const nlohmann::json& stage)
{
  lap_size = stage.value("lap_size", 3);
  thr_lap = stage.value("thr_lap", 0.1f);
  thr_spk = stage.value("thr_spk", -0.1f);
  med_size = stage.value("med_size", 3);
  dil_radius = stage.value("dil_radius", 0);

  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  raster = Raster(p->get_raster());

  return true;
}

bool LASRpitfill::process()
{

  if (connections.empty())
  {
    last_error = "Unitialized pointer to Stage"; // # nocov
    return false; // # nocov
  }

  // 'connections' contains a single stage that is supposed to be a raster stage
  // This is the only supported stage as of feb 2024
  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  if (!p)
  {
    last_error = "Invalid pointer dynamic cast. Expecting a pointer to StageRaster"; // # nocov
    return false; // # nocov
  }

  const Raster& rin = p->get_raster();
  const auto geom = rin.get_data();
  int sncol = rin.get_ncols();
  int snlin = rin.get_nrows();

  if (geom.size() != (unsigned int)sncol*(unsigned int)snlin)
  {
    last_error = "Internal error: wrong data size. Please report."; // # nocov
    return false; // # nocov
  }

  float* ans = geophoton::chm_prep(&geom[0], snlin, sncol, lap_size, thr_lap, thr_spk, med_size, dil_radius, rin.get_nodata());

  if (ans == NULL)
  {
    last_error = "st_onge::chm_prep failed to allocate memory"; // # nocov
    return false; // # nocov
  }

  for (int i = 0 ; i < snlin*sncol ; i++)
  {
    double x = rin.x_from_cell(i);
    double y = rin.y_from_cell(i);
    raster.set_value(x, y, ans[i]);
  }

  free(ans);

  return true;
}

double LASRpitfill::need_buffer() const
{
  int buff = MAX3(lap_size, med_size, dil_radius);
  return buff*raster.get_xres();
}

bool LASRpitfill::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  StageRaster* p = dynamic_cast<StageRaster*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'transform_with'"; // # nocov
    return false; // # nocov
  }

  return true;
}