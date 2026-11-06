#include "support/alloc_guard.h"

#include <fpe/detail/mpmc_queue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using fpe::detail::MpmcQueue;

TEST(MpmcQueue, FifoAndBounds) {
  MpmcQueue<std::uint32_t> q(4);
  EXPECT_EQ(q.capacity(), 4u);
  std::uint32_t v = 0;
  EXPECT_FALSE(q.pop(v));
  for (std::uint32_t i = 0; i < 4; ++i) {
    EXPECT_TRUE(q.push(i));
  }
  EXPECT_FALSE(q.push(99)); // full
  for (std::uint32_t i = 0; i < 4; ++i) {
    ASSERT_TRUE(q.pop(v));
    EXPECT_EQ(v, i);
  }
  EXPECT_FALSE(q.pop(v));
}

TEST(MpmcQueue, CapacityRoundsUpToPowerOfTwo) {
  MpmcQueue<std::uint32_t> q(5);
  EXPECT_EQ(q.capacity(), 8u);
}

TEST(MpmcQueue, NoAllocations) {
  MpmcQueue<std::uint32_t> q(64);
  EXPECT_NO_ALLOCATIONS({
    std::uint32_t v = 0;
    for (std::uint32_t i = 0; i < 10'000; ++i) {
      q.push(i);
      q.pop(v);
    }
  });
}

TEST(MpmcQueue, ConcurrentProducersAndConsumersLoseNothing) {
  constexpr int kProducers = 3;
  constexpr int kConsumers = 3;
  constexpr std::uint32_t kPerProducer = 100'000;
  MpmcQueue<std::uint32_t> q(256);
  std::atomic<std::uint64_t> sum{0};
  std::atomic<std::uint32_t> popped{0};
  std::vector<std::thread> threads;
  for (int p = 0; p < kProducers; ++p) {
    threads.emplace_back([&] {
      for (std::uint32_t i = 1; i <= kPerProducer; ++i) {
        while (!q.push(i)) {
          std::this_thread::yield();
        }
      }
    });
  }
  for (int c = 0; c < kConsumers; ++c) {
    threads.emplace_back([&] {
      std::uint32_t v = 0;
      while (popped.load(std::memory_order_relaxed) < kProducers * kPerProducer) {
        if (q.pop(v)) {
          sum.fetch_add(v, std::memory_order_relaxed);
          popped.fetch_add(1, std::memory_order_relaxed);
        } else {
          std::this_thread::yield();
        }
      }
    });
  }
  for (auto& t : threads) {
    t.join();
  }
  const std::uint64_t expected =
      std::uint64_t{kProducers} * (std::uint64_t{kPerProducer} * (kPerProducer + 1) / 2);
  EXPECT_EQ(sum.load(), expected);
}
