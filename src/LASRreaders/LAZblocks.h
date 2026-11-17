#ifndef LAZBLOCKS_H
#define LAZBLOCKS_H

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class LASreader;
class LASreaderLAS;
class LASreadOpener;
class LASheader;
class LASpoint;

// Decodes the LAS/LAZ files of a LASreader with several threads and hands the points back one by
// one, in the order the reader would have read them.
//
// A LAZ file is a sequence of chunks compressed independently. The files are cut into numbered
// blocks of whole chunks. With a spatial index (LAX or COPC) a block only holds the intervals of
// points that the sequential reader would visit. The workers decode the blocks in any order and
// next() consumes them in the order of their numbers. Only a small window of blocks is decoded ahead
// of the consumer, so the memory is bounded.
//
// A worker applies the spatial query and converts the points (both are independent of the order of
// the points). A LASlib filter may depend on the order: it is applied by next().
class LazBlocks
{
public:
  // Converts a decoded point into a record of 'record_size' bytes. Each worker has its own copy.
  using Converter = std::function<void(const LASpoint&, const LASheader&, unsigned char*)>;

  // 'main' is the reader opened by 'opener'. Throws std::runtime_error, which says why, when its
  // files cannot be decoded this way.
  LazBlocks(LASreadOpener& opener, LASreader& main, int threads, std::size_t record_size, const Converter& convert);
  ~LazBlocks();

  LazBlocks(const LazBlocks&) = delete;
  LazBlocks& operator=(const LazBlocks&) = delete;

  const unsigned char* next(); // the next record, nullptr at the end. Throws the error of a worker.
  int threads() const { return (int)workers.size(); }

private:
  struct File
  {
    std::string path;
    bool rescale, reoffset; // the coordinates must be converted to the scales and offsets of 'main'
    bool indexed;           // read with a spatial index
  };

  struct Block
  {
    std::size_t file;
    std::vector<std::pair<std::int64_t, std::int64_t>> pieces; // (first point, number of points)
    std::int64_t count = 0;

    // Filled by a worker
    std::vector<unsigned char> data; // the records of the points in the query
    std::size_t kept = 0;
    std::string error;
    bool done = false;
  };

  void work(Converter convert);
  void decode(Block& block, LASreaderLAS& reader, Converter& convert) const;
  void stop();

  LASreader& main;
  std::vector<File> files;
  std::vector<Block> blocks;
  std::size_t raw_size = 0; // the decoded LASpoint in front of each record, for the LASlib filter
  std::size_t stride = 0;   // raw_size + record_size
  std::size_t window = 0;

  std::vector<std::thread> workers;
  std::mutex mutex;
  std::condition_variable wakeup;
  std::size_t next_job = 0; // next block to decode
  std::size_t current = 0;  // block read by next()
  std::size_t cursor = 0;   // next record of this block
  bool stopping = false;
};

#endif
