# Spec Delta

## ADDED Requirements

### Requirement: Task registration and validation
The engine SHALL let users register tasks with a typed fixed-size output and dependencies before `start()`, and SHALL reject invalid graphs at `start()` with an error status.

#### Scenario: Cycle rejected
- **WHEN** task A depends on B and B depends on A
- **THEN** `start()` returns an error naming the cycle and no worker thread is started

#### Scenario: Oversized output rejected at compile time
- **WHEN** a task's output type exceeds the configured maximum or is not trivially copyable
- **THEN** compilation fails with a clear static assertion

### Requirement: Parallel execution of independent tasks
Tasks of the same frame whose dependencies are satisfied SHALL run concurrently on the worker pool, and a task SHALL start only after all its dependencies finished.

#### Scenario: Diamond graph
- **WHEN** a frame runs the graph A → {B, C} → D on two or more workers
- **THEN** B and C overlap in time and D starts after both B and C have finished

### Requirement: One synchronized result per frame
For every processed frame the engine SHALL deliver exactly one result containing the frame's id and timestamp and, for every task, its output or a status.

#### Scenario: Failed task
- **WHEN** task B fails on a frame
- **THEN** the result reports B as `Failed`, its dependents as `Skipped`, and other tasks' outputs normally

#### Scenario: Real-time newest result
- **WHEN** the consumer is slower than the frame rate
- **THEN** it receives the newest available result and older ones are released

### Requirement: Deterministic offline mode
In offline mode the engine SHALL process every submitted frame in order, drop none, and produce identical outputs for identical inputs regardless of the number of workers.

#### Scenario: Reproducible run
- **WHEN** the same 1 000 synthetic frames are processed twice, with 1 and with 8 workers
- **THEN** all four runs produce identical outputs and zero drops

### Requirement: No allocation on the frame path
After `start()`, submitting frames, running tasks and delivering results SHALL not allocate or free heap memory on any engine thread.

#### Scenario: Allocation guard on all threads
- **WHEN** 1 000 frames are processed with the allocation guard armed on the producer and every worker
- **THEN** zero allocations are reported
