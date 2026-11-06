#pragma once

// Control surface of the fake NDK used by host tests. Fake images and readers are preallocated
// (no heap allocation in the image path, so allocation guards stay meaningful).

#include <android/hardware_buffer.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include <atomic>
#include <cstdint>

struct AHardwareBuffer {
  std::atomic<int> acquires{0};
  std::atomic<int> releases{0};
};

struct AImage {
  std::int32_t format = AIMAGE_FORMAT_YUV_420_888;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int64_t timestampNs = 0;
  std::int32_t planeCount = 0;
  std::uint8_t* data[3]{};
  int length[3]{};
  std::int32_t rowStride[3]{};
  std::int32_t pixelStride[3]{};
  AHardwareBuffer* buffer = nullptr;
  std::atomic<int> deletes{0};
  std::atomic<bool> inUse{false};
};

namespace fakendk {

/// Fills a YUV_420_888 image over caller-owned memory: Y (w×h), then U and V interleaved
/// (pixelStride 2, like most devices).
void makeYuv(AImage& img, std::uint8_t* memory, std::int32_t w, std::int32_t h, std::int64_t ts);

/// The fake reader (one at a time). `produce()` queues an image and invokes the listener, like
/// the camera producing a frame.
struct Reader {
  static constexpr int kMaxImages = 16;
  AImage images[kMaxImages];
  std::uint8_t memory[kMaxImages][64 * 48 * 2]{};
  int maxImages = 0;
  std::int32_t width = 0, height = 0, format = 0;
  std::atomic<int> held{0};       // acquired and not yet deleted
  std::atomic<int> pendingIndex{-1};
  std::atomic<std::int64_t> nextTimestamp{1'000'000};
  AImageReader_ImageListener listener{};
  bool deleted = false;
  int created = 0;
};

Reader& reader();
void resetReader();
/// Queues a new image (replacing a pending one) and calls the listener. False if every image
/// buffer is in use.
bool produce();

} // namespace fakendk
