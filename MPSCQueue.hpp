#ifndef MPSCQUEUE_HPP
#define MPSCQUEUE_HPP

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
namespace sp {
  template<typename T>
  class MPSCQueue {
  public:
    // Enqueue: called by multiple producers, never blocks
    void Enqueue(const T &value) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(value);
      }
      cv_.notify_one();
    }

    void Enqueue(T &&value) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(value));
      }
      cv_.notify_one();
    }

    void BulkEnqueue(const std::deque<T> &values) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &value: values) {
          queue_.push_back(value);
        }
      }
      cv_.notify_all();
    }

    void BulkEnqueue(std::deque<T> &&values) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &value: values) {
          queue_.push_back(std::move(value));
        }
      }
      cv_.notify_all();
    }

    // Dequeue: called by a single consumer, blocks if empty
    T Dequeue() {
      // Optimization
      if (!cache_.empty()) {
        T value = std::move(cache_.front());
        cache_.pop_front();
        return value;
      }

      // If cache is empty, wait for new items to be enqueued
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !queue_.empty(); });
      T value = std::move(queue_.front());
      queue_.pop_front();
      if (!queue_.empty()) {
        // Move remaining items to cache for next dequeue
        // This avoids locking the mutex again if the queue is not empty
        cache_.swap(queue_);
      }
      return value;
    }

    // TryDequeue: non-blocking, returns std::nullopt if empty
    std::optional<T> TryDequeue() {
      // Optimization
      if (!cache_.empty()) {
        T value = std::move(cache_.front());
        cache_.pop_front();
        return value;
      }
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty())
        return std::nullopt;
      T value = std::move(queue_.front());
      queue_.pop_front();
      if (!queue_.empty()) {
        // Move remaining items to cache for next dequeue
        // This avoids locking the mutex again if the queue is not empty
        cache_.swap(queue_);
      }
      return value;
    }

    // Returns true if the queue is empty
    bool Empty() const {
      if (!cache_.empty())
        return false;
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.empty();
    }

    void ProducerDone() {
      ++done_file_count_;
      cv_.notify_all(); // Notify consumer that a producer is done
    }

    size_t GetDoneFileCount() const { return done_file_count_.load(); }
    bool IsDone() const {
      return done_file_count_.load() >= total_files_;
    }

    void ResetDoneFileCount() {
      done_file_count_.store(0);
      cv_.notify_all(); // Notify consumer that the count has been reset
    }

    void WaitUntilDoneFileReset() {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return done_file_count_.load() == 0; });
    }

  private:
    std::deque<T> queue_;
    std::deque<T> cache_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic_size_t done_file_count_;
    constexpr size_t total_files_ =
        10000;
  };
} // namespace sp

#endif // MPSCQUEUE_HPP
