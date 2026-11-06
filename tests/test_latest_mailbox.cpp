#include "support/alloc_guard.h"

#include <fpe/frame.hpp>
#include <fpe/latest_mailbox.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>

using namespace fpe;
using namespace std::chrono_literals;

TEST(LatestMailbox, EmptyTakeReturnsNothing) {
  LatestMailbox<std::uint64_t> box;
  EXPECT_FALSE(box.take().has_value());
  EXPECT_FALSE(box.hasPending());
}

TEST(LatestMailbox, PublishThenTake) {
  LatestMailbox<std::uint64_t> box;
  EXPECT_FALSE(box.publish(7).has_value());
  EXPECT_TRUE(box.hasPending());
  EXPECT_EQ(box.take(), 7u);
  EXPECT_FALSE(box.take().has_value());
}

TEST(LatestMailbox, NewestWinsAndDisplacedValuesComeBack) {
  LatestMailbox<std::uint64_t> box;
  EXPECT_FALSE(box.publish(1).has_value());
  EXPECT_EQ(box.publish(2), 1u); // 1 was never taken
  EXPECT_EQ(box.publish(3), 2u);
  EXPECT_EQ(box.take(), 3u);
  EXPECT_FALSE(box.take().has_value());
  EXPECT_FALSE(box.publish(4).has_value()); // nothing pending to displace
  EXPECT_EQ(box.take(), 4u);
  EXPECT_EQ(box.published(), 4u);
  EXPECT_EQ(box.taken(), 2u);
  EXPECT_EQ(box.replaced(), 2u);
}

TEST(LatestMailbox, NoAllocations) {
  LatestMailbox<FrameHandle> box;
  EXPECT_NO_ALLOCATIONS({
    for (std::uint32_t i = 0; i < 10'000; ++i) {
      box.publish(FrameHandle{i % 8, i});
      if (i % 3 == 0) {
        box.take();
      }
    }
  });
}

namespace {

void runMonotonicStress(std::chrono::nanoseconds producerPeriod, int count) {
  LatestMailbox<std::uint64_t> box;
  std::atomic<bool> done{false};
  std::uint64_t lastSeen = 0;
  bool monotonic = true;
  std::uint64_t takes = 0;

  std::thread consumer([&] {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sleepMs(5, 50);
    while (!done.load(std::memory_order_acquire) || box.hasPending()) {
      if (auto v = box.take()) {
        if (*v <= lastSeen) {
          monotonic = false;
        }
        lastSeen = *v;
        ++takes;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs(rng)) / 10);
    }
  });

  auto next = std::chrono::steady_clock::now();
  for (int i = 1; i <= count; ++i) {
    box.publish(static_cast<std::uint64_t>(i));
    if (producerPeriod.count() > 0) {
      next += producerPeriod;
      std::this_thread::sleep_until(next);
    }
  }
  done.store(true, std::memory_order_release);
  consumer.join();

  EXPECT_TRUE(monotonic);
  EXPECT_EQ(lastSeen, static_cast<std::uint64_t>(count)); // the final value is always delivered
  EXPECT_EQ(box.published(), box.taken() + box.replaced());
  EXPECT_EQ(box.taken(), takes);
}

} // namespace

TEST(LatestMailbox, MonotonicAt240Fps) { runMonotonicStress(std::chrono::microseconds(4167), 480); }

TEST(LatestMailbox, MonotonicUnthrottled) { runMonotonicStress(std::chrono::nanoseconds(0), 200'000); }

TEST(LatestMailbox, FramesReleasedExactlyOnceThroughTheMailbox) {
  FrameStore store({.poolSlots = 5, .poolSlotBytes = 64, .externalSlots = 0});
  LatestMailbox<FrameHandle> box;
  std::atomic<bool> done{false};
  std::atomic<int> consumed{0};
  int produced = 0;
  int droppedAtSource = 0;

  std::thread consumer([&] {
    while (!done.load(std::memory_order_acquire) || box.hasPending()) {
      if (auto h = box.take()) {
        EXPECT_TRUE(store.isLive(*h));
        store.release(*h);
        consumed.fetch_add(1, std::memory_order_relaxed);
      }
      std::this_thread::sleep_for(200us);
    }
  });

  for (int i = 0; i < 20'000; ++i) {
    FrameHandle h;
    if (store.acquire(h) != Status::Ok) {
      ++droppedAtSource;
      std::this_thread::yield();
      continue;
    }
    store.mutableInfo(h)->frameId = static_cast<std::uint64_t>(i);
    ++produced;
    if (auto displaced = box.publish(h)) {
      store.release(*displaced);
    }
  }
  done.store(true, std::memory_order_release);
  consumer.join();

  EXPECT_EQ(store.inUse(), 0u);
  EXPECT_EQ(static_cast<std::uint64_t>(produced), box.published());
  EXPECT_EQ(static_cast<std::uint64_t>(consumed.load()), box.taken());
  EXPECT_GT(box.replaced(), 0u);
  (void)droppedAtSource;
}
