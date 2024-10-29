#pragma once

#include <fpe/frame_processor_engine_interface.h>
#include <fpe/ring_buffer.hpp>
#include <span>

namespace fpe {

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
