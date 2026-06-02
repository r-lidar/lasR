#include "li2012.h"

LASRli2012::LASRli2012()
  : unicity_table(std::make_shared<
        std::unordered_map<ApexKey, int, ApexKeyHash>>()),
    global_counter(std::make_shared<std::atomic<int>>(0)),
    table_mutex(std::make_shared<std::mutex>())
{
}

bool LASRli2012::set_parameters(const nlohmann::json& stage)
{
  dt1 = stage.value("dt1", 1.5);
  dt2 = stage.value("dt2", 2.0);
  R = stage.value("R", 2.0);
  Zu = stage.value("Zu", 15.0);
  hmin = stage.value("hmin", 2.0);
  speed_up = stage.value("speed_up", 10.0);
  store_in_attribute = stage.value("store_in_attribute",
                                   std::string("treeID"));

  if (dt1 <= 0)      { last_error = "dt1 must be positive";       return false; }
  if (dt2 <= 0)      { last_error = "dt2 must be positive";       return false; }
  if (R < 0)         { last_error = "R must be non-negative";     return false; }
  if (Zu <= 0)       { last_error = "Zu must be positive";        return false; }
  if (hmin <= 0)     { last_error = "hmin must be positive";      return false; }
  if (speed_up <= 0) { last_error = "speed_up must be positive";  return false; }
  if (store_in_attribute.empty())
  { last_error = "store_in_attribute must be a non-empty string"; return false; }

  return true;
}

bool LASRli2012::process(PointCloud*& las)
{
  // Stub — implementation lands in Tasks 4-10.
  return true;
}

int LASRli2012::register_apex(const ApexKey& key)
{
  std::lock_guard<std::mutex> lock(*table_mutex);
  auto it = unicity_table->find(key);
  if (it != unicity_table->end()) return it->second;
  int tree_id = ++(*global_counter);
  (*unicity_table)[key] = tree_id;
  return tree_id;
}

void LASRli2012::clear(bool last)
{
  // No per-chunk state to reset. The shared dedup table must persist
  // across chunks under concurrent-files; we deliberately do NOT
  // reset unicity_table or global_counter on clear(false).
  // On clear(last=true) the shared_ptrs die with the stage anyway.
  (void)last;
}
