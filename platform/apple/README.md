# Apple camera adapter (iOS, macOS)

Feeds AVFoundation camera frames into FrameProcessorEngine **without copying them**.

```
AVCaptureSession ──▶ AVCaptureVideoDataOutput delegate ──▶ SampleBufferSource::submit(sample)
CVPixelBuffer ──retain + lock (read-only)──▶ external frame (planes point at the buffer) ──▶ Engine::submit()
                unlock + release when the last task releases the frame
```

## Use

```cmake
add_subdirectory(FrameProcessorEngine fpe)        # builds platform/apple when APPLE is set
target_link_libraries(my-app PRIVATE fpe::apple)
```

```objc
fpe::FrameStore store({.poolSlots = 0, .externalSlots = framesInFlight + 2});
fpe::Engine engine(store, {.workers = 2, .streams = 1, .maxFramesInFlight = framesInFlight});
// ... addTask, engine.start()
fpe::apple::SampleBufferSource source(store, engine);   // stream 0

// AVCaptureVideoDataOutput with kCVPixelFormatType_420YpCbCr8BiPlanarFullRange (or BGRA),
// alwaysDiscardsLateVideoFrames = YES, and in the delegate:
- (void)captureOutput:(AVCaptureOutput*)o didOutputSampleBuffer:(CMSampleBufferRef)sb
       fromConnection:(AVCaptureConnection*)c {
  source.submit(sb);
}
```

- Formats: `420f` / `420v` → `NV12` (plane 0 = Y, plane 1 = interleaved CbCr, pixel stride 2);
  `BGRA` → `BGRA8`; anything else → `Opaque` with `planes.native` = the `CVPixelBufferRef` (for
  GPU tasks through IOSurface / Metal). CPU formats also expose the buffer as `planes.native`.
- Timestamps: the sample's presentation time in ns (AVFoundation's host clock,
  `mach_absolute_time` based).
- The app owns the capture session; FPE does not manage cameras.
- AVFoundation recycles its buffers when they are released: a frame held too long starves the
  capture. Latest frame wins; drops are counted by the engine and in `stats()`.
- Lifetime: stop the engine (which releases every frame) before destroying the store.

`examples/macos/camera_demo.mm` runs it on the Mac's camera (the terminal needs the camera
permission). `wrapPixelBuffer` can also be used directly for buffers from other sources.
