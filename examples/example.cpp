#include <fpe/frame_processor_engine.hpp>

int main() {
  fpe::FrameProcessorEngine frameProcessorEngine;
  frameProcessorEngine.start();
  frameProcessorEngine.stop();

  return 0; // Return 0 to indicate successful execution
}