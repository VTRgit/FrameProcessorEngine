# Tasks

## 1. Frames

- [x] 1.1 `Planes::pixelStride`, `Planes::native`, `PixelFormat::YUV420`; existing tests still green

## 2. Adapter (platform/android)

- [x] 2.1 `wrapAImage` with format mapping and exactly-once `AImage_delete`; failure leaves the image with the caller
- [x] 2.2 `wrapHardwareBuffer` with balanced acquire/release
- [x] 2.3 `ImageReaderSource` (reader, window, JNI surface, listener → submit, counters, outstanding images, stop)
- [x] 2.4 Host tests against the fake NDK: plane mapping, zero copy (same pointers), exactly-once delete with shared owners, exhaustion, PRIVATE → native buffer, source → engine results, outstanding back to 0 after engine stop, no allocations in the image callback; GCC/Clang/ASan/TSan

## 3. Example and CI

- [x] 3.1 `examples/android/` (Gradle, NDK Camera2, two tasks, on-screen fps/latency/drops)
- [x] 3.2 CI job building the example with the NDK (written; NDK build verified locally; first CI run on push)
- [x] 3.3 On-device run (Pixel 9 Pro): record fps, latency and drops in the change's design outcome

## 4. Docs

- [x] 4.1 `platform/android/README.md`; README and architecture updated
