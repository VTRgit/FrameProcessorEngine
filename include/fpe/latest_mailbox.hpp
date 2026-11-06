#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace fpe {

namespace detail {
// Fixed rather than std::hardware_destructive_interference_size: that value may differ between
// compilers and flags, and this header's layout must not.
inline constexpr std::size_t kCacheLine = 64;
} // namespace detail

/// Single-producer / single-consumer exchange where the consumer always gets the newest value.
///
/// Triple buffer: the producer owns one slot, the consumer owns one, and the third is swapped
/// through a single atomic byte that also carries a "fresh" bit. publish() and take() are
/// wait-free and never allocate.
///
/// When publish() replaces a value the consumer never took, that value is handed back to the
/// producer (return value) so it can release whatever it refers to.
///
/// The producer and consumer roles may move between threads only if each hand-over is ordered
/// by a happens-before relation (e.g. an acquire/release flag).
template <class T> class LatestMailbox {
  static_assert(std::is_trivially_copyable_v<T>, "LatestMailbox holds trivially copyable values");
  static_assert(sizeof(T) <= 16, "LatestMailbox holds handles, not payloads");

public:
  LatestMailbox() noexcept = default;
  LatestMailbox(const LatestMailbox&) = delete;
  LatestMailbox& operator=(const LatestMailbox&) = delete;

  /// Producer side. Returns the displaced value if the consumer never saw it.
  std::optional<T> publish(const T& value) noexcept {
    slots_[back_] = value;
    const std::uint8_t old = middle_.exchange(static_cast<std::uint8_t>(back_ | kFresh),
                                              std::memory_order_acq_rel);
    back_ = static_cast<std::uint8_t>(old & kIndexMask);
    published_.fetch_add(1, std::memory_order_relaxed);
    if ((old & kFresh) != 0) {
      replaced_.fetch_add(1, std::memory_order_relaxed);
      return slots_[back_];
    }
    return std::nullopt;
  }

  /// Consumer side. The newest value published since the last take, if any.
  std::optional<T> take() noexcept {
    if ((middle_.load(std::memory_order_relaxed) & kFresh) == 0) {
      return std::nullopt;
    }
    const std::uint8_t old = middle_.exchange(front_, std::memory_order_acq_rel);
    front_ = static_cast<std::uint8_t>(old & kIndexMask);
    taken_.fetch_add(1, std::memory_order_relaxed);
    return slots_[front_];
  }

  /// True if a value is waiting (any thread; approximate under concurrency).
  bool hasPending() const noexcept {
    return (middle_.load(std::memory_order_acquire) & kFresh) != 0;
  }

  std::uint64_t published() const noexcept { return published_.load(std::memory_order_relaxed); }
  std::uint64_t taken() const noexcept { return taken_.load(std::memory_order_relaxed); }
  std::uint64_t replaced() const noexcept { return replaced_.load(std::memory_order_relaxed); }

private:
  static constexpr std::uint8_t kIndexMask = 0x3;
  static constexpr std::uint8_t kFresh = 0x4;

  T slots_[3]{};
  alignas(detail::kCacheLine) std::atomic<std::uint8_t> middle_{1};
  alignas(detail::kCacheLine) std::uint8_t back_ = 0;  // producer
  alignas(detail::kCacheLine) std::uint8_t front_ = 2; // consumer
  alignas(detail::kCacheLine) std::atomic<std::uint64_t> published_{0};
  std::atomic<std::uint64_t> taken_{0};
  std::atomic<std::uint64_t> replaced_{0};
};

} // namespace fpe
