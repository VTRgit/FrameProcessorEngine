# Tasks

- [x] 1.1 `TaskOptions`, `EngineConfig::lanes`, `onLaneStart`; validation in `start()`
- [x] 1.2 Lane threads with their own queues; routing in `enqueue`; `stop()` drains them
- [x] 1.3 Tests: a pinned task always runs on the same thread (1 000 frames, 3 workers), the hook runs on that thread before the first task, mixed graph A(worker) → B(lane) → C(worker) gives correct results, unknown lane rejected, zero allocations on all threads, TSan
