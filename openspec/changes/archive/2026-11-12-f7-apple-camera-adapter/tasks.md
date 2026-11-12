# Tasks

- [x] 1.1 `platform/apple`: `describePixelBuffer`, `wrapPixelBuffer`, `SampleBufferSource`; CMake
  target `fpe::apple` on Apple platforms
- [x] 1.2 `tests/test_apple_camera.cpp` (scenarios of the spec); ctest with warnings as errors,
  ASan+UBSan, TSan on macOS
- [x] 1.3 `examples/macos` camera demo
- [x] 1.4 iOS build check of `fpe::apple` (CMake iOS toolchain)
- [x] 1.5 README and architecture doc: the Apple adapter

Outcome (2026-11-12): 8 tests on real CoreVideo/CoreMedia objects (62 in all) green with warnings
as errors, ASan+UBSan and TSan on macOS. `fpe::apple` builds for iOS arm64 with warnings as errors;
that build surfaced `TaskInput::self_` as unused in release builds (it is read only by
`FPE_ASSERT`): now `[[maybe_unused]]`. The macOS demo builds; running it needs the camera
permission of the terminal it runs in, which the check's environment did not have.
