#pragma once

#include <thread>

namespace fpe {
namespace rte {

class RealTimeExecutor {
private:
  // Periodic task interval in milliseconds
  static constexpr int task_interval_ms = 500;

public:
  RealTimeExecutor(/* args */);
  ~RealTimeExecutor();

  void setThreadPriority(std::thread &thread, int priority);
  void realTimeTask();
};

} // namespace rte
} // namespace fpe
