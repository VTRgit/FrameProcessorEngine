// FrameProcessorInterface.h
#include "FrameProcessorEngine.h"

using namespace fpe;

// Creating a global instance of FrameProcessor
static FrameProcessorEngine fpe;

// Function to start the frame processor
extern "C" fpe::Response startFPE() { return fpe.start(); }

// Function to stop the frame processor
extern "C" fpe::Response stopFPE() { return fpe.stop(); }
