#include <fpe/ring_buffer.hpp>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace fpe;

TEST(RingBufferMultithreadTest, SingleProducerSingleConsumer) {
  constexpr std::size_t buffer_size = 10;
  constexpr int num_elements = 100;
  RingBuffer<int, buffer_size> buffer;

  std::vector<int> produced_elements;
  std::vector<int> consumed_elements;

  // Create a producer thread that pushes elements into the buffer
  std::thread producer([&]() {
    for (int i = 0; i < num_elements; ++i) {
      buffer.push(i);
      produced_elements.push_back(i);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });

  // Create a consumer thread that pops elements from the buffer
  std::thread consumer([&]() {
    int popped_count = 0;
    while (popped_count < num_elements) {
      auto item = buffer.pop();
      if (item.has_value()) {
        consumed_elements.push_back(item.value());
        ++popped_count;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    }
  });

  // Wait for both threads to finish
  producer.join();
  consumer.join();

  // Check that all elements produced were consumed correctly
  ASSERT_EQ(produced_elements.size(), consumed_elements.size());
  for (size_t i = 0; i < produced_elements.size(); ++i) {
    EXPECT_EQ(produced_elements[i], consumed_elements[i]);
  }
}
