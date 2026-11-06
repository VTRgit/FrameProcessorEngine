#pragma once

#define FPE_VERSION_MAJOR 0
#define FPE_VERSION_MINOR 2
#define FPE_VERSION_PATCH 0

namespace fpe {

struct Version {
  int major;
  int minor;
  int patch;
};

inline constexpr Version version{FPE_VERSION_MAJOR, FPE_VERSION_MINOR, FPE_VERSION_PATCH};

} // namespace fpe
