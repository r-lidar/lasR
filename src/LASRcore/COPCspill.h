#ifndef COPC_SPILL_H
#define COPC_SPILL_H

#include <cstdio>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "lascopc.hpp"
#include "mydefs.hpp"

// Per-cell point-byte accumulator with adaptive RAM-then-spill behaviour.
//
// Design:
//   - Each cell starts RAM-resident: a growing vector<U8> holds raw point bytes.
//   - Aggregate RAM usage is tracked. When it exceeds a configurable budget
//     (default 256 MB), the largest RAM-resident cells are evicted to per-cell
//     temp files, one at a time, until pressure subsides. Eviction is one-way:
//     once spilled, a cell stays spilled.
//   - A spilled cell keeps a small fixed write-buffer (default 64 KB) in RAM.
//     Appends fill that buffer; flushes go to the cell's temp file.
//   - Open temp-file handles are LRU-capped (default 256). Handles close on
//     eviction and reopen (O_APPEND-equivalent) on next flush.
//   - The spill directory is created lazily on the first eviction. Small inputs
//     that never exceed the budget never touch disk.
class COPCspill
{
public:
  COPCspill(const std::string& output_path,
            U32 point_record_size,
            U64 ram_budget = 256ULL * 1024 * 1024,
            U32 spilled_write_buffer_size = 64 * 1024,
            U32 open_fd_cap = 256,
            U32 eviction_check_cadence = 4096,
            U64 write_buf_budget = 128ULL * 1024 * 1024);

  ~COPCspill();

  // Non-copyable (owns file handles).
  COPCspill(const COPCspill&) = delete;
  COPCspill& operator=(const COPCspill&) = delete;

  // Append one point's bytes (of size point_record_size) to the given leaf.
  // Returns false on I/O error; after a failure the writer is poisoned and
  // subsequent calls will also return false.
  bool append(const EPTkey& key, const U8* bytes);

  // Per-leaf point counts. Includes every key ever appended to.
  std::unordered_map<EPTkey, U64, EPTKeyHasher> cell_counts() const;

  // Read the raw point bytes for a final octant (one or more source leaves),
  // in the order the leaves are listed and, within each leaf, the order points
  // were appended. out_buffer must be at least byte_count bytes. byte_count
  // must equal sum(counts[leaf]) * point_record_size over the given leaves.
  bool read_octant(const std::vector<EPTkey>& leaves, U8* out_buffer, U64 byte_count);

  // Release all resources for an octant's leaves (in-memory buffers + temp files).
  void drop_octant(const std::vector<EPTkey>& leaves);

  // Remove the spill directory if any files were ever spilled. No-op otherwise.
  // Called from close() on success and from the destructor as a safety net.
  void cleanup();

  bool is_poisoned() const { return poisoned; }
  const std::string& last_error() const { return error_msg; }

  // Test / introspection hooks
  bool spill_dir_exists() const { return spill_dir_created; }
  const std::string& spill_dir_path() const { return spill_dir; }
  U64 aggregate_ram_bytes() const { return agg_ram; }

private:
  struct CellBuffer
  {
    U64 point_count = 0;

    // RAM-resident state (spilled == false): holds all bytes for this cell.
    std::vector<U8> ram;

    // Spilled state (spilled == true): ram is cleared; bytes live in the temp
    // file. write_buf is lazily allocated on the first append after spill,
    // and is released by the LRU evictor when the global write-buffer budget
    // is exceeded. A spilled cell with no write_buf simply re-allocates on
    // its next append.
    bool spilled = false;
    std::string file_path;
    FILE* file_handle = nullptr;
    std::vector<U8> write_buf;

    // Position in the open-fd LRU list when file_handle != nullptr; end()
    // otherwise.
    std::list<EPTkey>::iterator lru_pos;

    // Position in the write-buffer LRU list when write_buf has capacity;
    // end() otherwise. Tracks the "active" set of write buffers so the
    // evictor can pick the least-recently-touched one to release.
    std::list<EPTkey>::iterator wb_lru_pos;
  };

  // Append to a RAM-resident cell. Updates agg_ram.
  void append_ram(CellBuffer& cell, const U8* bytes);

  // Append to a spilled cell. Flushes write_buf if full. Returns false on I/O error.
  bool append_spilled(const EPTkey& key, CellBuffer& cell, const U8* bytes);

  // Budget enforcement: evict largest RAM-resident cells until aggregate < 0.9*budget.
  // Called every eviction_check_cadence appends.
  bool enforce_budget();

  // Write-buffer eviction: while aggregate write_buf capacity is over the
  // configured budget, flush + release the LRU cell's write_buf. Lets a
  // huge fan-out of spilled cells share a small bounded RAM pool instead
  // of each pinning write_buf_size bytes forever.
  bool enforce_write_buf_budget();

  // Transition a RAM-resident cell to Spilled: write its ram vector to a temp
  // file, clear ram, set spilled flags. write_buf is now lazily allocated by
  // append_spilled, not here. Returns false on I/O error.
  bool spill_cell(const EPTkey& key, CellBuffer& cell);

  // Flush the cell's write_buf to its temp file, keeping its capacity for
  // the next append. Returns false on I/O error.
  bool flush_write_buf(const EPTkey& key, CellBuffer& cell);

  // Flush + release the cell's write_buf capacity. Removes the cell from
  // wb_lru and decrements agg_write_buf_bytes. The cell stays spilled and
  // can re-append later (the next append re-allocates write_buf).
  bool evict_write_buf(const EPTkey& key, CellBuffer& cell);

  // Mark `key`/`cell` as the most-recently-touched write_buf in wb_lru.
  // Splices an existing entry to the back, or inserts at the back if absent.
  // Caller is responsible for ensuring write_buf has capacity > 0.
  void touch_wb_lru(const EPTkey& key, CellBuffer& cell);

  // Ensure the cell's file handle is open. Enforces fd cap by closing LRU entries.
  bool ensure_open(const EPTkey& key, CellBuffer& cell, const char* mode);

  // Close a cell's file handle and remove it from the LRU list.
  void close_handle(CellBuffer& cell);

  // Create the spill directory on first use. Returns false on conflict with
  // any pre-existing <output>.copc-spill-* directory.
  bool create_spill_dir_if_needed();

  // Build the temp file path for a given key, inside spill_dir.
  std::string build_cell_path(const EPTkey& key) const;

  void fail(const std::string& msg);

private:
  std::string output_path;
  U32 point_size;
  U64 ram_budget;
  U32 write_buf_size;
  U32 fd_cap;
  U32 check_cadence;

  std::unordered_map<EPTkey, CellBuffer, EPTKeyHasher> cells;
  U64 agg_ram = 0;
  U32 appends_since_check = 0;

  // LRU of cells whose file_handle is currently open. Front = least recently used.
  std::list<EPTkey> lru;

  // Independent LRU of cells whose write_buf has capacity allocated. Front
  // is the least-recently-touched (next eviction candidate). The aggregate
  // capacity across all entries is tracked in agg_write_buf_bytes and kept
  // under wb_budget by enforce_write_buf_budget().
  std::list<EPTkey> wb_lru;
  U64 wb_budget;
  U64 agg_write_buf_bytes = 0;
  bool wb_budget_warned = false;

  bool spill_dir_created = false;
  std::string spill_dir;

  bool poisoned = false;
  std::string error_msg;
  bool budget_overrun_warned = false;
};

#endif
