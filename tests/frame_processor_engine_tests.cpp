#include <fpe/frame_processor_engine.hpp>
#include <gtest/gtest.h>

using namespace fpe;

TEST(FrameProcessorEngineTests, StartStop) {
  using IntRingBuffer = fpe::RingBuffer<int, 10>;

  const std::array<IntRingBuffer, 3> ringBuffers;
  std::span<const IntRingBuffer> bufferSpan(ringBuffers);

  FrameProcessorEngine frameProcessorEngine(bufferSpan);

  ASSERT_EQ(frameProcessorEngine.start(), Response::noError);
  ASSERT_EQ(frameProcessorEngine.stop(), Response::noError);
}
