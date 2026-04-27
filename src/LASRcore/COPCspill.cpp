#include "COPCspill.h"
#include "print.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #include <windows.h>
  #define LASR_MKDIR(path) _mkdir(path)
  #define LASR_GETPID()    (unsigned long)GetCurrentProcessId()
  #define LASR_PATH_SEP    '\\'
#else
  #include <dirent.h>
  #include <unistd.h>
  #define LASR_MKDIR(path) mkdir((path), 0755)
  #define LASR_GETPID()    (unsigned long)getpid()
  #define LASR_PATH_SEP    '/'
#endif

namespace
{
  // Parse LASR_COPC_RAM_BUDGET env var (bytes). Returns 0 if unset or invalid.
  U64 env_ram_budget()
  {
    const char* v = std::getenv("LASR_COPC_RAM_BUDGET");
    if (!v || !*v) return 0;
    char* end = nullptr;
    unsigned long long n = std::strtoull(v, &end, 10);
    if (!end || *end != '\0' || n == 0) return 0;
    return (U64)n;
  }

  // Parent directory of `<output>`. If no separator, returns ".".
  std::string parent_dir(const std::string& path)
  {
    auto p = path.find_last_of("/\\");
    if (p == std::string::npos) return std::string(".");
    return path.substr(0, p);
  }

  // Basename of `<output>` (file name without directory).
  std::string file_basename(const std::string& path)
  {
    auto p = path.find_last_of("/\\");
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
  }

  bool path_exists(const std::string& p)
  {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
  }

  // Scan parent dir for any entry that starts with <basename>.copc-spill-
  // Returns true (and fills matches) if any found. Used to detect stale spill dirs.
  bool find_stale_spill_dirs(const std::string& parent,
                             const std::string& base,
                             std::vector<std::string>& matches)
  {
    const std::string prefix = base + ".copc-spill-";
    matches.clear();
#ifdef _WIN32
    std::string glob = parent + LASR_PATH_SEP + prefix + "*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(glob.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
      matches.emplace_back(std::string(fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = ::opendir(parent.c_str());
    if (!d) return false;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr)
    {
      std::string name(e->d_name);
      if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
        matches.push_back(name);
    }
    ::closedir(d);
#endif
    return !matches.empty();
  }

  // Remove dir recursively. Not a general-purpose implementation; assumes the
  // dir only contains regular files (no subdirs), which is true for our spill dir.
  bool remove_spill_dir(const std::string& dir)
  {
    if (dir.empty()) return true;
#ifdef _WIN32
    std::string glob = dir + LASR_PATH_SEP + "*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(glob.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
      do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = dir + LASR_PATH_SEP + name;
        DeleteFileA(full.c_str());
      } while (FindNextFileA(h, &fd));
      FindClose(h);
    }
    return RemoveDirectoryA(dir.c_str()) != 0;
#else
    DIR* d = ::opendir(dir.c_str());
    if (d)
    {
      struct dirent* e;
      while ((e = ::readdir(d)) != nullptr)
      {
        std::string name(e->d_name);
        if (name == "." || name == "..") continue;
        std::string full = dir + LASR_PATH_SEP + name;
        ::unlink(full.c_str());
      }
      ::closedir(d);
    }
    return ::rmdir(dir.c_str()) == 0;
#endif
  }

  // 32-bit random suffix, drawn once per process at construction time.
  U32 random_suffix()
  {
    static std::mt19937 gen{(std::mt19937::result_type)std::chrono::steady_clock::now().time_since_epoch().count()};
    std::uniform_int_distribution<U32> dist;
    return dist(gen);
  }
}

COPCspill::COPCspill(const std::string& output_path,
                     U32 point_record_size,
                     U64 ram_budget,
                     U32 spilled_write_buffer_size,
                     U32 open_fd_cap,
                     U32 eviction_check_cadence)
  : output_path(output_path),
    point_size(point_record_size),
    ram_budget(ram_budget),
    write_buf_size(spilled_write_buffer_size),
    fd_cap(open_fd_cap),
    check_cadence(eviction_check_cadence == 0 ? 1u : eviction_check_cadence)
{
  U64 env = env_ram_budget();
  if (env > 0) this->ram_budget = env;
}

COPCspill::~COPCspill()
{
  // Safety-net cleanup. Close any open handles first so rmdir can succeed.
  for (auto& kv : cells)
  {
    if (kv.second.file_handle)
    {
      std::fclose(kv.second.file_handle);
      kv.second.file_handle = nullptr;
    }
  }
  cleanup();
}

bool COPCspill::append(const EPTkey& key, const U8* bytes)
{
  if (poisoned) return false;

  auto it = cells.find(key);
  if (it == cells.end())
  {
    CellBuffer cb;
    cb.lru_pos = lru.end();
    auto ins = cells.emplace(key, std::move(cb));
    it = ins.first;
  }

  CellBuffer& cell = it->second;

  if (!cell.spilled)
  {
    append_ram(cell, bytes);
  }
  else
  {
    if (!append_spilled(key, cell, bytes)) return false;
  }
  cell.point_count++;

  if (++appends_since_check >= check_cadence)
  {
    appends_since_check = 0;
    if (agg_ram > ram_budget)
    {
      if (!enforce_budget()) return false;
    }
  }
  return true;
}

void COPCspill::append_ram(CellBuffer& cell, const U8* bytes)
{
  cell.ram.insert(cell.ram.end(), bytes, bytes + point_size);
  agg_ram += point_size;
}

bool COPCspill::append_spilled(const EPTkey& key, CellBuffer& cell, const U8* bytes)
{
  // write_buf is reserved to write_buf_size capacity at spill time and
  // accounted as a fixed reservation in agg_ram (see spill_cell). Appends
  // grow the size up to the capacity and flush; neither the resize nor
  // the flush changes the cell's memory footprint, so agg_ram doesn't
  // change here.
  if (cell.write_buf.size() + point_size > write_buf_size)
  {
    if (!flush_write_buf(key, cell)) return false;
  }
  size_t old = cell.write_buf.size();
  cell.write_buf.resize(old + point_size);
  std::memcpy(cell.write_buf.data() + old, bytes, point_size);
  return true;
}

bool COPCspill::enforce_budget()
{
  if (cells.empty()) return true;

  const U64 low_water = (U64)((double)ram_budget * 0.9);

  while (agg_ram > low_water)
  {
    EPTkey victim_key;
    size_t victim_size = 0;
    bool found = false;
    for (auto& kv : cells)
    {
      if (kv.second.spilled) continue;
      if (kv.second.ram.size() > victim_size)
      {
        victim_size = kv.second.ram.size();
        victim_key = kv.first;
        found = true;
      }
    }
    if (!found) break; // nothing RAM-resident left to evict

    auto it = cells.find(victim_key);
    if (it == cells.end()) break;
    if (!spill_cell(victim_key, it->second)) return false;
  }

  // Persistent budget overrun: every cell is already spilled and the only
  // remaining RAM is the per-cell write_buf reservations (write_buf_size
  // bytes per spilled cell). We can't reduce further without giving up
  // batched writes, so warn the user once per writer so they can either
  // raise LASR_COPC_RAM_BUDGET or accept the overshoot. Worst-case
  // overhead is bounded by (number of spilled cells) * write_buf_size.
  if (agg_ram > ram_budget && !budget_overrun_warned)
  {
    budget_overrun_warned = true;
    warning("COPC writer: RAM budget cannot be enforced — every cell is "
            "already spilled and per-cell write buffers are %llu bytes each. "
            "Aggregate RAM is %llu MB vs budget %llu MB. Raise "
            "LASR_COPC_RAM_BUDGET or accept the overshoot.\n",
            (unsigned long long)write_buf_size,
            (unsigned long long)(agg_ram >> 20),
            (unsigned long long)(ram_budget >> 20));
  }
  return true;
}

bool COPCspill::spill_cell(const EPTkey& key, CellBuffer& cell)
{
  if (cell.spilled) return true;

  if (!create_spill_dir_if_needed()) return false;

  cell.file_path = build_cell_path(key);
  if (!ensure_open(key, cell, "wb")) return false;

  if (!cell.ram.empty())
  {
    size_t n = std::fwrite(cell.ram.data(), 1, cell.ram.size(), cell.file_handle);
    if (n != cell.ram.size())
    {
      fail("write to spill file failed: " + cell.file_path);
      return false;
    }
  }

  agg_ram -= cell.ram.size();
  std::vector<U8>().swap(cell.ram); // release capacity, not just .clear()
  cell.write_buf.reserve(write_buf_size);
  agg_ram += write_buf_size; // account for the reserved capacity, which is real RAM
  cell.spilled = true;
  return true;
}

bool COPCspill::flush_write_buf(const EPTkey& key, CellBuffer& cell)
{
  // Flush moves bytes to disk but doesn't change the cell's RAM footprint:
  // write_buf's capacity (write_buf_size) is fixed and is what agg_ram
  // tracks for spilled cells.
  if (cell.write_buf.empty()) return true;
  if (!ensure_open(key, cell, "ab")) return false;
  size_t n = std::fwrite(cell.write_buf.data(), 1, cell.write_buf.size(), cell.file_handle);
  if (n != cell.write_buf.size())
  {
    fail("write to spill file failed: " + cell.file_path);
    return false;
  }
  cell.write_buf.clear();
  return true;
}

bool COPCspill::ensure_open(const EPTkey& key, CellBuffer& cell, const char* mode)
{
  if (cell.file_handle)
  {
    // Touch LRU position.
    if (cell.lru_pos != lru.end())
    {
      lru.erase(cell.lru_pos);
    }
    lru.push_back(key);
    cell.lru_pos = std::prev(lru.end());
    return true;
  }

  // Enforce fd cap.
  while (lru.size() >= fd_cap)
  {
    EPTkey oldest = lru.front();
    lru.pop_front();
    auto it = cells.find(oldest);
    if (it != cells.end() && it->second.file_handle)
    {
      std::fclose(it->second.file_handle);
      it->second.file_handle = nullptr;
      it->second.lru_pos = lru.end();
    }
  }

  cell.file_handle = std::fopen(cell.file_path.c_str(), mode);
  if (!cell.file_handle)
  {
    fail("cannot open spill file: " + cell.file_path);
    return false;
  }
  lru.push_back(key);
  cell.lru_pos = std::prev(lru.end());
  return true;
}

void COPCspill::close_handle(CellBuffer& cell)
{
  if (cell.file_handle)
  {
    std::fclose(cell.file_handle);
    cell.file_handle = nullptr;
  }
  if (cell.lru_pos != lru.end())
  {
    lru.erase(cell.lru_pos);
    cell.lru_pos = lru.end();
  }
}

bool COPCspill::create_spill_dir_if_needed()
{
  if (spill_dir_created) return true;

  const char* env_dir = std::getenv("LASR_SPILL_DIR");
  std::string parent = env_dir && *env_dir ? std::string(env_dir) : parent_dir(output_path);
  std::string base = file_basename(output_path);

  std::vector<std::string> stale;
  if (find_stale_spill_dirs(parent, base, stale))
  {
    std::ostringstream oss;
    oss << "cannot create COPC spill directory: pre-existing dir(s) match '"
        << base << ".copc-spill-*' in '" << parent
        << "'. Remove stale directories before retrying. Found:";
    for (const auto& s : stale) oss << " '" << s << "'";
    fail(oss.str());
    return false;
  }

  std::ostringstream oss;
  oss << parent << LASR_PATH_SEP << base << ".copc-spill-"
      << LASR_GETPID() << "-" << std::hex << random_suffix();
  spill_dir = oss.str();

  if (LASR_MKDIR(spill_dir.c_str()) != 0)
  {
    fail("failed to create COPC spill directory: " + spill_dir);
    return false;
  }
  spill_dir_created = true;
  return true;
}

std::string COPCspill::build_cell_path(const EPTkey& key) const
{
  std::ostringstream oss;
  oss << spill_dir << LASR_PATH_SEP
      << key.d << "_" << key.x << "_" << key.y << "_" << key.z << ".bin";
  return oss.str();
}

std::unordered_map<EPTkey, U64, EPTKeyHasher> COPCspill::cell_counts() const
{
  std::unordered_map<EPTkey, U64, EPTKeyHasher> out;
  out.reserve(cells.size());
  for (const auto& kv : cells) out.emplace(kv.first, kv.second.point_count);
  return out;
}

bool COPCspill::read_octant(const std::vector<EPTkey>& leaves, U8* out_buffer, U64 byte_count)
{
  if (poisoned) return false;

  U8* cursor = out_buffer;
  U8* end = out_buffer + byte_count;

  for (const EPTkey& k : leaves)
  {
    auto it = cells.find(k);
    if (it == cells.end())
    {
      // A leaf in the caller's list has no corresponding cell — that means
      // either the cell was already dropped or the caller's count
      // bookkeeping disagrees with the spill's. Either way the sort_buf
      // would end up partially uninitialized; refuse loudly.
      fail("read_octant: leaf has no spill state (caller bookkeeping mismatch)");
      return false;
    }
    CellBuffer& cell = it->second;

    if (!cell.spilled)
    {
      if (cursor + cell.ram.size() > end)
      {
        fail("read_octant buffer overflow (ram)");
        return false;
      }
      std::memcpy(cursor, cell.ram.data(), cell.ram.size());
      cursor += cell.ram.size();
    }
    else
    {
      // Flush any pending writes, then rewind and stream contents into the buffer.
      if (!flush_write_buf(k, cell)) return false;
      close_handle(cell);

      FILE* f = std::fopen(cell.file_path.c_str(), "rb");
      if (!f)
      {
        fail("cannot open spill file for read: " + cell.file_path);
        return false;
      }

      // Stream in chunks to avoid a large temporary.
      constexpr size_t CHUNK = 1 << 16;
      U8 scratch[CHUNK];
      while (true)
      {
        size_t n = std::fread(scratch, 1, CHUNK, f);
        if (n == 0) break;
        if (cursor + n > end)
        {
          std::fclose(f);
          fail("read_octant buffer overflow (spill)");
          return false;
        }
        std::memcpy(cursor, scratch, n);
        cursor += n;
      }
      // Distinguish EOF (clean end) from I/O error. ferror() on a stream
      // that hit a read error is non-zero; without this check a short
      // read from a truncated spill file would leave the tail of
      // sort_buf uninitialized and pass through to the LAZ encoder.
      const bool io_err = std::ferror(f) != 0;
      std::fclose(f);
      if (io_err)
      {
        fail("read_octant: I/O error reading spill file " + cell.file_path);
        return false;
      }
    }
  }

  // The caller sized byte_count = sum(cell.point_count * point_size) over
  // the supplied leaves; if cursor != end, either a leaf was missing
  // (handled above) or a spill file was truncated relative to its
  // tracked count. Both cases would otherwise feed uninitialized bytes
  // through to LAZ encoding.
  if (cursor != end)
  {
    fail("read_octant: short read — gathered "
         + std::to_string((U64)(cursor - out_buffer))
         + " bytes, expected "
         + std::to_string(byte_count));
    return false;
  }
  return true;
}

void COPCspill::drop_octant(const std::vector<EPTkey>& leaves)
{
  for (const EPTkey& k : leaves)
  {
    auto it = cells.find(k);
    if (it == cells.end()) continue;
    CellBuffer& cell = it->second;

    if (!cell.spilled)
    {
      agg_ram -= cell.ram.size();
    }
    else
    {
      // Spilled cell's RAM footprint is the reserved write_buf capacity,
      // not write_buf.size(); see spill_cell / append_spilled accounting.
      agg_ram -= write_buf_size;
      close_handle(cell);
      if (!cell.file_path.empty())
      {
#ifdef _WIN32
        DeleteFileA(cell.file_path.c_str());
#else
        ::unlink(cell.file_path.c_str());
#endif
      }
    }
    cells.erase(it);
  }
}

void COPCspill::cleanup()
{
  // Close any open handles left over (e.g. on error paths).
  for (auto& kv : cells)
  {
    if (kv.second.file_handle)
    {
      std::fclose(kv.second.file_handle);
      kv.second.file_handle = nullptr;
    }
  }
  lru.clear();

  if (spill_dir_created)
  {
    remove_spill_dir(spill_dir);
    spill_dir_created = false;
  }
}

void COPCspill::fail(const std::string& msg)
{
  poisoned = true;
  if (error_msg.empty()) error_msg = msg;
}
