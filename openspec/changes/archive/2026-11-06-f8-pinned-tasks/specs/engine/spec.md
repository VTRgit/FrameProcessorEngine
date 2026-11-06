# Spec Delta

## ADDED Requirements

### Requirement: Pinned tasks
A task SHALL be able to be pinned to a dedicated engine thread (lane) so that every invocation runs on that same thread, and a per-lane hook SHALL run on that thread before any task.

#### Scenario: Same thread every time
- **WHEN** a pinned task runs for 1 000 frames on an engine with three workers
- **THEN** every invocation runs on the same thread, which is not one of the workers

#### Scenario: Lane start hook
- **WHEN** the engine starts with a lane start hook
- **THEN** the hook runs on the lane thread before the first pinned task

#### Scenario: Mixed graph
- **WHEN** a worker task feeds a pinned task that feeds a worker task
- **THEN** every frame's result contains the correct outputs of all three

#### Scenario: Unknown lane
- **WHEN** a task is pinned to a lane that does not exist
- **THEN** `start()` returns `InvalidArgument` naming the task
