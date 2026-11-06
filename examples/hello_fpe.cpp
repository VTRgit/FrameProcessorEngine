#include <fpe/version.hpp>

#include <cstdio>

int main() {
  std::printf("FrameProcessorEngine %d.%d.%d\n", fpe::version.major, fpe::version.minor,
              fpe::version.patch);
  return 0;
}
