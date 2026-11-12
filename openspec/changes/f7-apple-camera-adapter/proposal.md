# Proposal

## Why

Backlog **F7** — "iOS/macOS camera adapter", acceptance criteria:
*"platform/apple wraps CVPixelBuffer from AVFoundation as external frames without copying;
example on macOS with the built-in camera."*

FPE has an Android capture adapter (F6) but none for Apple platforms, so an iOS or macOS app has
to copy pixels or write its own wrapper. A consumer now needs camera frames on iOS.

## What Changes

- `platform/apple/` (like `platform/android/`):
  - `describePixelBuffer` / `wrapPixelBuffer`: a `CVPixelBufferRef` becomes an external frame
    whose planes point at the buffer's memory. The buffer is retained and its base address
    locked (read-only) for the frame's lifetime; unlocked and released exactly once after the last
    reference. Formats: bi-planar 4:2:0 (`420f`, `420v`) → `NV12` (2 planes: Y, interleaved CbCr),
    `BGRA` → `BGRA8`; anything else → `Opaque` with `planes.native` = the `CVPixelBufferRef`
    (for GPU work through IOSurface/Metal).
  - `SampleBufferSource`: called from an `AVCaptureVideoDataOutput` delegate with each
    `CMSampleBuffer`; wraps its pixel buffer with the sample's presentation time (host clock, ns)
    and submits it to an engine stream; counts failures like `ImageReaderSource`. The app keeps its
    `AVCaptureSession` (FPE does not manage cameras).
- `examples/macos`: a command-line demo on the built-in camera, with the same two tasks and
  stats as the Android example.
- The library builds for macOS and iOS.

Touches the frame path (the source's submit and the frames' release run per frame): **needs human
approval before apply** (FPE rule). No change to the core.

## Impact

New `platform/apple/` and `examples/macos/`; CMake option to build them on Apple platforms only.
