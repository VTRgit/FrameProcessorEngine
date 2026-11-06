# FrameProcessorEngine

A real-time frame processing engine in C++20. It runs your tasks on every frame of one or more
video streams, in parallel, and gives you **one synchronized, timestamped result per frame** with
bounded latency.

Status: **pre-release (0.2).** Frames, mailboxes, task graph, real-time and offline modes are in;
the Android camera adapter (`platform/android`) is in, with an example app
(`examples/android`: 60 fps camera, 0.4 ms in FPE on a Pixel 9 Pro); deadline policies, metrics, coroutine tasks and the Apple adapter are next. See [docs/BACKLOG.md](docs/BACKLOG.md) and
[docs/architecture.md](docs/architecture.md). The early prototype lives in the `v0.1.x` branch.

## Why

Computer-vision pipelines on phones and PCs usually run several algorithms on the same frame
(detection, segmentation, tracking, a GPU pass) and need their results together, now, for the
newest frame, not a growing queue of old ones. FPE does that plumbing once:

- **Zero-copy frames**: preallocated slots or external buffers (`AHardwareBuffer`, `CVPixelBuffer`, …) referenced by handle.
- **Latest frame wins**: slow tasks never build a backlog; dropped frames are counted.
- **Task graph**: declare tasks and their dependencies; independent tasks run in parallel on a fixed worker pool.
- **Synchronized results**: one `FrameResult` per frame with every task's output and status.
- **Real-time friendly**: no allocations, locks or logging on the frame path after `start()`.
- **Deterministic offline mode**: process a recorded sequence frame by frame, no drops, same results every time.
- **Async tasks** with C++20 coroutines: wait for a GPU fence or an inference callback without blocking a thread.

## Usage

```cpp
fpe::FrameStore store({.poolSlots = 8, .poolSlotBytes = 320 * 240, .externalSlots = 4});
fpe::Engine engine(store, {.workers = 3, .streams = 1, .maxFramesInFlight = 4});

auto brightness = engine.addTask<Brightness>("brightness", computeBrightness);
auto edges      = engine.addTask<EdgeCount>("edges", countEdges);
auto summary    = engine.addTask<Summary>("summary",
    [=](const fpe::TaskInput& in, Summary& out) {
      out.brightness = in.input<Brightness>(brightness).mean;
      out.edges = in.input<EdgeCount>(edges).edges;
    },
    {brightness, edges});
engine.start();

// Capture thread: wrap the camera buffer (no copy) and submit it.
fpe::FrameHandle h;
store.wrapExternal(info, planes, releaseCameraBuffer, cameraBuffer, h);
engine.submit(0, fpe::FrameRef(store, h));

// Consumer: always the newest finished frame.
if (fpe::FrameResult r = engine.takeResult(0)) {
  use(*r.get<Summary>(summary), r.info().timestampNs);
}
```

See `examples/hello_fpe.cpp` for a complete two-stream program.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFPE_WARNINGS_AS_ERRORS=ON
cmake --build build && ctest --test-dir build --output-on-failure
cmake -S . -B build-asan -DFPE_SANITIZE=ON && cmake --build build-asan && ctest --test-dir build-asan
cmake -S . -B build-tsan -DFPE_TSAN=ON && cmake --build build-tsan && ctest --test-dir build-tsan
```

Requires CMake ≥ 3.21 and a C++20 compiler (GCC 13, Clang 17, AppleClang 15 or newer). The library
has no dependencies; GoogleTest is fetched for the tests only. Use it from another project with
`add_subdirectory(FrameProcessorEngine)` and link `fpe::fpe`.

## Licence

MIT. See [LICENSE](LICENSE).
