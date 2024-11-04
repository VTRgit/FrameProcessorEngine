#include "real_time_executor.hpp"

#include <chrono>
#include <iostream>
#include <pthread.h>

namespace fpe {
namespace rte {

RealTimeExecutor::RealTimeExecutor() {
  // Create a real-time task using std::thread
  std::thread task_thread(&RealTimeExecutor::realTimeTask, this);

  // Set high real-time priority (requires root privileges)
  setThreadPriority(task_thread, 90);

  // Ensure main does not terminate
  task_thread.join();
}

RealTimeExecutor::~RealTimeExecutor() {}

// Helper function to set real-time priority for a std::thread
void RealTimeExecutor::setThreadPriority(std::thread &thread, int priority) {
  // Get native handle for std::thread
  pthread_t native_handle = thread.native_handle();

  // Set real-time scheduling policy to FIFO and set the priority
  sched_param param;
  param.sched_priority = priority;
  if (pthread_setschedparam(native_handle, SCHED_FIFO, &param) != 0) {
    std::cerr << "Failed to set thread priority." << std::endl;
  }
}

void RealTimeExecutor::realTimeTask() {
  auto next_execution_time = std::chrono::steady_clock::now();

  while (true) {
    // Simulate task workload
    std::cout << "[Task] Executing periodic task..." << std::endl;

    // Advance the next execution time by the task interval
    next_execution_time += std::chrono::milliseconds(task_interval_ms);

    // Sleep until the next execution time (sleep for remaining time in
    // interval)
    std::this_thread::sleep_until(next_execution_time);
  }
}

} // namespace rte
} // namespace fpe
