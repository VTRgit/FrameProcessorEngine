#include <fpe/frame_processor_engine.hpp>

using namespace fpe;

static RingBuffer<int> ringBuffers[5];
// Creating a global instance of FrameProcessor
static FrameProcessorEngine frameProcessorEngine(ringBuffers);

// Function to start the frame processor
extern "C" fpe::Response startFPE() { return frameProcessorEngine.start(); }

// Function to stop the frame processor
extern "C" fpe::Response stopFPE() { return frameProcessorEngine.stop(); }
