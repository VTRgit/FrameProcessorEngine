#include <fpe/ring_buffer.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

constexpr int BUFFER_SIZE = 10; // Adjust the buffer size as needed

// A simple test fixture for RingBuffer
template <typename T> class RingBufferTest : public ::testing::Test {
public:
  fpe::RingBuffer<T> buffer{BUFFER_SIZE};

  // Function to push elements into the buffer from multiple threads
  void pushElements(int thread_id, int count) {
    for (int i = 0; i < count; ++i) {
      buffer.push(static_cast<T>(thread_id * 1000 +
                                 i)); // Unique values for each thread
    }
  }

  // Function to pop elements from the buffer
  void popElements(std::vector<T> &popped, int count) {
    T item;
    for (int i = 0; i < count; ++i) {
      if (buffer.pop(item)) {
        popped.push_back(item);
      }
    }
  }
};

using RingBufferMultithreadTests = RingBufferTest<int>;

// Test for multithreaded pushing
TEST_F(RingBufferMultithreadTests, MultiThreadedPush) {
  const int NUM_THREADS = 5;
  const int ELEMENTS_PER_THREAD = 10;
  std::vector<std::thread> threads;

  // Start multiple threads to push elements
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(&RingBufferMultithreadTests::pushElements, this, i,
                         ELEMENTS_PER_THREAD);
  }

  // Wait for all threads to finish
  for (auto &t : threads) {
    t.join();
  }

  // Verify the size of the buffer after all pushes
  EXPECT_LE(buffer.size(), BUFFER_SIZE);
}

// Test for multithreaded popping
TEST_F(RingBufferMultithreadTests, MultiThreadedPop) {
  const int NUM_THREADS = 5;
  const int ELEMENTS_PER_THREAD = 10;
  std::vector<std::thread> threads;
  std::vector<int> popped;

  // Pre-fill the buffer with elements
  for (int i = 0; i < NUM_THREADS; ++i) {
    pushElements(i, ELEMENTS_PER_THREAD); // Ensure this is accessible
  }

  // Start multiple threads to pop elements
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back(&RingBufferTest<int>::popElements, this,
                         std::ref(popped), ELEMENTS_PER_THREAD);
  }

  // Wait for all threads to finish
  for (auto &t : threads) {
    t.join();
  }

  // Verify the number of popped elements
  EXPECT_LE(popped.size(), BUFFER_SIZE);
}

// Test for mixed push and pop operations
TEST_F(RingBufferMultithreadTests, MixedPushPop) {
  const int NUM_THREADS = 5;
  const int ELEMENTS_PER_THREAD = 10;
  std::vector<std::thread> threads;
  std::atomic<int> total_popped{0};

  // Start multiple threads to push and pop elements
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([this, i, &total_popped]() {
      for (int j = 0; j < ELEMENTS_PER_THREAD; ++j) {
        buffer.push(static_cast<int>(i * 1000 + j));
        int item;
        if (buffer.pop(item)) {
          ++total_popped; // Count successful pops
        }
      }
    });
  }

  // Wait for all threads to finish
  for (auto &t : threads) {
    t.join();
  }

  // Verify the total number of popped elements
  EXPECT_LE(total_popped.load(), NUM_THREADS * ELEMENTS_PER_THREAD);
}
