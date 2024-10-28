#include <fpe/frame_processor_engine.hpp>
#include <spdlog/spdlog.h>

namespace fpe {

FrameProcessorEngine::FrameProcessorEngine(/* args */) {
  spdlog::info("initializing FrameProcessorEngine");
}

FrameProcessorEngine::~FrameProcessorEngine() {}

Response FrameProcessorEngine::start() { return Response::noError; }

Response FrameProcessorEngine::stop() { return Response::noError; }

} // namespace fpe
