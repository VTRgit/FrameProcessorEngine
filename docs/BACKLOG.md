# Backlog

Work restarts on `main`. The `v0.1.x` branch (PR #1) is reference only: its lock-free ring buffer
and tests are reused where they still fit, the rest is replaced. PR #1 can be closed: F2 replaced its ring buffer.

Priority: P0 = needed for the first consumer prototype, P1 = needed for v1.0.

| ID | P | Title | Acceptance criteria |
| --- | --- | --- | --- |
| F0 ✅ | P0 | Project foundation | "CMake project with library, tests (GoogleTest) and example; CI on Linux (gcc, clang) and macOS with warnings as errors, ASan+UBSan and TSan; ADR 0001 with the core rules; a test helper that fails if the frame path allocates." |
| F1 ✅ | P0 | Frame pool and external buffers | "Frames are preallocated slots or external buffers wrapped with a release callback; the engine moves 8-byte handles, never pixels; every slot is released exactly once (tested under ASan and TSan); zero allocations after start()." |
| F2 ✅ | P0 | Latest-frame mailbox per stream | "Wait-free single-producer/single-consumer mailbox per stream where the consumer always gets the newest frame; overwritten frames are released and counted; no data race under TSan with a producer at 240 fps and a slow consumer." |
| F3 ✅ | P0 | Task graph and synchronized results | "Tasks declared with dependencies and a typed output; per frame the engine runs independent tasks in parallel on a fixed worker pool and delivers one FrameResult with the frame timestamp and every task's output or status; offline mode processes a frame sequence deterministically with zero drops." |
| F4 | P1 | Deadlines, drop policy and metrics | "Per-stream policy when a new frame arrives before the previous one finished (skip-new or supersede-old); per-task latency percentiles and drop counters readable without locks; injectable clock; tests with a fake clock." |
| F5 | P1 | Asynchronous tasks (C++20 coroutines) | "A task can co_await an external completion (e.g. a GPU fence or an inference callback) without blocking a worker; cancellation when its frame is superseded; no allocation per frame (coroutine frames from a preallocated arena)." |
| F6 ✅ | P1 | Android camera adapter | "platform/android wraps AHardwareBuffer / AImage from Camera2 or CameraX as external frames without copying; example app shows fps, latency and drops; builds with NDK in CI." |
| F7 ✅ | P1 | iOS/macOS camera adapter | "platform/apple wraps CVPixelBuffer from AVFoundation as external frames without copying; example on macOS with the built-in camera." |
| F8 ✅ | P0 | Pinned tasks (thread affinity) | "A task can be pinned to a dedicated engine thread (a lane) so every call runs on the same thread, as GPU contexts (EGL, LiteRT GPU delegate) require; a per-lane start hook runs on that thread before any task; pinned and unpinned tasks mix in one graph; zero allocations; TSan clean." |
| F9 | P0 | ThreadSanitizer CI job builds | "The Linux TSan CI job builds and runs every test: the test allocation guard does not replace the global operator new/delete under TSan (the sanitizer runtime owns them), allocation-counting checks are skipped there and still run in every other job." |
