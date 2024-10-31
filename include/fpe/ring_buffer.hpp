#pragma once

#include <array>
#include <atomic>
#include <optional>

namespace fpe {

template <typename T, std::size_t Capacity> class RingBuffer {
public:
  // Push an item into the buffer, overwriting the oldest if full
  void push(const T &item) {
    auto current_head = head_.load(std::memory_order_relaxed);
    if (current_head == tail_.load(std::memory_order_acquire) &&
        is_full_.load(std::memory_order_acquire)) {
      // Buffer full, advance the tail to overwrite
      tail_.store(increment(tail_.load(std::memory_order_relaxed)),
                  std::memory_order_release);
    }

    auto next_head = increment(current_head);

    buffer_[current_head] = item;

    // Move the head, overwrite if needed
    head_.store(next_head, std::memory_order_release);

    if (next_head == tail_.load(std::memory_order_acquire)) {
      // Buffer full, advance the tail to overwrite
      is_full_.store(true, std::memory_order_release);
    }
  }

  // Pop an item from the buffer, returns std::nullopt if empty
  std::optional<T> pop() {
    auto current_tail = tail_.load(std::memory_order_acquire);

    if (current_tail == head_.load(std::memory_order_acquire) &&
        !is_full_.load(std::memory_order_acquire)) {
      // Buffer empty
      return std::nullopt;
    }

    T item = buffer_[current_tail];
    tail_.store(increment(current_tail), std::memory_order_release);
    is_full_.store(false, std::memory_order_release);
    return item;
  }

  // Current size of the buffer (number of elements stored)
  std::size_t size() const {
    auto current_head = head_.load(std::memory_order_acquire);
    auto current_tail = tail_.load(std::memory_order_acquire);

    std::size_t size;
    if (current_head >= current_tail) {
      size = (is_full_.load(std::memory_order_acquire))
                 ? Capacity
                 : current_head - current_tail;
    } else {
      size = Capacity - current_tail + current_head;
    }

    return size;
  }

  // Total capacity of the buffer
  std::size_t length() const { return Capacity; }

private:
  std::array<T, Capacity> buffer_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  std::atomic<bool> is_full_{false};

  // Utility to wrap around the buffer's indices
  std::size_t increment(std::size_t idx) const { return (idx + 1) % Capacity; }
};

} // namespace fpe
