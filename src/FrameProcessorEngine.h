#pragma once

#include "FrameProcessorEngineInterface.h"

namespace fpe {

class FrameProcessorEngine {
private:
  /* data */
public:
  FrameProcessorEngine(/* args */);
  ~FrameProcessorEngine();

  Response start();
  Response stop();
};

} // namespace fpe
