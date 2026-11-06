# FrameProcessorEngine

A real-time frame processing engine in C++20. It runs your tasks on every frame of one or more
video streams, in parallel, and gives you **one synchronized, timestamped result per frame** with
bounded latency.

Status: **pre-release, being rebuilt on `main`.** See [docs/BACKLOG.md](docs/BACKLOG.md) and
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

## Licence

MIT. See [LICENSE](LICENSE).
