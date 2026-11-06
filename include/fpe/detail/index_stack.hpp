#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

namespace fpe::detail {

/// Lock-free stack of indices in [0, capacity), for free lists of preallocated slots.
/// The head packs a 32-bit ABA tag and (index + 1); 0 means empty. Storage is allocated once in
/// the constructor; push and pop never allocate.
class IndexStack {
public:
  static constexpr std::uint32_t npos = 0xFFFFFFFFu;

  explicit IndexStack(std::uint32_t capacity)
      : capacity_(capacity), next_(std::make_unique<std::atomic<std::uint32_t>[]>(capacity)) {}

  IndexStack(const IndexStack&) = delete;
  IndexStack& operator=(const IndexStack&) = delete;

  std::uint32_t capacity() const noexcept { return capacity_; }

  void push(std::uint32_t index) noexcept {
    std::uint64_t old = head_.load(std::memory_order_acquire);
    for (;;) {
      next_[index].store(low(old), std::memory_order_relaxed);
      std::uint64_t desired = pack(tag(old) + 1, index + 1);
      if (head_.compare_exchange_weak(old, desired, std::memory_order_release,
                                      std::memory_order_acquire)) {
        return;
      }
    }
  }

  /// Returns npos when empty.
  std::uint32_t pop() noexcept {
    std::uint64_t old = head_.load(std::memory_order_acquire);
    for (;;) {
      std::uint32_t top = low(old);
      if (top == 0) {
        return npos;
      }
      std::uint32_t next = next_[top - 1].load(std::memory_order_relaxed);
      std::uint64_t desired = pack(tag(old) + 1, next);
      if (head_.compare_exchange_weak(old, desired, std::memory_order_acq_rel,
                                      std::memory_order_acquire)) {
        return top - 1;
      }
    }
  }

private:
  static constexpr std::uint32_t low(std::uint64_t v) noexcept {
    return static_cast<std::uint32_t>(v & 0xFFFFFFFFu);
  }
  static constexpr std::uint32_t tag(std::uint64_t v) noexcept {
    return static_cast<std::uint32_t>(v >> 32);
  }
  static constexpr std::uint64_t pack(std::uint32_t t, std::uint32_t l) noexcept {
    return (static_cast<std::uint64_t>(t) << 32) | l;
  }

  std::uint32_t capacity_;
  std::unique_ptr<std::atomic<std::uint32_t>[]> next_;
  std::atomic<std::uint64_t> head_{0};
};

} // namespace fpe::detail
