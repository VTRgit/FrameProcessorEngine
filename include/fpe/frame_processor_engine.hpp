#pragma once

#include <fpe/frame_processor_engine_interface.h>
#include <fpe/ring_buffer.hpp>
#include <span>

namespace fpe {

class FrameProcessorEngine {
private:
  /* data */
public:
  FrameProcessorEngine(std::span<const RingBuffer<int>> inputBuffers);
  ~FrameProcessorEngine();

  Response start();
  Response stop();
};

} // namespace fpe
