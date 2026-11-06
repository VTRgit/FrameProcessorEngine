# Architecture (target)

Status: F0–F3 done (frames, mailboxes, task graph, real-time and offline modes). F4+ pending; see docs/BACKLOG.md.

```
 capture thread(s)                 engine                                consumer
 ─────────────────          ─────────────────────────────────          ─────────────
 camera / file / test  ──▶  FramePool slot or ExternalFrame
                            │  (handle, timestamp, release cb)
                            ▼
                         Mailbox per stream (latest wins)       ──▶ drops counted
                            │
                            ▼
                         Scheduler ──▶ worker pool (fixed size)
                            │          runs the task graph per frame:
                            │            A ─┐
                            │            B ─┼─▶ D
                            │            C ─┘
                            ▼
                         FrameResult {stream, frameId, timestamp,
                                      outputs[task], status[task]}   ──▶  result mailbox
                                                                         (latest wins) or callback
```

## Concepts

- **Frame** (`FrameStore`, `FrameHandle`, `FrameRef`, done): a handle to pixels the engine does not own or copy, plus `{streamId, frameId,
  timestampNs, width, height, format}`. Either a slot of a preallocated `FramePool` or an
  `ExternalFrame` wrapping a platform buffer with a release callback (F1).
- **Mailbox** (`LatestMailbox<T>`, done): triple buffer, single producer / single consumer, the
  consumer always takes the newest value; a displaced value is handed back to the producer, which
  releases it; `published`, `taken` and `replaced` counters.
- **Task** (done): a callable `fn(const TaskInput&, Output&)` with a fixed-size, trivially copyable
  output and declared dependencies, registered before `start()`; `TaskInput` gives the frame and
  the outputs of dependencies. The graph is validated at `start()` (ADR 0003).
- **FrameResult** (done): one per processed frame, with every task's output or status (`Ok`,
  `Failed`, `Skipped`). Delivered through a per-stream result mailbox (latest wins) or a callback.
- **Lanes** (done, F8): dedicated threads for pinned tasks (`TaskOptions{.lane = n}`), for work
  that must stay on one thread such as a GPU context; `onLaneStart` runs on the lane first.
- **Policy**: what happens when a new frame arrives while the previous one is still being processed:
  `SkipNew` (finish the current one) or `SupersedeOld` (cancel pending tasks of the old frame) (F4).
- **Clock**: injected; monotonic by default; a fake clock in tests (F4).
- **Async task**: a task that `co_await`s an external completion (GPU, NN runtime) without holding a
  worker (F5).

## Modes

- **Real time**: latest frame wins, drops are expected and counted.
- **Offline**: every frame is processed in order, nothing is dropped, results are deterministic.
  Used for tests and for processing recorded video.

## What is not here

Pose estimation, feature extraction, rendering, audio, any app logic. Consumers build those as
tasks in their own repositories.
