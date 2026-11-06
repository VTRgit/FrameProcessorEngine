#pragma once

// Android camera adapter: Android frames into FPE without copies.
//
//   camera (Camera2 / CameraX / NDK camera) ──renders into──▶ ImageReaderSource::window()
//   AImageReader ──newest AImage──▶ wrapped as an external frame ──▶ Engine::submit(stream)
//
// The library never opens the camera: apps give the reader's window (or Java Surface) to the
// camera API they use as an output target.

#include <fpe/detail/index_stack.hpp>
#include <fpe/engine.hpp>
#include <fpe/frame.hpp>

#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <jni.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace fpe::android {

/// Fills `info` and `planes` from an AImage without copying (planes point at the image's memory).
/// Supported: YUV_420_888 → YUV420 (3 planes), RGBA_8888 → RGBA8 (1 plane), PRIVATE → Opaque
/// (0 CPU planes, `planes.native` = the image's AHardwareBuffer). Other formats: InvalidArgument.
Status describeImage(const AImage* image, std::uint32_t streamId, std::uint64_t frameId,
                     FrameInfo& info, Planes& planes) noexcept;

/// Wraps an AImage as an external frame. On success the frame owns the image and calls
/// AImage_delete after the last reference; on failure the caller still owns the image.
Status wrapAImage(FrameStore& store, AImage* image, std::uint32_t streamId, std::uint64_t frameId,
                  FrameRef& out) noexcept;

/// Wraps an AHardwareBuffer (no CPU planes; `planes.native` = buffer). Acquires a reference on
/// success and releases it after the frame's last reference.
Status wrapHardwareBuffer(FrameStore& store, AHardwareBuffer* buffer, const FrameInfo& info,
                          FrameRef& out) noexcept;

struct ImageReaderConfig {
  std::int32_t width = 640;
  std::int32_t height = 480;
  std::int32_t format = AIMAGE_FORMAT_YUV_420_888;
  std::uint64_t usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN;
  std::uint32_t stream = 0;
  /// Should match EngineConfig::maxFramesInFlight; the reader gets this + 2 images.
  std::uint32_t maxFramesInFlight = 4;
};

struct ImageReaderStats {
  std::uint64_t images = 0;          ///< Images submitted to the engine.
  std::uint64_t acquireFailures = 0; ///< The reader had no new image or no free slot.
  std::uint64_t wrapFailures = 0;    ///< No free external slot in the FrameStore.
  std::uint64_t rejected = 0;        ///< The engine did not accept the frame (not started).
};

/// Owns an AImageReader whose window the app hands to the camera, and submits every newest image
/// to `engine` on `config.stream`. The image callback does not allocate.
///
/// Lifetime: stop the engine (which releases every frame) before destroying the source; deleting
/// the reader invalidates images still referenced.
class ImageReaderSource {
public:
  ImageReaderSource(FrameStore& store, Engine& engine, const ImageReaderConfig& config);
  ~ImageReaderSource();
  ImageReaderSource(const ImageReaderSource&) = delete;
  ImageReaderSource& operator=(const ImageReaderSource&) = delete;

  /// Creates the reader. Call once.
  Status open() noexcept;
  /// Starts delivering images (attaches the listener).
  Status start() noexcept;
  /// Stops delivering images (detaches the listener). Frames already submitted stay valid.
  void stop() noexcept;

  ANativeWindow* window() const noexcept { return window_; }
  /// Java `android.view.Surface` for Camera2/CameraX (local reference).
  jobject surface(JNIEnv* env) const noexcept;

  ImageReaderStats stats() const noexcept;
  /// Images acquired from the reader and not yet deleted.
  std::uint32_t outstanding() const noexcept { return outstanding_.load(std::memory_order_acquire); }

private:
  struct Slot {
    ImageReaderSource* source = nullptr;
    AImage* image = nullptr;
    std::uint32_t index = 0;
  };

  static void onImageAvailable(void* context, AImageReader* reader);
  static void releaseImage(void* context) noexcept;
  void handleImage() noexcept;

  FrameStore& store_;
  Engine& engine_;
  ImageReaderConfig config_;
  AImageReader* reader_ = nullptr;
  ANativeWindow* window_ = nullptr;
  AImageReader_ImageListener listener_{};
  std::unique_ptr<Slot[]> slots_;
  detail::IndexStack freeSlots_;
  std::uint64_t nextFrameId_ = 1; // image callback only
  std::atomic<std::uint32_t> outstanding_{0};
  std::atomic<std::uint64_t> images_{0};
  std::atomic<std::uint64_t> acquireFailures_{0};
  std::atomic<std::uint64_t> wrapFailures_{0};
  std::atomic<std::uint64_t> rejected_{0};
};

} // namespace fpe::android
