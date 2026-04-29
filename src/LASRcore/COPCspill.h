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
//   - Aggregate RAM (capacity-tracked) is bounded by a configurable budget
//     (default 256 MB). When over budget the largest RAM-resident cells are
//     spilled in a single batch pass until aggregate < 40% of budget.
//     Eviction is one-way: once spilled, a cell stays spilled.
//   - All spilled bytes append to a single shared spill log file. Each
//     cell records (offset, byte_size) extents into the log; read_octant()
//     walks those extents via fseek+fread. This replaces the previous
//     "one temp file per cell" design that paid for thousands of file
//     paths, FILE* handles, an open-fd LRU, and per-file inodes — on
//     huge fan-out inputs that overhead dominated both RSS and syscall
//     time.
//   - A spilled cell keeps a small write-buffer (default 64 KB) in RAM,
//     allocated lazily on first append after spill. The aggregate
//     write-buffer capacity across cells is bounded by a separate
//     budget; the LRU-front cell's buffer is flushed and released when
//     the budget is exceeded.
//   - The spill directory is created lazily on the first eviction. Small
//     inputs that never exceed the RAM budget never touch disk.
class COPCspill
{
public:
  // Default budgets exposed so callers (e.g. COPCwriter) can build their
  // own fallback paths without duplicating literal byte counts and
  // silently diverging if these change.
  static constexpr U64 DEFAULT_RAM_BUDGET       = 256ULL * 1024 * 1024;
  static constexpr U64 DEFAULT_WRITE_BUF_BUDGET = 128ULL * 1024 * 1024;
  static constexpr U32 DEFAULT_WRITE_BUF_SIZE   = 64u * 1024u;
  static constexpr U32 DEFAULT_CHECK_CADENCE    = 4096u;

  COPCspill(const std::string& output_path,
            U32 point_record_size,
            U64 ram_budget = DEFAULT_RAM_BUDGET,
            U32 spilled_write_buffer_size = DEFAULT_WRITE_BUF_SIZE,
            U32 eviction_check_cadence = DEFAULT_CHECK_CADENCE,
            U64 write_buf_budget = DEFAULT_WRITE_BUF_BUDGET);

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

  // Release all per-cell resources for an octant's leaves: in-memory
  // buffers and the per-cell extent vectors. Bytes already written to
  // the shared spill log become dead (unreferenced) and will be reclaimed
  // by cleanup() unlinking the whole log file at close.
  void drop_octant(const std::vector<EPTkey>& leaves);

  // Close the shared spill log handle and remove the spill directory
  // (with the log file inside). No-op if no spill ever happened.
  // Called from close() on success and from the destructor as a safety net.
  void cleanup();

  bool is_poisoned() const { return poisoned; }
  const std::string& last_error() const { return error_msg; }

  // Test / introspection hooks
  bool spill_dir_exists() const { return spill_dir_created; }
  const std::string& spill_dir_path() const { return spill_dir; }
  U64 aggregate_ram_bytes() const { return agg_ram; }

private:
  // Single append-only spill log + per-cell extents. Replaces the
  // previous "one temp file per spilled cell" design where each of N
  // cells held a file_path string, a FILE* handle (LRU-capped at 256
  // open), and an inode on disk. With ~144k cells on the sofi run that
  // overhead was a real footprint and a real source of system-call
  // churn; this design folds it into a single FD and per-cell vectors
  // of (offset, byte_size) extents into the shared log file.
  //
  // Per spilled cell now: a vector<Extent>. With typical write-buf
  // flushes producing ~64 KB extents, even fully-flushed octants
  // accumulate only a handful of extents (3 MB / 64 KB ≈ 50). The
  // per-cell file_path string (~50 B), file_handle (8 B), open-fd
  // LRU iterator (16-24 B), and the per-file inode metadata are
  // gone. write_buf is still lazy + LRU-budgeted because it's a hot
  // batching mechanism, not per-cell state.
  struct Extent
  {
    U64 offset;     // byte offset into the spill log
    U32 byte_size;  // length of this run
  };

  struct CellBuffer
  {
    U64 point_count = 0;

    // RAM-resident state (spilled == false): holds all bytes for this cell.
    std::vector<U8> ram;

    // Spilled state (spilled == true): ram is cleared; bytes live in the
    // shared spill log at the recorded extents. write_buf is lazily
    // allocated on the first append after spill and is released by the
    // wb_lru evictor when the global write-buffer budget is exceeded.
    bool spilled = false;
    std::vector<Extent> extents;
    std::vector<U8> write_buf;

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

  // Transition a RAM-resident cell to Spilled: write its ram vector to
  // the shared spill log, record the resulting extent on the cell, clear
  // ram. write_buf is lazily allocated by append_spilled, not here.
  // Returns false on I/O error.
  bool spill_cell(const EPTkey& key, CellBuffer& cell);

  // Flush the cell's write_buf to the shared spill log and append a
  // new extent to the cell's extents vector, keeping write_buf's
  // capacity for the next append. Returns false on I/O error.
  bool flush_write_buf(const EPTkey& key, CellBuffer& cell);

  // Flush + release the cell's write_buf capacity. Removes the cell from
  // wb_lru and decrements agg_write_buf_bytes. The cell stays spilled and
  // can re-append later (the next append re-allocates write_buf).
  bool evict_write_buf(const EPTkey& key, CellBuffer& cell);

  // Mark `key`/`cell` as the most-recently-touched write_buf in wb_lru.
  // Splices an existing entry to the back, or inserts at the back if absent.
  // Caller is responsible for ensuring write_buf has capacity > 0.
  void touch_wb_lru(const EPTkey& key, CellBuffer& cell);

  // Open the shared spill log file lazily on the first spill, and create
  // its parent spill directory if needed. Returns false on I/O or path
  // conflict.
  bool open_spill_log_if_needed();

  // Append `bytes` to the spill log, returning the file offset at which
  // the run starts. Caller records (offset, n) as the cell's extent.
  // Returns false on I/O error.
  bool append_to_spill_log(const U8* bytes, std::size_t n, U64& out_offset);

  void fail(const std::string& msg);

private:
  std::string output_path;
  U32 point_size;
  U64 ram_budget;
  U32 write_buf_size;
  U32 check_cadence;

  std::unordered_map<EPTkey, CellBuffer, EPTKeyHasher> cells;
  U64 agg_ram = 0;
  U32 appends_since_check = 0;
  // Old per-cell-file LRU cap is dropped — the spill log is one FD.
  // The member is gone from the public ctor and the implementation.

  // Single shared spill log: opened lazily on the first spill, all
  // spilled bytes append here. Per-cell extents (offset, byte_size)
  // hand out windows into this file. Removes per-cell file paths,
  // file handles, and the open-fd LRU that the previous design had
  // to maintain across thousands of cells.
  FILE* spill_log_handle = nullptr;
  std::string spill_log_path;
  U64 spill_log_size = 0;  // current end-of-file = offset for the next append

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
