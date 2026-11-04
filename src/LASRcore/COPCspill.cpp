#include "COPCspill.h"
#include "print.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <utility>
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

  // Parse LASR_COPC_SPILL_WRITE_BUDGET env var (bytes) — caps the aggregate
  // capacity of all per-cell write buffers. Returns 0 if unset or invalid.
  // Independent of LASR_COPC_RAM_BUDGET (which now bounds only the
  // pre-spill RAM-resident point bytes).
  U64 env_wb_budget()
  {
    const char* v = std::getenv("LASR_COPC_SPILL_WRITE_BUDGET");
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
                     U32 eviction_check_cadence,
                     U64 write_buf_budget)
  : output_path(output_path),
    point_size(point_record_size),
    ram_budget(ram_budget),
    write_buf_size(spilled_write_buffer_size),
    check_cadence(eviction_check_cadence == 0 ? 1u : eviction_check_cadence),
    wb_budget(write_buf_budget)
{
  U64 env = env_ram_budget();
  if (env > 0) this->ram_budget = env;
  U64 env_wb = env_wb_budget();
  if (env_wb > 0) this->wb_budget = env_wb;
}

COPCspill::~COPCspill()
{
  // Safety-net cleanup. Close the shared spill log so rmdir can succeed
  // (cleanup() unlinks the log file and the spill dir).
  if (spill_log_handle)
  {
    std::fclose(spill_log_handle);
    spill_log_handle = nullptr;
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
    cb.wb_lru_pos = wb_lru.end();
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
    if (agg_write_buf_bytes > wb_budget)
    {
      if (!enforce_write_buf_budget()) return false;
    }
  }
  return true;
}

void COPCspill::append_ram(CellBuffer& cell, const U8* bytes)
{
  // Track capacity, not logical size. std::vector growth doubles capacity,
  // so logical-N × point_size undercounts the real RAM footprint by up to
  // 2× — the same bug as the writer-side residents until commit 39de3cf1.
  // Snapshot capacity before/after the insert and add the delta to
  // agg_ram. Drop/spill subtracts cell.ram.capacity() before releasing
  // (see spill_cell / drop_octant) so the budget bookkeeping stays
  // balanced even when many small cells trigger geometric growth.
  const std::size_t cap_before = cell.ram.capacity();
  cell.ram.insert(cell.ram.end(), bytes, bytes + point_size);
  const std::size_t cap_after = cell.ram.capacity();
  agg_ram += (cap_after - cap_before);
}

bool COPCspill::append_spilled(const EPTkey& key, CellBuffer& cell, const U8* bytes)
{
  // write_buf is lazy: empty until the first append after spill or after
  // a wb_lru eviction. Allocate to capacity write_buf_size on first use,
  // and account that capacity in agg_write_buf_bytes (independent budget
  // from agg_ram, which now bounds only RAM-resident pre-spill buffers).
  if (cell.write_buf.capacity() == 0)
  {
    cell.write_buf.reserve(write_buf_size);
    agg_write_buf_bytes += write_buf_size;
  }

  if (cell.write_buf.size() + point_size > write_buf_size)
  {
    if (!flush_write_buf(key, cell)) return false;
  }
  size_t old = cell.write_buf.size();
  cell.write_buf.resize(old + point_size);
  std::memcpy(cell.write_buf.data() + old, bytes, point_size);

  // Mark this cell as the most-recently-touched write_buf. Splices the
  // existing wb_lru entry to the back (or inserts at back if absent).
  touch_wb_lru(key, cell);
  return true;
}

void COPCspill::touch_wb_lru(const EPTkey& key, CellBuffer& cell)
{
  if (cell.wb_lru_pos != wb_lru.end())
  {
    wb_lru.splice(wb_lru.end(), wb_lru, cell.wb_lru_pos);
  }
  else
  {
    wb_lru.push_back(key);
    cell.wb_lru_pos = std::prev(wb_lru.end());
  }
}

bool COPCspill::evict_write_buf(const EPTkey& key, CellBuffer& cell)
{
  // Flush any buffered bytes to disk, then release the write_buf capacity
  // and remove the cell from wb_lru. The cell stays spilled and may
  // re-allocate write_buf on its next append.
  if (cell.write_buf.capacity() == 0) return true; // nothing to do
  if (!flush_write_buf(key, cell)) return false;
  // Account: drop the previously-reserved capacity from the global count.
  if (agg_write_buf_bytes >= write_buf_size) agg_write_buf_bytes -= write_buf_size;
  else agg_write_buf_bytes = 0;
  std::vector<U8>().swap(cell.write_buf);
  if (cell.wb_lru_pos != wb_lru.end())
  {
    wb_lru.erase(cell.wb_lru_pos);
    cell.wb_lru_pos = wb_lru.end();
  }
  return true;
}

bool COPCspill::enforce_write_buf_budget()
{
  // Drive aggregate write_buf capacity below the LOW WATER mark (40% of
  // budget) so we don't get re-triggered on every append, and so we
  // leave headroom for transient bursts. Each iteration releases one
  // cell's write_buf (write_buf_size bytes typically), so this is
  // amortised cheap.
  const U64 low_water = (U64)((double)wb_budget * 0.4);
  while (agg_write_buf_bytes > low_water && !wb_lru.empty())
  {
    EPTkey victim_key = wb_lru.front();
    auto it = cells.find(victim_key);
    if (it == cells.end())
    {
      // Stale lru entry — pop and continue.
      wb_lru.pop_front();
      continue;
    }
    if (!evict_write_buf(victim_key, it->second)) return false;
  }
  if (agg_write_buf_bytes > wb_budget && !wb_budget_warned)
  {
    wb_budget_warned = true;
    warning("COPC writer: spill write-buffer budget cannot be enforced "
            "(agg %llu MB vs budget %llu MB). Raise "
            "LASR_COPC_SPILL_WRITE_BUDGET to give the spill more "
            "scratch RAM, or accept the overshoot.\n",
            (unsigned long long)(agg_write_buf_bytes >> 20),
            (unsigned long long)(wb_budget >> 20));
  }
  return true;
}

bool COPCspill::enforce_budget()
{
  if (cells.empty()) return true;

  // Tighter low-water (0.4 of budget) creates more headroom after each
  // enforce, so it doesn't re-trigger on the very next append. Was 0.9 —
  // too close to budget under heavy spill churn — and 0.5 was already
  // enough to amortise but leaves less reserve for fragmentation /
  // metadata. 0.4 trades a few extra spill cells per enforce for a
  // larger steady-state cushion.
  const U64 low_water = (U64)((double)ram_budget * 0.4);
  if (agg_ram <= low_water) return true;

  // Single-pass batch eviction. The previous implementation re-scanned
  // every cell to find the largest RAM-resident, spilled it, and looped
  // — O(N²) in the cell count. With ~144k cells on the sofi run that
  // pattern dominated CPU once the writer's flush_hot_octant started
  // feeding us appends faster than we could spill them. Same fix shape
  // as COPCwriter::enforce_resident_budget: snapshot, sort heaviest-
  // first (by capacity, since that's what costs RAM and what agg_ram
  // tracks), drain until under low_water.
  std::vector<std::pair<size_t, EPTkey>> sorted;
  sorted.reserve(cells.size());
  for (auto& kv : cells)
  {
    if (kv.second.spilled) continue;
    if (kv.second.ram.empty()) continue;
    sorted.emplace_back(kv.second.ram.capacity(), kv.first);
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  for (const auto& kv : sorted)
  {
    if (agg_ram <= low_water) break;

    auto it = cells.find(kv.second);
    if (it == cells.end()) continue;
    if (!spill_cell(kv.second, it->second)) return false;
  }

  // Persistent budget overrun: nothing left to evict from the RAM-resident
  // pool (every cell is already spilled). agg_ram should now be ~0 since
  // write_buf capacity moved to its own budget (agg_write_buf_bytes /
  // wb_budget). If we're still over here, the user has unusual conditions
  // and should raise LASR_COPC_RAM_BUDGET.
  if (agg_ram > ram_budget && !budget_overrun_warned)
  {
    budget_overrun_warned = true;
    warning("COPC writer: RAM budget cannot be enforced — nothing left "
            "RAM-resident to evict. agg_ram %llu MB vs budget %llu MB. "
            "Raise LASR_COPC_RAM_BUDGET or accept the overshoot.\n",
            (unsigned long long)(agg_ram >> 20),
            (unsigned long long)(ram_budget >> 20));
  }
  return true;
}

bool COPCspill::spill_cell(const EPTkey& key, CellBuffer& cell)
{
  if (cell.spilled) return true;

  if (!open_spill_log_if_needed()) return false;

  if (!cell.ram.empty())
  {
    U64 off = 0;
    if (!append_to_spill_log(cell.ram.data(), cell.ram.size(), off)) return false;
    cell.extents.push_back(Extent{off, (U32)cell.ram.size()});
  }

  // Subtract the cell's vector CAPACITY, not its logical size. Capacity
  // is what was added to agg_ram by append_ram (which tracks growth
  // deltas); subtracting size would leak the over-allocated bytes into
  // agg_ram.
  agg_ram -= cell.ram.capacity();
  std::vector<U8>().swap(cell.ram); // release capacity, not just .clear()
  cell.spilled = true;
  return true;
}

bool COPCspill::flush_write_buf(const EPTkey& key, CellBuffer& cell)
{
  // Append the buffered bytes to the shared spill log and record the
  // extent on the cell. Keeps write_buf's capacity for the next append.
  if (cell.write_buf.empty()) return true;
  U64 off = 0;
  if (!append_to_spill_log(cell.write_buf.data(), cell.write_buf.size(), off)) return false;
  cell.extents.push_back(Extent{off, (U32)cell.write_buf.size()});
  cell.write_buf.clear();
  return true;
}

bool COPCspill::open_spill_log_if_needed()
{
  if (spill_log_handle) return true;

  // Resolve directory: env override or output's parent.
  const char* env_dir = std::getenv("LASR_SPILL_DIR");
  std::string parent = env_dir && *env_dir ? std::string(env_dir) : parent_dir(output_path);
  std::string base = file_basename(output_path);

  // Stale-dir scan: list any pre-existing `<base>.copc-spill-*` in the
  // parent directory. Previous behaviour was to refuse to start when any
  // matched — overly strict, because (a) our new spill dir name embeds
  // the live PID + a random suffix, so a name collision with a stale
  // dir is statistically impossible, and (b) a single crashed prior
  // run blocks ALL subsequent runs (including unrelated processes
  // writing different outputs that share a basename prefix). Downgrade
  // to a one-shot warning that lists the matches so the operator can
  // clean up at their leisure, and proceed.
  std::vector<std::string> stale;
  if (find_stale_spill_dirs(parent, base, stale))
  {
    std::ostringstream oss;
    oss << "COPC writer: stale spill dir(s) found alongside output '"
        << base << "' in '" << parent << "' — likely from an aborted "
        "previous run. The current writer creates its own uniquely-"
        "named dir (PID + random suffix) so it will not collide. "
        "Consider removing:";
    for (const auto& s : stale) oss << " '" << s << "'";
    oss << "\n";
    warning("%s", oss.str().c_str());
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

  spill_log_path = spill_dir + LASR_PATH_SEP + "spill.log";
  spill_log_handle = std::fopen(spill_log_path.c_str(), "w+b");
  if (!spill_log_handle)
  {
    fail("cannot open spill log: " + spill_log_path);
    return false;
  }
  spill_log_size = 0;
  return true;
}

// 64-bit seek wrapper. fseek with `long` truncates 64-bit offsets on
// Windows (long is 32-bit there); use _fseeki64 / fseeko to keep
// large spill logs working past 2 GB. Returns 0 on success.
static int spill_seek64(FILE* fp, std::int64_t off, int whence)
{
#ifdef _WIN32
  return _fseeki64(fp, off, whence);
#else
  return fseeko(fp, (off_t)off, whence);
#endif
}

bool COPCspill::append_to_spill_log(const U8* bytes, std::size_t n, U64& out_offset)
{
  if (!spill_log_handle)
  {
    fail("internal error: append_to_spill_log called before open");
    return false;
  }
  // Seek to end-of-log before writing. read_octant() leaves the file
  // position somewhere inside the log after fseek+fread; without this
  // explicit seek the next append would overwrite earlier extent data.
  if (spill_seek64(spill_log_handle, (std::int64_t)spill_log_size, SEEK_SET) != 0)
  {
    fail("spill log seek-to-end failed: " + spill_log_path);
    return false;
  }
  out_offset = spill_log_size;
  std::size_t written = std::fwrite(bytes, 1, n, spill_log_handle);
  if (written != n)
  {
    fail("write to spill log failed: " + spill_log_path);
    return false;
  }
  spill_log_size += n;
  return true;
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
      // Flush any pending writes (records the final extent), then walk
      // each extent in order via the shared spill log. fseek + fread per
      // extent — typical extent is ~64 KB (a write_buf flush) so the
      // syscall overhead is amortised.
      if (!flush_write_buf(k, cell)) return false;
      if (!spill_log_handle && !cell.extents.empty())
      {
        fail("read_octant: spill log not open but cell has extents");
        return false;
      }
      for (const Extent& e : cell.extents)
      {
        if (cursor + e.byte_size > end)
        {
          fail("read_octant buffer overflow (spill)");
          return false;
        }
        if (spill_seek64(spill_log_handle, (std::int64_t)e.offset, SEEK_SET) != 0)
        {
          fail("read_octant: fseek failed in spill log " + spill_log_path);
          return false;
        }
        std::size_t n = std::fread(cursor, 1, e.byte_size, spill_log_handle);
        if (n != e.byte_size)
        {
          fail("read_octant: short read from spill log " + spill_log_path);
          return false;
        }
        cursor += e.byte_size;
      }
    }
  }

  // The caller sized byte_count = sum(cell.point_count * point_size) over
  // the supplied leaves; if cursor != end, either a leaf was missing
  // (handled above) or the cell's recorded extents in the shared spill
  // log don't sum to its tracked count. Both cases would otherwise feed
  // uninitialized bytes through to LAZ encoding.
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

bool COPCspill::flush_all_write_bufs()
{
  if (poisoned) return false;
  if (!spill_log_handle) return true;  // nothing was spilled, nothing to flush
  for (auto& kv : cells)
  {
    CellBuffer& cell = kv.second;
    if (!cell.spilled) continue;
    if (cell.write_buf.empty() && cell.write_buf.capacity() == 0) continue;
    // Flush appends an extent and clears write_buf, then we release its
    // capacity (the post-finalize emit phase reads from the spill log;
    // no point keeping write-buf RAM committed).
    if (!flush_write_buf(kv.first, cell)) return false;
    if (cell.write_buf.capacity() > 0)
    {
      if (agg_write_buf_bytes >= write_buf_size) agg_write_buf_bytes -= write_buf_size;
      else agg_write_buf_bytes = 0;
      std::vector<U8>().swap(cell.write_buf);
    }
    if (cell.wb_lru_pos != wb_lru.end())
    {
      wb_lru.erase(cell.wb_lru_pos);
      cell.wb_lru_pos = wb_lru.end();
    }
  }
  return true;
}

bool COPCspill::stream_octant(const std::vector<EPTkey>& leaves,
                              U64 expected_points,
                              const std::function<bool(const U8*)>& fn)
{
  if (poisoned) return false;

  // Stack-stable scratch buffer reused across spilled extents. Sized to
  // typical extent (write_buf_size, ~64 KB) and grown only if a single
  // extent exceeds it. Avoids per-extent malloc.
  std::vector<U8> extent_buf;

  // Mirror of read_octant's byte_count guard: track emitted points and
  // verify each spilled extent is a whole multiple of point_size. A
  // mismatch would otherwise produce a chunk shorter than what the
  // hierarchy records — corrupt COPC.
  U64 emitted = 0;

  for (const EPTkey& k : leaves)
  {
    auto it = cells.find(k);
    if (it == cells.end())
    {
      fail("stream_octant: leaf has no spill state (caller bookkeeping mismatch)");
      return false;
    }
    CellBuffer& cell = it->second;

    if (!cell.spilled)
    {
      // RAM-resident: walk the cell's bytes in point_size-stride chunks.
      // Guard against bookkeeping drift between point_count and ram.size:
      // if they ever desynchronise we'd read past the vector or drop
      // tail bytes. Symmetric with the spilled-path extent check below.
      const U64 n = cell.point_count;
      if (cell.ram.size() != n * (U64)point_size)
      {
        fail("stream_octant: RAM cell size mismatch — got "
             + std::to_string((U64)cell.ram.size())
             + " bytes, expected " + std::to_string(n * (U64)point_size));
        return false;
      }
      const U8* base = cell.ram.data();
      for (U64 i = 0; i < n; ++i)
      {
        if (!fn(base + i * point_size))
        {
          fail("stream_octant: callback aborted");
          return false;
        }
        ++emitted;
      }
    }
    else
    {
      // Spilled: same flush-then-walk pattern as read_octant. The flush
      // commits any pending write_buf to the shared spill log first; then
      // we read each extent into extent_buf and iterate.
      if (!flush_write_buf(k, cell)) return false;
      if (!spill_log_handle && !cell.extents.empty())
      {
        fail("stream_octant: spill log not open but cell has extents");
        return false;
      }
      for (const Extent& e : cell.extents)
      {
        if ((e.byte_size % point_size) != 0)
        {
          fail("stream_octant: extent byte_size not a multiple of point_size in "
               + spill_log_path);
          return false;
        }
        if (e.byte_size > extent_buf.size()) extent_buf.resize(e.byte_size);
        if (spill_seek64(spill_log_handle, (std::int64_t)e.offset, SEEK_SET) != 0)
        {
          fail("stream_octant: fseek failed in spill log " + spill_log_path);
          return false;
        }
        std::size_t got = std::fread(extent_buf.data(), 1, e.byte_size, spill_log_handle);
        if (got != e.byte_size)
        {
          fail("stream_octant: short read from spill log " + spill_log_path);
          return false;
        }
        const U64 n_in_extent = e.byte_size / point_size;
        for (U64 i = 0; i < n_in_extent; ++i)
        {
          if (!fn(extent_buf.data() + i * point_size))
          {
            fail("stream_octant: callback aborted");
            return false;
          }
          ++emitted;
        }
      }
    }
  }

  if (emitted != expected_points)
  {
    fail("stream_octant: emitted " + std::to_string(emitted)
         + " points, expected " + std::to_string(expected_points));
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
      // Subtract capacity to match append_ram's growth-aware bookkeeping.
      agg_ram -= cell.ram.capacity();
    }
    else
    {
      // Release write_buf capacity (if any) and remove from wb_lru —
      // write_buf is lazy and tracked in its own budget, so dropping a
      // spilled cell only affects agg_write_buf_bytes when it actually
      // had a buffer.
      if (cell.write_buf.capacity() > 0)
      {
        if (agg_write_buf_bytes >= write_buf_size) agg_write_buf_bytes -= write_buf_size;
        else agg_write_buf_bytes = 0;
        std::vector<U8>().swap(cell.write_buf);
      }
      if (cell.wb_lru_pos != wb_lru.end())
      {
        wb_lru.erase(cell.wb_lru_pos);
        cell.wb_lru_pos = wb_lru.end();
      }
      // The cell's bytes in the shared spill log become unreferenced
      // (dead bytes inside the file). We don't try to reclaim them — the
      // whole log is unlinked at cleanup() and the OS reclaims the
      // disk space then. Release the per-cell extents vector capacity.
      std::vector<Extent>().swap(cell.extents);
    }
    cells.erase(it);
  }
}

void COPCspill::cleanup()
{
  // Close the shared spill log handle (if still open). The per-cell
  // file_handle / open-fd LRU from the previous design are gone.
  if (spill_log_handle)
  {
    std::fclose(spill_log_handle);
    spill_log_handle = nullptr;
  }

  if (spill_dir_created)
  {
    // remove_spill_dir() unlinks the spill log file along with the
    // directory itself.
    remove_spill_dir(spill_dir);
    spill_dir_created = false;
  }
}

void COPCspill::fail(const std::string& msg)
{
  poisoned = true;
  if (error_msg.empty()) error_msg = msg;
}
