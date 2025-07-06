#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "../MPSCQueue.hpp" // Adjust path as needed

using namespace sp;
TEST(MPSCQueueTest, SingleProducerSingleConsumer) {
    MPSCQueue<int> queue;
    queue.Enqueue(42);
    auto val = queue.Dequeue();
    EXPECT_EQ(val, 42);
}

TEST(MPSCQueueTest, SingleProducerSingleConsumerTry) {
  MPSCQueue<int> queue;
  queue.Enqueue(42);
  auto val = queue.TryDequeue();
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(val.value(), 42);
}

TEST(MPSCQueueTest, MultipleProducersSingleConsumer) {
    MPSCQueue<int> queue;
    constexpr int num_producers = 4;
    constexpr int items_per_producer = 100;
    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&queue, i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                queue.Enqueue(i * items_per_producer + j);
            }
        });
    }
    std::vector<int> results;
    std::thread consumer([&queue, &results]() {
        for (int i = 0; i < num_producers * items_per_producer; ++i) {
            auto val = queue.TryDequeue();
            ASSERT_TRUE(val.has_value());
            results.push_back(val.value());
        }
    });
    for (auto& t : producers) t.join();
    consumer.join();
    EXPECT_EQ(results.size(), num_producers * items_per_producer);
}

TEST(MPSCQueueTest, ConsumerBlocksOnEmptyQueue) {
    MPSCQueue<int> queue;
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};
    std::thread consumer([&]() {
        started = true;
        auto val = queue.Dequeue();
        finished = true;
        EXPECT_EQ(val, 123);
    });
    while (!started) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(finished);
    queue.Enqueue(123);
    consumer.join();
    EXPECT_TRUE(finished);
}

TEST(MPSCQueueTest, EnqueueNeverBlocks) {
    MPSCQueue<int> queue;
    for (int i = 0; i < 1000; ++i) {
        queue.Enqueue(i);
    }
    for (int i = 0; i < 1000; ++i) {
        auto val = queue.TryDequeue();
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(val.value(), i);
    }
}