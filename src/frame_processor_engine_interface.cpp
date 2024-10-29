#include <fpe/frame_processor_engine.hpp>

using namespace fpe;

// Allocating buffers in the stack
static RingBuffer<int, 10> ringBuffers[5];
static std::span<const fpe::RingBuffer<int, 10>> bufferSpan(ringBuffers);

// Creating a global instance of FrameProcessor
static FrameProcessorEngine frameProcessorEngine(bufferSpan);

// Function to start the frame processor
extern "C" fpe::Response startFPE() { return frameProcessorEngine.start(); }

// Function to stop the frame processor
extern "C" fpe::Response stopFPE() { return frameProcessorEngine.stop(); }
