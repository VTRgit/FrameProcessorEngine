# Design

## Decisions

- **Lanes are extra threads, not workers taken from the pool**, so CPU tasks keep their parallelism
  and a slow GPU task never blocks them.
- **One queue per lane** (same bounded MPMC queue, capacity jobs × tasks) and a per-lane wake-up
  counter (`atomic::wait`), so routing stays lock-free and a lane sleeps only on its own work.
- **`onLaneStart(laneIndex, ctx)`** runs on the lane thread before its loop: the place to create an
  EGL context or a GPU delegate. Tasks may also initialise lazily on first call; both are safe
  because the thread never changes.
- **Validation** at `start()`: `lane >= lanes` → `Status::InvalidArgument` with the task name.
- `stop()` drains lanes like workers.

## Risks

- [A lane can become the bottleneck] → it is visible in latency; consumers decide how many lanes.

## Outcome

- 54/54 tests on GCC and Clang (Werror), ASan+UBSan, TSan; engine and pinned tests repeated 5× under TSan.
- `TaskOptions` is the last (optional) argument of `addTask`; `lanes`/`onLaneStart` are the last fields of `EngineConfig`, so existing designated initialisers keep compiling.
