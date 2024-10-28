#pragma once

#include <fpe/frame_processor_engine_interface.h>

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
