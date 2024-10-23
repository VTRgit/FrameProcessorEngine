#pragma once

#include "FrameProcessorEngineInterface.h"

namespace FPE {

class FrameProcessorEngine {
private:
  /* data */
public:
  FrameProcessorEngine(/* args */);
  ~FrameProcessorEngine();

  Response start();
  Response stop();
};

} // namespace FPE