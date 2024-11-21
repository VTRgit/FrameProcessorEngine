#pragma once

#include <any>
#include <fpe/response.hpp>
#include <fpe/ring_buffer.hpp>
#include <memory>
#include <tuple>
#include "../src/real_time_executor/real_time_executor.hpp"

namespace fpe {
class FrameProcessorEngine {
public:
  template <typename... Args>
  FrameProcessorEngine(const std::tuple<Args...> &ringBuffers);

  Response start();
  Response stop();
};

template <typename... Args>
FrameProcessorEngine::FrameProcessorEngine(
    const std::tuple<Args...> &ringBuffers) {
  fpe::rte::RealTimeExecutor realTimeExecutor(ringBuffers);
}

Response FrameProcessorEngine::start() { return Response::noError; }

Response FrameProcessorEngine::stop() { return Response::noError; }

} // namespace fpe
