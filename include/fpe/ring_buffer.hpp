#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>

namespace fpe {

template <typename T, std::size_t Size> class RingBuffer {
public:
  // Default constructor to initialize the buffer
  RingBuffer();

  // Adds an item to the buffer, overwrites the oldest item if the buffer is
  // full
  bool push(const T &item);

  // Retrieves and removes an item from the buffer, returns false if the buffer
  // is empty
  bool pop(T &item);

  // Returns the current number of elements in the buffer
  std::size_t size() const;

  // Returns the total capacity of the buffer
  std::size_t length() const;

private:
  std::array<T, Size> buffer;    // The actual buffer to store items
  std::atomic<std::size_t> head; // Index of the front element (oldest)
  std::atomic<std::size_t> tail; // Index of the next write position
  static constexpr std::size_t capacity = Size; // Total capacity of the buffer
};

// Implementation of RingBuffer methods

template <typename T, std::size_t Size>
RingBuffer<T, Size>::RingBuffer() : head(0), tail(0) {}

// Pushes an item to the buffer, overwriting the oldest element if full
template <typename T, std::size_t Size>
bool RingBuffer<T, Size>::push(const T &item) {
  std::size_t current_tail = tail.load(std::memory_order_relaxed);
  std::size_t next_tail = (current_tail + 1) % capacity;

  // If the buffer is full, overwrite the oldest element by advancing the head
  if (next_tail == head.load(std::memory_order_acquire)) {
    // Only advance the head if we overwrite
    head.store((head.load(std::memory_order_relaxed) + 1) % capacity,
               std::memory_order_release);
  }

  buffer[current_tail] = item;                      // Store the new item
  tail.store(next_tail, std::memory_order_release); // Update tail
  return true;
}

// Pops an item from the buffer, returning false if the buffer is empty
template <typename T, std::size_t Size> bool RingBuffer<T, Size>::pop(T &item) {
  std::size_t current_head = head.load(std::memory_order_relaxed);

  // Check if the buffer is empty
  if (current_head == tail.load(std::memory_order_acquire)) {
    return false; // Buffer is empty
  }

  item = buffer[current_head];
  head.store((current_head + 1) % capacity, std::memory_order_release);
  return true;
}

// Returns the current number of elements in the buffer
template <typename T, std::size_t Size>
std::size_t RingBuffer<T, Size>::size() const {
  std::size_t current_head = head.load(std::memory_order_relaxed);
  std::size_t current_tail = tail.load(std::memory_order_relaxed);

  if (current_tail >= current_head) {
    return current_tail - current_head;
  } else {
    return capacity - (current_head - current_tail);
  }
}

// Returns the total capacity of the buffer
template <typename T, std::size_t Size>
std::size_t RingBuffer<T, Size>::length() const {
  return capacity;
}

} // namespace fpe
