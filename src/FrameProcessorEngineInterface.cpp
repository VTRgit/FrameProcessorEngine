// FrameProcessorInterface.h
#include "FrameProcessorEngine.h"

using namespace FPE;

// Creating a global instance of FrameProcessor
static FrameProcessorEngine fpe;

// Function to start the frame processor
extern "C" FPE::Response startFPE() { return fpe.start(); }

// Function to stop the frame processor
extern "C" FPE::Response stopFPE() { return fpe.stop(); }
