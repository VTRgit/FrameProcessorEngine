# Proposal

## Why

Backlog **F3** — "Task graph and synchronized results", acceptance criteria: *"Tasks declared with
dependencies and a typed output; per frame the engine runs independent tasks in parallel on a fixed
worker pool and delivers one FrameResult with the frame timestamp and every task's output or
status; offline mode processes a frame sequence deterministically with zero drops."*

This is the engine itself. In the prototype, `FrameProcessorEngine::start()`/`stop()` are empty
and the executor's loop is commented out.

## What Changes

- `fpe::EngineConfig {workers, maxFramesInFlight, streams, mode}`; `mode` is `RealTime` or `Offline`.
- `fpe::TaskId fpe::Engine::addTask<Output>(name, fn, dependsOn...)` before `start()`. `Output` is
  trivially copyable with a fixed maximum size (configurable, default 4 KB) and stored in
  preallocated per-frame result storage. Function stored as a function pointer + context, not
  `std::function`.
- Graph validated at `start()`: acyclic, dependency outputs exist; errors returned as `Status`.
- Fixed worker pool created at `start()`, joined at `stop()`; optional platform hook to set
  thread priority/affinity (no platform code in the core).
- Per frame: tasks without pending dependencies are dispatched to workers; completion of a task
  releases its dependents; when all tasks finish, a `FrameResult` (handle) is published to the
  result mailbox (F2) or passed to a callback.
- `FrameResult` gives typed access to each task's output (`result.get<Output>(taskId)`) and status
  (`Ok`, `Failed`, `Skipped`).
- Real-time mode: one frame in flight per stream for now; a newer frame waits in the input mailbox
  (latest wins). Policies beyond that are F4.
- Offline mode: `engine.process(FrameRef)` blocks the caller until the result is ready; frames are
  processed in order; nothing is dropped; results are deterministic.
- Replace `examples/hello_fpe.cpp` with a two-stream example using synthetic frames (gradient +
  checkerboard) and three tasks (mean brightness, edge count, a dependent task combining both).

## Capabilities

### New Capabilities
- `engine`: task registration, scheduling and per-frame results.

### Modified Capabilities
<!-- none -->

## Impact

- `include/fpe/engine.hpp`, `task.hpp`, `frame_result.hpp`, `src/worker_pool.cpp` (library becomes `STATIC`), tests, example.
- **Touches the frame path**: requires human approval before apply.
