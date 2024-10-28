#pragma once

namespace fpe {

enum class Response {
  noError,         ///< No error occurred.
  allocationError, ///< Memory allocation failed.
  bufferEmpty      ///< The buffer is empty.
};
}

extern "C" __attribute__((visibility("default"))) fpe::Response startFPE();
extern "C" __attribute__((visibility("default"))) fpe::Response stopFPE();
