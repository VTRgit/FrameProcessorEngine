# ADR 0003 — Static task graph and fixed-size outputs

Status: Accepted · 2026-11-06

## Context

Per frame, consumers run a few tasks (2–6 typically) with dependencies between them, at 24–120 fps,
and need every output of a frame together. ADR 0001 forbids allocation after `start()`.

## Decision

- Tasks are registered before `start()` with `addTask<Output>(name, fn, deps)`; `addDependency()`
  allows forward references. `start()` validates the graph (Kahn) and rejects cycles, duplicate
  names and unknown dependencies with a `Status` and a readable `errorMessage()`.
- Callables are stored at registration (allocation is allowed before `start()`) behind a virtual
  `Callable`; calling them never allocates. `fn(const TaskInput&, Output&)` returns `bool` or `void`.
- Outputs are trivially copyable, default constructible, at most `kMaxTaskOutputBytes` (4096) with
  alignment ≤ 64, checked at compile time; they live in preallocated per-job storage.
- A **job** per frame in flight (`maxFramesInFlight`) holds atomic dependency counters, task statuses
  and outputs. Workers (fixed pool) pull `(job, task)` items from a bounded lock-free MPMC queue and
  park on `std::atomic::wait`. Finishing a task releases its dependents; the last task finishes the job.
- A failed task marks its dependents `Skipped`; other tasks still run.
- Real time: one frame in flight per stream; newer frames wait in the stream's latest-wins mailbox;
  results go to a per-stream latest-wins mailbox or to a callback. Offline: `process()` runs one
  frame and waits for its result; nothing is dropped.

## Consequences

- No cross-frame dependencies in the engine: a task needing history keeps its own state.
- A consumer that holds results too long exhausts jobs; it shows as `droppedNoJob` in `stats()`.
- Policies for overlapping frames of one stream (supersede/skip, F4) build on the per-stream busy flag.
