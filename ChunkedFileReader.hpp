#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

#include "MPSCQueue.hpp" // Assume this is your MPSCQueue header
#include "MktData.hpp"
#include "MktDataMessage.hpp"
#include "Mmf.hpp"
#include "utils.hpp" // Assume this contains GetMaxMemoryPerThread

using QueueType = sp::MPSCQueue<sp::MktDataMessage>;
inline std::atomic_size_t thread_count_{0};

namespace sp {
class ChunkedFileReader {
public:
  ChunkedFileReader() = delete;
  ChunkedFileReader(const ChunkedFileReader&) = delete;
  ChunkedFileReader& operator=(const ChunkedFileReader&) = delete;
  ChunkedFileReader(ChunkedFileReader&&) = delete;
  ChunkedFileReader& operator=(ChunkedFileReader&&) = delete;

  ChunkedFileReader(
    const std::string &filename,
    QueueType &queue,
    size_t chunk_size = GetDefaultChunkSize(),
    std::chrono::seconds timespan = std::chrono::hours(1))
    :
      filename_(filename),
      symbol_(filename_.substr(filename_.find_first_of(".") + 1)),
      queue_(queue),
      chunk_size_(chunk_size),
      stop_flag_(false),
      mmf_(filename_,0, chunk_size_, sp::MMF::OpenMode::ReadOnly) {
      std::cout << "Constructed ChunkedFileReader for file: " << filename_
              << " with symbol: " << symbol_
              << " and chunk size: " << chunk_size_ << std::endl;
    }

  void Run() {
    if (!mmf_.IsValid()) {
      std::cerr << "Failed to open file: " << filename_ << " with error: "
                << mmf_.GetLastError() << std::endl;
      return;
    }
    ++thread_count_;

    std::cout << "Starting thread " << thread_id_ << " for file: " << filename_
              << " with symbol: " << symbol_
              << " and chunk size: " << chunk_size_ << std::endl;

    using namespace std::chrono;
    size_t prev_hour_ = 0;
    while (!stop_flag_) {
      auto line_opt = mmf_.ReadLineView(true);
      if (!line_opt) break;
      if (line_opt->empty()) continue; // Skip empty lines
      if (line_opt->size() > chunk_size_) {
        std::cerr << "Line exceeds chunk size, skipping: " << *line_opt << std::endl;
        continue; // Skip lines that are too large
      }
      auto hour = sp::MktData::GetHourFromTimestamp(*line_opt);
      if (prev_hour_ == 0) [[unlikely]] {
        prev_hour_ = hour; // Initialize prev_hour_ on first line
      }

      if (hour != prev_hour_) [[unlikely]] {
        queue_.ProducerDone(); // Notify consumer of hour change
        prev_hour_ = hour;
        std::cout << "Hour change:" << hour  << " waiting until prev hour:"
          << prev_hour_ << " is finished for all other symbols"
          << ", thread_id:" << thread_id_ <<  std::endl;
        queue_.WaitUntilDoneFileReset();
        std::cout << "Resuming thread " << thread_id_ << " for file: "
                  << filename_ << " with symbol: " << symbol_
                  << " after hour change to: " << hour << std::endl;
      }

      queue_.Enqueue( {symbol_, line_opt.value(), hour}); // or whatever your queue method is
    }
  }

  void Stop() { stop_flag_ = true; }

  static size_t GetDefaultChunkSize() {
    const size_t max_mem = sp::GetMaxMemoryPerThread();
    return (max_mem > 1024 * 1024) ? (max_mem - 1024 * 1024) : max_mem;
  }

private:
  std::string filename_;
  std::string_view symbol_;
  QueueType& queue_;
  size_t chunk_size_;
  std::atomic<bool> stop_flag_;
  sp::MMF mmf_;
  thread_local size_t thread_id_ = thread_count_++; // Unique ID for each thread
};
} // namespace sp