#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace fpe::detail {

/// Bounded multi-producer / multi-consumer queue (Dmitry Vyukov's design). Capacity is rounded up
/// to a power of two and allocated once; push and pop never allocate and never block.
template <class T> class MpmcQueue {
  static_assert(std::is_trivially_copyable_v<T>);

public:
  explicit MpmcQueue(std::size_t capacity) {
    std::size_t size = 2;
    while (size < capacity) {
      size <<= 1;
    }
    mask_ = size - 1;
    cells_ = std::make_unique<Cell[]>(size);
    for (std::size_t i = 0; i < size; ++i) {
      cells_[i].sequence.store(i, std::memory_order_relaxed);
    }
  }

  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  std::size_t capacity() const noexcept { return mask_ + 1; }

  /// False when full.
  bool push(const T& value) noexcept {
    Cell* cell = nullptr;
    std::size_t pos = enqueue_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &cells_[pos & mask_];
      const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
      const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
      if (diff == 0) {
        if (enqueue_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        return false;
      } else {
        pos = enqueue_.load(std::memory_order_relaxed);
      }
    }
    cell->value = value;
    cell->sequence.store(pos + 1, std::memory_order_release);
    return true;
  }

  /// False when empty.
  bool pop(T& out) noexcept {
    Cell* cell = nullptr;
    std::size_t pos = dequeue_.load(std::memory_order_relaxed);
    for (;;) {
      cell = &cells_[pos & mask_];
      const std::size_t seq = cell->sequence.load(std::memory_order_acquire);
      const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos + 1);
      if (diff == 0) {
        if (dequeue_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        return false;
      } else {
        pos = dequeue_.load(std::memory_order_relaxed);
      }
    }
    out = cell->value;
    cell->sequence.store(pos + mask_ + 1, std::memory_order_release);
    return true;
  }

private:
  struct Cell {
    std::atomic<std::size_t> sequence{0};
    T value{};
  };

  std::unique_ptr<Cell[]> cells_;
  std::size_t mask_ = 0;
  alignas(64) std::atomic<std::size_t> enqueue_{0};
  alignas(64) std::atomic<std::size_t> dequeue_{0};
};

} // namespace fpe::detail
