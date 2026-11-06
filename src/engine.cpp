// Placeholder translation unit so the library is a real (static) target from F0 on.
// The engine implementation lands here in F3.
#include <fpe/version.hpp>

namespace fpe::detail {
int versionMajor() noexcept { return version.major; }
} // namespace fpe::detail
