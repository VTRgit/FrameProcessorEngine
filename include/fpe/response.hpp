#pragma once

namespace fpe {

enum class Response {
  noError,         ///< No error occurred.
  allocationError, ///< Memory allocation failed.
  bufferEmpty      ///< The buffer is empty.
};

} // namespace fpe
