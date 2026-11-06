# Proposal

## Why

Backlog **F6** — "Android camera adapter", acceptance criteria: *"platform/android wraps
AHardwareBuffer / AImage from Camera2 or CameraX as external frames without copying; example app
shows fps, latency and drops; builds with NDK in CI."*

F0–F3 give a working engine, but nothing feeds it real camera frames. The first consumer is a
camera app on Android; its frames arrive as `AImage` (from an `AImageReader`) or `AHardwareBuffer`.

## What Changes

- `Planes` gains `pixelStride[3]` (needed for `YUV_420_888`, whose chroma planes may be
  interleaved) and `void* native` (the platform buffer, e.g. `AHardwareBuffer*`, for GPU tasks).
  `PixelFormat` gains `YUV420` (flexible 4:2:0 described by strides). Additive; F1 behaviour unchanged.
- New target `fpe_android` in `platform/android/` (built only for Android):
  - `wrapAImage(store, image, streamId, frameId, out)`: zero-copy `FrameRef` over an `AImage`
    (YUV_420_888, RGBA_8888, PRIVATE); `AImage_delete` runs when the last reference drops.
  - `wrapHardwareBuffer(store, buffer, info, out)`: acquires the buffer, releases it at the end.
  - `ImageReaderSource`: owns an `AImageReader`, exposes its `ANativeWindow` (and a Java `Surface`
    via JNI) to be given to Camera2 or CameraX as a target, and submits every new image to an
    `Engine` stream (latest image wins, as in the engine).
- Host tests of the adapter against a fake NDK (same C signatures), so the logic is covered by the
  normal test suite, ASan and TSan.
- `examples/android/`: a minimal app (NDK Camera2 → `ImageReaderSource` → FPE with two tasks)
  showing fps, latency and drops on screen.
- CI job that builds the example with the NDK.

## Capabilities

### New Capabilities
- `android-camera`: feeding Android camera frames into the engine without copies.

### Modified Capabilities
- `frames`: plane layout gains pixel strides and a native buffer pointer (added requirement).

## Impact

- `include/fpe/frame.hpp` (additive fields), `platform/android/`, `tests/android_host/`,
  `examples/android/`, `.github/workflows/ci.yml`.
- Touches the frame path (capture callback): requires human approval — given in chat on 2026-11-06.
