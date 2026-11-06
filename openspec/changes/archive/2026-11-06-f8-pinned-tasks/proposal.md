# Proposal

## Why

Backlog **F8** — "Pinned tasks (thread affinity)", acceptance criteria: *"A task can be pinned to a
dedicated engine thread (a lane) so every call runs on the same thread, as GPU contexts (EGL,
LiteRT GPU delegate) require; a per-lane start hook runs on that thread before any task; pinned and
unpinned tasks mix in one graph; zero allocations; TSan clean."*

LiteRT's GPU delegate (and OpenGL/EGL work in general) must be invoked from the thread that
created its context. Today any worker may run any task, so a GPU inference task cannot be written.
The first consumer needs it for GPU inference on camera frames.

## What Changes

- `EngineConfig::lanes` (default 0): number of dedicated threads for pinned tasks, and
  `EngineConfig::onLaneStart` (`WorkerHook`) called on each lane thread before it runs anything.
- `TaskOptions{ .lane = n }` as an optional last argument of `addTask`; `kAnyWorker` (default) keeps
  today's behaviour. Lanes are numbered `0 .. lanes-1`; an unknown lane is rejected by `start()`.
- Each lane has its own bounded queue; the scheduler routes a ready task to its lane's queue or to
  the shared one. Dependencies across lanes and workers work as before.

## Capabilities

### Modified Capabilities
- `engine`: task placement (added requirement).

## Impact

`include/fpe/engine.hpp`, `src/engine.cpp`, tests. Touches the frame path: approved in chat
(2026-11-06).
