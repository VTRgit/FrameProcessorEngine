# Design

## API (`platform/apple/include/fpe/apple/camera.hpp`, C++ usable from Objective-C++)

```cpp
namespace fpe::apple {
Status describePixelBuffer(CVPixelBufferRef buffer, std::uint32_t streamId, std::uint64_t frameId,
                           std::int64_t timestampNs, FrameInfo& info, Planes& planes) noexcept;
// Retains + locks read-only on success; unlock + release once after the last reference.
// On failure (PoolExhausted, unsupported) nothing is retained: the caller still owns the buffer.
Status wrapPixelBuffer(FrameStore& store, CVPixelBufferRef buffer, std::uint32_t streamId,
                       std::uint64_t frameId, std::int64_t timestampNs, FrameRef& out) noexcept;

struct SampleBufferSourceStats { std::uint64_t frames = 0, wrapFailures = 0, rejected = 0, noImage = 0; };
class SampleBufferSource {
 public:
  SampleBufferSource(FrameStore& store, Engine& engine, std::uint32_t stream = 0) noexcept;
  /// From the capture delegate's queue (one producer per stream). Never blocks, no allocation.
  void submit(CMSampleBufferRef sample) noexcept;
  SampleBufferSourceStats stats() const noexcept;
};
}
```

- Planes: `NV12` → `data[0]` = Y base, `data[1]` = CbCr base, strides from
  `CVPixelBufferGetBytesPerRowOfPlane`, `pixelStride` 1 and 2; `count` 2. `BGRA8` → 1 plane.
  `Opaque` → `count` 0, `native` = the buffer (not locked: no CPU access).
- Timestamp: `CMSampleBufferGetPresentationTimeStamp` in ns (the host clock AVFoundation uses);
  `receivedNs` set by the engine as usual.
- Release: the external slot's `ReleaseFn` gets the buffer as context: unlock (if locked), then
  `CVPixelBufferRelease`. AVFoundation recycles its pool buffers when released, so a frame held
  too long starves the capture (the same back-pressure as Android's image reader: latest frame
  wins, drops counted by the engine).

## Tests (host, macOS; ctest with warnings as errors, ASan+UBSan, TSan)

`tests/test_apple_camera.cpp` with buffers from `CVPixelBufferCreateWithPlanarBytes` /
`CVPixelBufferCreateWithBytes` (a release callback counts the frees):

- NV12 frame: format, size, timestamp, 2 planes with the buffer's strides, pointers equal to the
  buffer's plane addresses;
- BGRA frame: 1 plane; other format: `Opaque`, no CPU planes, `native` is the buffer;
- shared by three owners: released once, after the last;
- store exhausted: `PoolExhausted`, buffer not retained (retain count unchanged);
- `SampleBufferSource` with `CMSampleBuffer`s made from pixel buffers: results arrive with their
  timestamps; outstanding buffers return to zero after the engine stops;
- no allocation in `submit` after warm-up (the alloc guard on the producer thread).

## Example

`examples/macos/camera_demo.mm`: `AVCaptureSession` on the default camera, 640×480 `420f`, the
delegate calls `SampleBufferSource::submit`; tasks: mean luma and motion energy (as Android);
prints fps, latency (presentation time → result), drops every second. Needs the camera permission
of the terminal it runs in.
