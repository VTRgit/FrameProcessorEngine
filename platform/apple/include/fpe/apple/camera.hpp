#pragma once

// Apple capture adapter (F7): CVPixelBuffers from AVFoundation (iOS, macOS) as external frames,
// without copying pixels. Plain C++ over CoreVideo/CoreMedia, usable from Objective-C++.

#include <fpe/engine.hpp>
#include <fpe/frame.hpp>

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

#include <atomic>
#include <cstdint>

namespace fpe::apple {

/// Fills `info` and `planes` from a pixel buffer whose base address the caller has locked (CPU
/// formats) — planes point at the buffer's memory. Bi-planar 4:2:0 (`420f`, `420v`) → NV12 (2
/// planes: Y, interleaved CbCr); `BGRA` → BGRA8 (1 plane); any other format → Opaque (0 CPU
/// planes; `planes.native` = the CVPixelBufferRef). Also sets `planes.native` for CPU formats.
Status describePixelBuffer(CVPixelBufferRef buffer, std::uint32_t streamId, std::uint64_t frameId,
                           std::int64_t timestampNs, FrameInfo& info, Planes& planes) noexcept;

/// True for the formats wrapPixelBuffer exposes as CPU planes.
bool hasCpuPlanes(CVPixelBufferRef buffer) noexcept;

/// Wraps a pixel buffer as an external frame. On success it retains the buffer and, for CPU
/// formats, locks its base address read-only; both are undone exactly once, after the frame's last
/// reference. On failure nothing is retained or locked: the caller still owns the buffer.
Status wrapPixelBuffer(FrameStore& store, CVPixelBufferRef buffer, std::uint32_t streamId,
                       std::uint64_t frameId, std::int64_t timestampNs, FrameRef& out) noexcept;

/// Presentation time of a capture sample in nanoseconds (AVFoundation's host clock).
std::int64_t presentationTimeNs(CMSampleBufferRef sample) noexcept;

struct SampleBufferSourceStats {
  std::uint64_t frames = 0;       ///< Samples submitted to the engine.
  std::uint64_t noImage = 0;      ///< Samples without a pixel buffer.
  std::uint64_t wrapFailures = 0; ///< No free external slot (or an unsupported buffer).
  std::uint64_t rejected = 0;     ///< The engine did not accept the frame (not started, ...).
};

/// Feeds an engine stream from an AVCaptureVideoDataOutput delegate: call submit() with each
/// sample, on the delegate's queue (one producer per stream). The app owns the capture session;
/// FPE does not manage cameras. Never blocks, never allocates.
///
/// Lifetime: stop the engine (which releases every frame) before destroying the source's store.
class SampleBufferSource {
public:
  SampleBufferSource(FrameStore& store, Engine& engine, std::uint32_t stream = 0) noexcept
      : store_(store), engine_(engine), stream_(stream) {}
  SampleBufferSource(const SampleBufferSource&) = delete;
  SampleBufferSource& operator=(const SampleBufferSource&) = delete;

  void submit(CMSampleBufferRef sample) noexcept;
  SampleBufferSourceStats stats() const noexcept;

private:
  FrameStore& store_;
  Engine& engine_;
  std::uint32_t stream_;
  std::uint64_t nextFrameId_ = 1; // producer only
  std::atomic<std::uint64_t> frames_{0}, noImage_{0}, wrapFailures_{0}, rejected_{0};
};

} // namespace fpe::apple
