#ifndef MPSCQUEUE_HPP
#define MPSCQUEUE_HPP

#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace sp {
  template<typename T>
  class MPSCQueue {
  public:
    // Enqueue: called by multiple producers, never blocks
    void Enqueue(const T& value) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(value);
      }
      cv_.notify_one();
    }

    void Enqueue(T&& value) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(value));
      }
      cv_.notify_one();
    }

    // Dequeue: called by a single consumer, blocks if empty
    T Dequeue() {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]{ return !queue_.empty(); });
      T value = std::move(queue_.front());
      queue_.pop_front();
      return value;
    }

    // TryDequeue: non-blocking, returns std::nullopt if empty
    std::optional<T> TryDequeue() {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) return std::nullopt;
      T value = std::move(queue_.front());
      queue_.pop_front();
      return value;
    }

    // Returns true if the queue is empty
    bool Empty() const {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.empty();
    }

  private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
  };
}// namespace sp

#endif // MPSCQUEUE_HPP