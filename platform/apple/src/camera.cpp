#include <fpe/apple/camera.hpp>

namespace fpe::apple {

namespace {

constexpr CVPixelBufferLockFlags kReadOnly = kCVPixelBufferLock_ReadOnly;

void unlockAndRelease(void* context) noexcept {
  auto buffer = static_cast<CVPixelBufferRef>(context);
  CVPixelBufferUnlockBaseAddress(buffer, kReadOnly);
  CVPixelBufferRelease(buffer);
}

void releaseOnly(void* context) noexcept { CVPixelBufferRelease(static_cast<CVPixelBufferRef>(context)); }

std::uint32_t u32(std::size_t v) noexcept { return static_cast<std::uint32_t>(v); }

} // namespace

bool hasCpuPlanes(CVPixelBufferRef buffer) noexcept {
  if (buffer == nullptr) {
    return false;
  }
  switch (CVPixelBufferGetPixelFormatType(buffer)) {
  case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
  case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
  case kCVPixelFormatType_32BGRA:
    return true;
  default:
    return false;
  }
}

Status describePixelBuffer(CVPixelBufferRef buffer, std::uint32_t streamId, std::uint64_t frameId,
                           std::int64_t timestampNs, FrameInfo& info, Planes& planes) noexcept {
  if (buffer == nullptr) {
    return Status::InvalidArgument;
  }
  info = FrameInfo{};
  info.streamId = streamId;
  info.frameId = frameId;
  info.timestampNs = timestampNs;
  info.width = u32(CVPixelBufferGetWidth(buffer));
  info.height = u32(CVPixelBufferGetHeight(buffer));
  planes = Planes{};
  planes.native = buffer;

  switch (CVPixelBufferGetPixelFormatType(buffer)) {
  case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
  case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange: {
    if (CVPixelBufferGetPlaneCount(buffer) != 2) {
      return Status::InvalidArgument;
    }
    info.format = PixelFormat::NV12;
    for (std::size_t p = 0; p < 2; ++p) {
      planes.data[p] = static_cast<std::uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(buffer, p));
      planes.stride[p] = u32(CVPixelBufferGetBytesPerRowOfPlane(buffer, p));
      if (planes.data[p] == nullptr) {
        return Status::InvalidArgument; // not locked
      }
    }
    planes.pixelStride[0] = 1;
    planes.pixelStride[1] = 2; // interleaved Cb, Cr
    planes.count = 2;
    return Status::Ok;
  }
  case kCVPixelFormatType_32BGRA:
    info.format = PixelFormat::BGRA8;
    planes.data[0] = static_cast<std::uint8_t*>(CVPixelBufferGetBaseAddress(buffer));
    planes.stride[0] = u32(CVPixelBufferGetBytesPerRow(buffer));
    planes.pixelStride[0] = 4;
    planes.count = 1;
    return planes.data[0] != nullptr ? Status::Ok : Status::InvalidArgument;
  default:
    info.format = PixelFormat::Opaque; // GPU work through IOSurface / Metal
    return Status::Ok;
  }
}

Status wrapPixelBuffer(FrameStore& store, CVPixelBufferRef buffer, std::uint32_t streamId,
                       std::uint64_t frameId, std::int64_t timestampNs, FrameRef& out) noexcept {
  if (buffer == nullptr) {
    return Status::InvalidArgument;
  }
  const bool cpu = hasCpuPlanes(buffer);
  if (cpu && CVPixelBufferLockBaseAddress(buffer, kReadOnly) != kCVReturnSuccess) {
    return Status::InvalidArgument;
  }
  auto undo = [&] {
    if (cpu) CVPixelBufferUnlockBaseAddress(buffer, kReadOnly);
  };
  FrameInfo info;
  Planes planes;
  if (Status s = describePixelBuffer(buffer, streamId, frameId, timestampNs, info, planes); s != Status::Ok) {
    undo();
    return s;
  }
  FrameHandle h;
  if (Status s = store.wrapExternal(info, planes, cpu ? &unlockAndRelease : &releaseOnly, buffer, h);
      s != Status::Ok) {
    undo();
    return s;
  }
  CVPixelBufferRetain(buffer);
  out = FrameRef(store, h);
  return Status::Ok;
}

std::int64_t presentationTimeNs(CMSampleBufferRef sample) noexcept {
  const CMTime t = CMSampleBufferGetPresentationTimeStamp(sample);
  if (!(t.flags & kCMTimeFlags_Valid) || t.timescale == 0) {
    return 0;
  }
  // value / timescale seconds, in ns without overflow for realistic host times.
  const std::int64_t s = t.value / t.timescale;
  const std::int64_t r = t.value % t.timescale;
  return s * 1'000'000'000 + r * 1'000'000'000 / t.timescale;
}

void SampleBufferSource::submit(CMSampleBufferRef sample) noexcept {
  CVImageBufferRef image = sample != nullptr ? CMSampleBufferGetImageBuffer(sample) : nullptr;
  if (image == nullptr) {
    noImage_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  FrameRef frame;
  if (wrapPixelBuffer(store_, image, stream_, nextFrameId_, presentationTimeNs(sample), frame) != Status::Ok) {
    wrapFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  ++nextFrameId_;
  if (engine_.submit(stream_, std::move(frame)) != Status::Ok) {
    rejected_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  frames_.fetch_add(1, std::memory_order_relaxed);
}

SampleBufferSourceStats SampleBufferSource::stats() const noexcept {
  SampleBufferSourceStats s;
  s.frames = frames_.load(std::memory_order_relaxed);
  s.noImage = noImage_.load(std::memory_order_relaxed);
  s.wrapFailures = wrapFailures_.load(std::memory_order_relaxed);
  s.rejected = rejected_.load(std::memory_order_relaxed);
  return s;
}

} // namespace fpe::apple
