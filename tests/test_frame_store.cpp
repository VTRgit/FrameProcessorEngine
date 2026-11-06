#include "support/alloc_guard.h"

#include <fpe/frame.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

using namespace fpe;

namespace {

struct ReleaseCounter {
  std::atomic<int> calls{0};
  static void onRelease(void* ctx) noexcept {
    static_cast<ReleaseCounter*>(ctx)->calls.fetch_add(1, std::memory_order_relaxed);
  }
};

Planes externalPlanes(std::uint8_t* data, std::uint32_t stride) {
  Planes p;
  p.data[0] = data;
  p.stride[0] = stride;
  p.count = 1;
  return p;
}

} // namespace

TEST(FrameHandle, IsEightBytesAndTriviallyCopyable) {
  static_assert(sizeof(FrameHandle) == 8);
  static_assert(std::is_trivially_copyable_v<FrameHandle>);
  EXPECT_FALSE(FrameHandle{}.valid());
}

TEST(FrameStore, PoolSlotsHaveAlignedMemory) {
  FrameStore store({.poolSlots = 3, .poolSlotBytes = 1000, .externalSlots = 0, .alignment = 64});
  FrameHandle a, b;
  ASSERT_EQ(store.acquire(a), Status::Ok);
  ASSERT_EQ(store.acquire(b), Status::Ok);
  auto* pa = store.poolMemory(a);
  auto* pb = store.poolMemory(b);
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pa) % 64, 0u);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pb) % 64, 0u);
  EXPECT_GE(static_cast<std::size_t>(pb > pa ? pb - pa : pa - pb), 1000u);
  EXPECT_EQ(store.planes(a)->data[0], pa);
  store.release(a);
  store.release(b);
}

TEST(FrameStore, ExhaustedPoolReturnsImmediately) {
  FrameStore store({.poolSlots = 2, .poolSlotBytes = 16, .externalSlots = 0});
  FrameHandle a, b, c;
  ASSERT_EQ(store.acquire(a), Status::Ok);
  ASSERT_EQ(store.acquire(b), Status::Ok);
  EXPECT_EQ(store.acquire(c), Status::PoolExhausted);
  EXPECT_FALSE(c.valid());
  EXPECT_EQ(store.inUse(), 2u);
  EXPECT_EQ(store.release(a), Status::Ok);
  EXPECT_EQ(store.acquire(c), Status::Ok);
  store.release(b);
  store.release(c);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(FrameStore, ExternalFrameReleasedExactlyOnceWithItsContext) {
  FrameStore store({.poolSlots = 0, .externalSlots = 2});
  std::uint8_t pixels[64]{};
  ReleaseCounter counter;
  FrameInfo info;
  info.width = 8;
  info.height = 8;
  info.format = PixelFormat::Gray8;
  FrameHandle h;
  ASSERT_EQ(store.wrapExternal(info, externalPlanes(pixels, 8), &ReleaseCounter::onRelease,
                               &counter, h),
            Status::Ok);
  EXPECT_EQ(store.info(h)->width, 8u);
  EXPECT_EQ(store.planes(h)->data[0], pixels);
  EXPECT_EQ(store.poolMemory(h), nullptr);

  ASSERT_EQ(store.retain(h), Status::Ok);
  ASSERT_EQ(store.retain(h), Status::Ok); // three owners
  store.release(h);
  store.release(h);
  EXPECT_EQ(counter.calls.load(), 0);
  store.release(h);
  EXPECT_EQ(counter.calls.load(), 1);
}

TEST(FrameStore, ExternalWrapFailureDoesNotCallRelease) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  std::uint8_t pixels[4]{};
  ReleaseCounter counter;
  FrameHandle a, b;
  ASSERT_EQ(store.wrapExternal({}, externalPlanes(pixels, 4), &ReleaseCounter::onRelease, &counter, a),
            Status::Ok);
  EXPECT_EQ(store.wrapExternal({}, externalPlanes(pixels, 4), &ReleaseCounter::onRelease, &counter, b),
            Status::PoolExhausted);
  EXPECT_EQ(counter.calls.load(), 0);
  store.release(a);
  EXPECT_EQ(counter.calls.load(), 1);
}

class FrameStoreOwners : public ::testing::TestWithParam<int> {};

TEST_P(FrameStoreOwners, ReleasedExactlyOnce) {
  const int owners = GetParam();
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  std::uint8_t pixels[4]{};
  ReleaseCounter counter;
  FrameHandle h;
  ASSERT_EQ(store.wrapExternal({}, externalPlanes(pixels, 4), &ReleaseCounter::onRelease, &counter, h),
            Status::Ok);
  std::vector<FrameRef> refs;
  refs.emplace_back(store, h);
  for (int i = 1; i < owners; ++i) {
    refs.push_back(refs.front().share());
    ASSERT_TRUE(refs.back());
  }
  while (!refs.empty()) {
    EXPECT_EQ(counter.calls.load(), 0);
    refs.pop_back();
  }
  EXPECT_EQ(counter.calls.load(), 1);
}

INSTANTIATE_TEST_SUITE_P(Owners, FrameStoreOwners, ::testing::Values(1, 2, 16));

TEST(FrameStore, StaleHandleIsDetected) {
  FrameStore store({.poolSlots = 1, .poolSlotBytes = 16, .externalSlots = 0});
  FrameHandle first;
  ASSERT_EQ(store.acquire(first), Status::Ok);
  store.release(first);
  FrameHandle second;
  ASSERT_EQ(store.acquire(second), Status::Ok); // same slot, new generation
  EXPECT_EQ(second.index, first.index);
  EXPECT_NE(second.generation, first.generation);
  EXPECT_FALSE(store.isLive(first));
  EXPECT_TRUE(store.isLive(second));
#if defined(NDEBUG)
  EXPECT_EQ(store.retain(first), Status::StaleHandle);
  EXPECT_EQ(store.release(first), Status::StaleHandle);
  EXPECT_EQ(store.info(first), nullptr);
#else
  EXPECT_DEATH(store.retain(first), "stale frame handle");
#endif
  store.release(second);
}

TEST(FrameRef, MoveAndDetach) {
  FrameStore store({.poolSlots = 1, .poolSlotBytes = 16, .externalSlots = 0});
  FrameHandle h;
  ASSERT_EQ(store.acquire(h), Status::Ok);
  FrameRef a(store, h);
  FrameRef b(std::move(a));
  EXPECT_FALSE(a);
  EXPECT_TRUE(b);
  EXPECT_EQ(store.inUse(), 1u);
  FrameHandle raw = b.detach();
  EXPECT_EQ(store.inUse(), 1u);
  store.release(raw);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(FrameStore, NoAllocationsOnAcquireRetainRelease) {
  FrameStore store({.poolSlots = 4, .poolSlotBytes = 256, .externalSlots = 4});
  std::uint8_t pixels[16]{};
  ReleaseCounter counter;
  EXPECT_NO_ALLOCATIONS({
    for (int i = 0; i < 1000; ++i) {
      FrameHandle p, e;
      store.acquire(p);
      store.wrapExternal({}, externalPlanes(pixels, 4), &ReleaseCounter::onRelease, &counter, e);
      store.retain(p);
      store.retain(e);
      store.release(p);
      store.release(e);
      store.release(p);
      store.release(e);
    }
  });
  EXPECT_EQ(counter.calls.load(), 1000);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(FrameStore, ConcurrentStressReleasesEverySlotExactlyOnce) {
  constexpr int kThreads = 4;
  constexpr int kOpsPerThread = 250'000; // one million in total
  FrameStore store({.poolSlots = 6, .poolSlotBytes = 64, .externalSlots = 6});
  ReleaseCounter counter;
  std::atomic<long> externalWrapped{0};
  std::uint8_t pixels[16]{};

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < kOpsPerThread; ++i) {
        FrameHandle h;
        const bool external = ((i + t) & 1) != 0;
        Status s = external ? store.wrapExternal({}, externalPlanes(pixels, 4),
                                                 &ReleaseCounter::onRelease, &counter, h)
                            : store.acquire(h);
        if (s != Status::Ok) {
          continue; // exhausted: another thread holds the slots
        }
        if (external) {
          externalWrapped.fetch_add(1, std::memory_order_relaxed);
        } else {
          store.poolMemory(h)[0] = static_cast<std::uint8_t>(i); // touch owned memory
        }
        const int extra = i % 3;
        for (int k = 0; k < extra; ++k) {
          store.retain(h);
        }
        for (int k = 0; k <= extra; ++k) {
          store.release(h);
        }
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  EXPECT_EQ(store.inUse(), 0u);
  EXPECT_EQ(counter.calls.load(), externalWrapped.load());
  // Every slot is free again.
  std::vector<FrameHandle> all;
  FrameHandle h;
  while (store.acquire(h) == Status::Ok) {
    all.push_back(h);
  }
  EXPECT_EQ(all.size(), 6u);
  for (auto x : all) {
    store.release(x);
  }
}
