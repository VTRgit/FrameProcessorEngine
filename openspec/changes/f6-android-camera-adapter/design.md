# Design

## Context

On Android the zero-copy path is: camera (Camera2, CameraX, or the NDK camera) renders into a
`Surface` → an `AImageReader` owns the buffers → `AImageReader_acquireLatestImage` returns an
`AImage` whose planes point straight at the buffer. Holding the `AImage` keeps the buffer out of
the camera's queue; deleting it returns the buffer.

## Goals / Non-Goals

**Goals:** frames reach tasks without a copy; every image is deleted exactly once; the camera
never stalls because FPE holds too many images; the adapter logic is tested on the host.

**Non-Goals:** opening the camera inside the library (apps choose Camera2/CameraX/NDK camera);
format conversion (a task's job); `ImageProxy` from CameraX `ImageAnalysis` (it must be closed on
the Java side; out of scope until a consumer needs it).

## Decisions

- **The library offers a target Surface, not a camera.** `ImageReaderSource` creates the
  `AImageReader` and hands out `ANativeWindow*` / Java `Surface`. Camera2, CameraX (via a custom
  `SurfaceProvider` / output configuration) and the NDK camera can all render into it. This keeps
  FPE free of camera policy and permissions.
- **`maxImages = maxFramesInFlight + 2`.** `acquireLatestImage` needs one free slot to swap in the
  newest image; FPE may hold up to `maxFramesInFlight` (jobs) plus one waiting in the stream
  mailbox. If the reader still has no slot, the image is skipped and counted (`acquireFailures`).
- **Release callback = `AImage_delete(image)`**, context = the `AImage*` itself (no allocation).
  `AImage_delete` and `AHardwareBuffer_release` may be called from any thread.
- **Format mapping:** `YUV_420_888 → YUV420` (3 planes, row + pixel strides), `RGBA_8888 → RGBA8`
  (1 plane), `PRIVATE → Opaque` (0 CPU planes; `planes.native = AHardwareBuffer*` from
  `AImage_getHardwareBuffer`). Other formats → `InvalidArgument` (image stays with the caller).
- **Timestamps** are passed through from `AImage_getTimestamp` (the sensor clock; the app knows
  whether it is MONOTONIC or BOOTTIME). `frameId` is a per-source counter.
- **Lifetime:** frames must all be released before the reader is deleted (deleting a reader
  invalidates its images). `ImageReaderSource` counts outstanding images; `stop()` detaches the
  listener; destroying the source with outstanding images is an assertion (stop the engine first).
- **Host testing:** the adapter includes the real NDK header names; host tests put
  `tests/android_host/fake_ndk/` first on the include path and link fake implementations that
  record calls (deletes, acquires) without allocating.
- **Example:** NDK Camera2 (`ACameraManager` → back camera → repeating request into the reader's
  window), two tasks (mean luma on a subsampled Y plane; motion energy = mean absolute difference
  against the previous frame's 32×24 thumbnail, state kept inside the task since a stream has one
  frame in flight), stats polled by a Kotlin `TextView` every 250 ms.

## Risks / Trade-offs

- [No NDK in the authoring environment] → the adapter's logic is covered by host tests; the NDK
  build is verified by CI and on device by the owner (Pixel 9 Pro) before archiving.
- [Some devices give `YUV_420_888` with pixelStride 2 for chroma] → exposed as-is; tasks read strides.

## Progress (2026-11-06)

- Adapter and host tests done: 50/50 tests on GCC and Clang (Werror), ASan+UBSan and TSan
  (Android tests repeated 5× under TSan). The fake NDK mirrors the subset of `media/NdkImage.h`,
  `media/NdkImageReader.h` and `android/hardware_buffer.h` the adapter uses.
- Not yet compiled with the real NDK (none in the authoring environment): the example and the
  adapter are first built by the new CI job, then on the owner's Mac. Keep this change open until
  CI is green and the on-device run (task 3.3) is recorded.
