#ifndef LI2012_H
#define LI2012_H

#include "Stage.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

class LASRli2012 : public Stage
{
public:
  LASRli2012();
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  double need_buffer() const override { return 2.0 * speed_up + R / 2.0; }
  bool is_streamable() const override { return false; }
  bool is_parallelizable() const override { return true; }
  bool is_parallelized() const override { return false; }
  std::string get_name() const override { return "li2012"; }
  LASRli2012* clone() const override { return new LASRli2012(*this); }
  void clear(bool last = false) override;

private:
  // Algorithm parameters (set by set_parameters via JSON).
  double dt1 = 1.5;
  double dt2 = 2.0;
  double R = 2.0;
  double Zu = 15.0;
  double hmin = 2.0;
  double speed_up = 10.0;
  std::string store_in_attribute = "treeID";

  // Apex-keyed dedup state (shared across worker threads).
  struct ApexKey {
    int32_t x, y, z;
    bool operator==(const ApexKey& o) const
    { return x == o.x && y == o.y && z == o.z; }
  };
  struct ApexKeyHash {
    size_t operator()(const ApexKey& k) const noexcept {
      uint64_t h = (uint64_t)(uint32_t)k.x * 0x9E3779B185EBCA87ULL;
      h ^= (uint64_t)(uint32_t)k.y + 0xC2B2AE3D27D4EB4FULL
           + (h << 6) + (h >> 2);
      h ^= (uint64_t)(uint32_t)k.z + 0x165667B19E3779F9ULL
           + (h << 6) + (h >> 2);
      return (size_t)h;
    }
  };
  std::shared_ptr<std::unordered_map<ApexKey, int, ApexKeyHash>> unicity_table;
  std::shared_ptr<std::atomic<int>>                              global_counter;
  std::shared_ptr<std::mutex>                                    table_mutex;

  int register_apex(const ApexKey& key);
};

#endif
