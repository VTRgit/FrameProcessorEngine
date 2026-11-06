# Android camera adapter

Feeds Android camera frames into FrameProcessorEngine **without copying them**.

```
Camera2 / CameraX / NDK camera ──renders into──▶ ImageReaderSource::window()  (or surface(env))
AImageReader ──newest AImage──▶ external frame (planes point at the image) ──▶ Engine::submit()
                                 AImage_delete() when the last task releases the frame
```

## Use

```cmake
add_subdirectory(FrameProcessorEngine fpe)        # builds platform/android when ANDROID is set
target_link_libraries(my-native-lib PRIVATE fpe::android)
```

```cpp
fpe::FrameStore store({.poolSlots = 0, .externalSlots = framesInFlight + 2});
fpe::Engine engine(store, {.workers = 2, .streams = 1, .maxFramesInFlight = framesInFlight});
// engine.addTask<...>(...); engine.start();
fpe::android::ImageReaderSource source(store, engine,
    {.width = 640, .height = 480, .maxFramesInFlight = framesInFlight});
source.open();
source.start();
// Give source.window() (NDK camera) or source.surface(env) (Camera2/CameraX in Java/Kotlin)
// to the camera as an output target.
...
// Shutdown order: stop the camera, source.stop(), engine.stop(), then destroy the source.
```

Also available for frames you obtain yourself: `wrapAImage()` and `wrapHardwareBuffer()`.

## Formats

| AImage format | FPE format | Planes |
| --- | --- | --- |
| `YUV_420_888` | `YUV420` | 3, with row and pixel strides (chroma is often interleaved: pixel stride 2) |
| `RGBA_8888` | `RGBA8` | 1 |
| `PRIVATE` | `Opaque` | 0 CPU planes; `planes.native` is the `AHardwareBuffer*` |

Timestamps are the sensor's (`AImage_getTimestamp`): `CLOCK_BOOTTIME` if the camera reports
`ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME`, otherwise `CLOCK_MONOTONIC`.

## Notes

- The reader gets `maxFramesInFlight + 2` images so the camera never stalls on FPE.
- The image callback does not allocate; release callbacks may run on any worker thread.
- Tested on the host against a fake NDK (`tests/android_host/`); the example app in
  `examples/android/` exercises the real NDK.
- Requires API 26+.
