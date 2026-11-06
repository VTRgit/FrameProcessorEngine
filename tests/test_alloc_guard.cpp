#include "support/alloc_guard.h"

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

namespace {

// Keeps the compiler from eliding new/delete pairs (allowed since C++14).
template <class T> void escape(T* p) { asm volatile("" : : "g"(p) : "memory"); }

} // namespace

TEST(AllocGuard, CatchesAnAllocation) {
  fpe::test::AllocGuard guard;
  auto* p = new int(42);
  escape(p);
  delete p;
  EXPECT_EQ(guard.count(), 2); // one allocation, one free
}

TEST(AllocGuard, IgnoresAllocationsOutsideTheScope) {
  auto before = std::make_unique<std::vector<int>>(1000);
  escape(before.get());
  {
    fpe::test::AllocGuard guard;
    int x = 0;
    escape(&x);
    EXPECT_EQ(guard.count(), 0);
  }
  before.reset();
}

TEST(AllocGuard, NoAllocationsMacroPassesOnPlainCode) {
  int sum = 0;
  EXPECT_NO_ALLOCATIONS({
    for (int i = 0; i < 100; ++i) {
      sum += i;
    }
  });
  EXPECT_EQ(sum, 4950);
}

TEST(AllocGuard, CatchesAnAllocationOnAnotherArmedThread) {
  fpe::test::AllocGuard guard;
  std::int64_t beforeThread = guard.count();
  std::int64_t onThread = 0;
  std::thread t([&] {
    std::int64_t start = fpe::test::allocationCount();
    fpe::test::armCurrentThread();
    auto* p = new double(1.0);
    escape(p);
    delete p;
    fpe::test::disarmCurrentThread();
    onThread = fpe::test::allocationCount() - start;
  });
  t.join();
  EXPECT_GE(onThread, 2);
  EXPECT_GE(guard.count() - beforeThread, 2);
}

TEST(AllocGuard, UnarmedThreadsAreNotCounted) {
  std::int64_t start = fpe::test::allocationCount();
  std::thread t([] {
    auto* p = new double(1.0);
    escape(p);
    delete p;
  });
  t.join();
  // Thread creation itself happened on this (unarmed) thread.
  EXPECT_EQ(fpe::test::allocationCount(), start);
}
