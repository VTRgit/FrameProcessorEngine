#include <fpe/frame_processor_engine.hpp>
#include <spdlog/spdlog.h>

namespace fpe {

FrameProcessorEngine::FrameProcessorEngine(
    std::span<const RingBuffer<int>> inputBuffers) {
  spdlog::info("\nInitializing FrameProcessorEngine \n\tinputBuffers: "
               "{}\n\tbuffersSize: {}\n",
               inputBuffers.size(), inputBuffers[0].length());
}

FrameProcessorEngine::~FrameProcessorEngine() {}

Response FrameProcessorEngine::start() { return Response::noError; }

Response FrameProcessorEngine::stop() { return Response::noError; }

} // namespace fpe
