# Design

## Context

Consumers will register a handful of tasks (typically 2–6) per stream, some on the CPU, some
waiting on a GPU or a neural-network runtime (async waits are F5; in F3 a task just runs to
completion on a worker). Frames arrive at 24–120 fps; a frame's tasks must finish within roughly
one frame period.

## Goals / Non-Goals

**Goals:** parallel execution of independent tasks, one result per frame, no allocation after
`start()`, a deterministic offline path for tests.

**Non-Goals:** cross-frame dependencies (task reading the previous frame's output — later, if
needed, as explicit state owned by the task); dynamic graphs after `start()`; drop policies (F4).

## Decisions

- **Static graph, compiled at `start()`** into a topological order and per-task dependency counts.
  Per frame in flight, a `FrameJob` holds atomic remaining-dependency counters (copied from the
  template), the `FrameRef`, and output storage. `FrameJob`s are preallocated: `maxFramesInFlight`
  per engine.
- **Work queue:** a bounded lock-free MPMC queue of `(jobIndex, taskIndex)` pairs sized to
  `maxFramesInFlight × tasks`; workers park on a `std::atomic::wait`/`notify` (C++20) when it is
  empty — no mutex, no condition variable on the hot path.
- **Completion:** the worker finishing a task decrements each dependent's counter; reaching zero
  enqueues it. The last task of a frame publishes the result. A failed task (returns an error
  status) marks its dependents `Skipped`.
- **Typed outputs without allocation:** each task gets a fixed-size, aligned byte region in the
  `FrameJob`; `addTask<Output>` checks `sizeof(Output) <= maxOutputBytes` and trivial copyability at
  compile time.
- **FrameResult lifetime:** the result is a handle to its `FrameJob`; the consumer releases it,
  which returns the job (and the frame) to their pools. Consumer holding results too long shows as
  pool exhaustion (visible in F4 metrics).
- **Offline mode:** same scheduler, but `process()` waits for the job's completion with
  `atomic::wait`; inputs come in order and the input mailbox is bypassed, so nothing is dropped.
  Determinism is defined as identical outputs for identical inputs (task functions must be
  deterministic themselves; scheduling order may vary).
- **Exceptions:** task functions are `noexcept`; a thrown exception terminates (documented).

## Risks / Trade-offs

- [Lock-free MPMC queue is the hardest piece] → use a well-known bounded design (Vyukov) with tests
  under TSan; consider `std::mutex` fallback behind a flag only for debugging.
- [Fixed output size wastes memory for small outputs] → default 4 KB × tasks × frames in flight is
  ~200 KB for 6 tasks and 8 frames; configurable.
