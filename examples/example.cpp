#include <fpe/frame_processor_engine.hpp>

int main() {
  fpe::RingBuffer<int> ringBuffers[5];

  fpe::FrameProcessorEngine frameProcessorEngine(ringBuffers);
  frameProcessorEngine.start();
  frameProcessorEngine.stop();

  return 0; // Return 0 to indicate successful execution
}