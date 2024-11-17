#pragma once

#include <fpe/response.hpp>
#include <fpe/ring_buffer.hpp>
#include <iostream>

namespace fpe {
namespace rte {

template <typename... Args> class RealTimeExecutor {
public:
  RealTimeExecutor(const std::tuple<Args...> &ringBuffers);
  virtual ~RealTimeExecutor() = default;

  Response init();
  // void setThreadPriority(std::thread &thread, int priority);
  void realTimeTask();

private:
  void printInfo();

private:
  // Periodic task interval in milliseconds
  static constexpr int task_interval_ms = 500;

  const std::tuple<Args...> &ringBuffers;
};

template <typename... Args>
inline RealTimeExecutor<Args...>::RealTimeExecutor(
    const std::tuple<Args...> &ringBuffers)
    : ringBuffers(ringBuffers) {
  printInfo();
}

template <typename... Args> Response RealTimeExecutor<Args...>::init() {
  /* // Create a real-time task using std::thread
  std::thread task_thread(&RealTimeExecutor::realTimeTask, this);

  // Set high real-time priority (requires root privileges)
  setThreadPriority(task_thread, 90);

  // Ensure main does not terminate
  task_thread.join(); */

  return Response::noError;
}

// Helper function to set real-time priority for a std::thread
// template <typename... Args>
// void RealTimeExecutor<Args...>::setThreadPriority(std::thread &thread,
//                                                   int priority) {
//   // Get native handle for std::thread
//   /* pthread_t native_handle = thread.native_handle();

//   // Set real-time scheduling policy to FIFO and set the priority
//   sched_param param;
//   param.sched_priority = priority;
//   if (pthread_setschedparam(native_handle, SCHED_FIFO, &param) != 0) {
//     std::cerr << "Failed to set thread priority." << std::endl;
//   } */
// }

template <typename... Args> void RealTimeExecutor<Args...>::realTimeTask() {
  /* auto next_execution_time = std::chrono::steady_clock::now();

  while (true) {
    // Simulate task workload
    std::cout << "[Task] Executing periodic task..." << std::endl;

    // Advance the next execution time by the task interval
    next_execution_time += std::chrono::milliseconds(task_interval_ms);

    // Sleep until the next execution time (sleep for remaining time in
    // interval)
    std::this_thread::sleep_until(next_execution_time);
  } */
}

template <typename... Args> void RealTimeExecutor<Args...>::printInfo() {
  constexpr std::size_t numElements = sizeof...(Args);
  std::cout << "RealTimeExecutor number of buffers -> " << numElements
            << std::endl;

  std::apply(
      [this](auto &&...args) {
        ((std::cout << "Type -> " << typeid(args).name() << " Length -> "
                    << args.length() << std::endl),
         ...);
      },
      ringBuffers);
}

} // namespace rte
} // namespace fpe
