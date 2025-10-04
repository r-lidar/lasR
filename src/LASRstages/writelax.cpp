#include "writelax.h"
#include "macros.h"

#include "LASio.h"

LASRlaxwriter::LASRlaxwriter()
{
  embedded = false;
  onthefly = false;
  overwrite = false;
}

LASRlaxwriter::LASRlaxwriter(bool embedded, bool overwrite, bool onthefly)
{
  this->embedded = embedded;
  this->overwrite = overwrite;
  this->onthefly = onthefly;
}

bool LASRlaxwriter::set_parameters(const nlohmann::json& stage)
{
  embedded = stage.value("embedded", false);
  overwrite = stage.value("overwrite", false);
  return true;
}

bool LASRlaxwriter::process(FileCollection*& ctg)
{
  if (onthefly) return true;

  bool success = true;
  const auto& files = ctg->get_files();

  Progress progress;
  Progress* current = this->progress;
  progress.set_prefix("Pre-processing");
  progress.set_total(files.size());
  progress.set_display(current->get_display());
  progress.set_ncpu(ncpu_concurrent_files);
  progress.create_subprocess();
  this->progress = &progress;

  LASio lasio(&progress);

  #pragma omp parallel for num_threads(ncpu_concurrent_files)
  for (size_t i = 0 ; i < files.size() ; i++)
  {
    if (!success) continue;
    std::string file = files[i].string();
    if (!lasio.write_lax(file, overwrite, embedded)) success = false;

    #pragma omp critical
    {
      progress.update(i, true);
      progress.show();
    }
  }

  ctg->set_all_indexed();

  progress.done();
  progress.done(true);
  this->progress = current;

  return success;
}

bool LASRlaxwriter::set_chunk(Chunk& chunk)
{
  if (!onthefly) return true;

  bool success = true;

  LASio lasio(progress);

  #pragma omp critical (write_lax)
  {
    std::vector<std::string> files;
    files.insert(files.end(), chunk.main_files.begin(), chunk.main_files.end());
    files.insert(files.end(), chunk.neighbour_files.begin(), chunk.neighbour_files.end());

    for (const auto& file : files)
    {
      if (!success) continue;
      success = lasio.write_lax(file, overwrite, embedded);
    }
  }

  return success;
}


