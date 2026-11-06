# Tasks

## 1. Registration

- [ ] 1.1 `EngineConfig`, `Engine::addTask<Output>`, `TaskId`; compile-time checks on `Output`; tests for duplicate names and unknown dependencies
- [ ] 1.2 Graph validation at `start()` (cycles, missing deps) with `Status` errors; tests

## 2. Scheduler

- [ ] 2.1 Bounded MPMC work queue (Vyukov) + unit and TSan stress tests
- [ ] 2.2 Worker pool with `atomic::wait` parking, start/stop/join, optional thread hook; test: stop with work pending finishes or cancels cleanly
- [ ] 2.3 `FrameJob` pool and dependency counters; tests: diamond graph A→{B,C}→D runs B and C in parallel (observed overlap with timestamps), D after both

## 3. Results

- [ ] 3.1 `FrameResult` handle with `get<Output>(TaskId)` and per-task `TaskStatus`; failed task → dependents `Skipped`; tests
- [ ] 3.2 Real-time path: input mailbox → scheduler → result mailbox; test with a synthetic 60 fps producer: results arrive with the right frame ids and timestamps; newest-wins under a slow consumer

## 4. Offline and guarantees

- [ ] 4.1 Offline `process()`: 1 000 synthetic frames in order, zero drops, outputs identical across two runs and across 1, 2 and 8 workers
- [ ] 4.2 `EXPECT_NO_ALLOCATIONS` on all threads from the first frame to the 1 000th (armed workers)
- [ ] 4.3 Example with two streams and three tasks; prints fps, mean latency and drops

## 5. Docs

- [ ] 5.1 ADR 0003 "Static task graph and fixed-size outputs"; README usage snippet; `docs/architecture.md` updated
