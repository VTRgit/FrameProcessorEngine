// Host-test stand-in for the NDK header of the same name (subset, same signatures).
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AHardwareBuffer AHardwareBuffer;
enum {
  AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN = 3ULL,
  AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE = 1ULL << 8,
};
void AHardwareBuffer_acquire(AHardwareBuffer* buffer);
void AHardwareBuffer_release(AHardwareBuffer* buffer);
#ifdef __cplusplus
}
#endif
