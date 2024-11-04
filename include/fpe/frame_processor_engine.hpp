#pragma once

#include <fpe/ring_buffer.hpp>
#include <span>

namespace fpe {

enum class Response {
  noError,         ///< No error occurred.
  allocationError, ///< Memory allocation failed.
  bufferEmpty      ///< The buffer is empty.
};

class FrameProcessorEngine {
public:
  template <typename T, std::size_t Size>
  FrameProcessorEngine(std::span<const RingBuffer<T, Size>> inputBuffers);

  ~FrameProcessorEngine();

  Response start();
  Response stop();
};

template <typename T, std::size_t Size>
FrameProcessorEngine::FrameProcessorEngine(
    std::span<const RingBuffer<T, Size>> inputBuffers) {}
} // namespace fpe
