#pragma once

// Allocation guard for tests.
//
// The test executable replaces the global operator new/delete. While a thread is armed, every
// allocation and every free on that thread is counted. Engine worker threads can be armed from
// the engine's thread hook, so the guard covers the whole frame path, not just the test thread.

#include <cstdint>

namespace fpe::test {

/// Arms or disarms counting on the calling thread.
void armCurrentThread() noexcept;
void disarmCurrentThread() noexcept;

/// Allocations plus frees counted on armed threads since the last reset().
std::int64_t allocationCount() noexcept;
void resetAllocationCount() noexcept;

/// RAII: arms the current thread for its lifetime and remembers the count at construction.
class AllocGuard {
public:
  AllocGuard() noexcept;
  ~AllocGuard();
  AllocGuard(const AllocGuard&) = delete;
  AllocGuard& operator=(const AllocGuard&) = delete;

  /// Allocations plus frees counted (on any armed thread) since this guard was created.
  std::int64_t count() const noexcept;

private:
  std::int64_t start_;
  bool wasArmed_;
};

} // namespace fpe::test

/// Runs the block with the current thread armed and fails the test if anything was allocated or
/// freed on an armed thread meanwhile.
#define EXPECT_NO_ALLOCATIONS(...)                                                               \
  do {                                                                                             \
    std::int64_t fpeAllocs_ = 0;                                                                   \
    {                                                                                              \
      ::fpe::test::AllocGuard fpeGuard_;                                                           \
      __VA_ARGS__;                                                                                 \
      fpeAllocs_ = fpeGuard_.count();                                                              \
    }                                                                                              \
    EXPECT_EQ(fpeAllocs_, 0) << "heap allocations or frees on an armed thread";                    \
  } while (false)
