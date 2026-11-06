#pragma once

#include <cstdint>

namespace fpe {

/// Result of engine operations. Nothing on the frame path throws.
enum class Status : std::uint8_t {
  Ok,
  PoolExhausted,     ///< No free frame slot or frame job; the caller drops the frame.
  StaleHandle,       ///< The handle refers to a frame that was already released.
  InvalidArgument,
  AlreadyStarted,
  NotStarted,
  GraphCycle,        ///< The task graph has a cycle.
  UnknownDependency, ///< A task depends on a task id that does not exist.
  DuplicateTask,     ///< Two tasks share a name.
  TooManyTasks,
};

constexpr const char* toString(Status s) noexcept {
  switch (s) {
  case Status::Ok: return "Ok";
  case Status::PoolExhausted: return "PoolExhausted";
  case Status::StaleHandle: return "StaleHandle";
  case Status::InvalidArgument: return "InvalidArgument";
  case Status::AlreadyStarted: return "AlreadyStarted";
  case Status::NotStarted: return "NotStarted";
  case Status::GraphCycle: return "GraphCycle";
  case Status::UnknownDependency: return "UnknownDependency";
  case Status::DuplicateTask: return "DuplicateTask";
  case Status::TooManyTasks: return "TooManyTasks";
  }
  return "Unknown";
}

} // namespace fpe

#if defined(NDEBUG)
#define FPE_ASSERT(cond, msg) ((void)0)
#else
#include <cstdio>
#include <cstdlib>
#define FPE_ASSERT(cond, msg)                                                                      \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      std::fprintf(stderr, "FPE assertion failed: %s (%s:%d)\n", msg, __FILE__, __LINE__);         \
      std::abort();                                                                                \
    }                                                                                              \
  } while (false)
#endif
