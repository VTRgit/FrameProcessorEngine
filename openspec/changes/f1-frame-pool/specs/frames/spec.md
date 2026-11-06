# Spec Delta

## ADDED Requirements

### Requirement: No pixel copies
The engine SHALL move only fixed-size handles between threads; pixel memory SHALL never be copied by the engine.

#### Scenario: Handle size
- **WHEN** the library is compiled
- **THEN** a static assertion guarantees that a frame handle is at most 8 bytes and trivially copyable

### Requirement: Preallocated and external frames
The engine SHALL support frames backed by a preallocated pool and frames wrapping external memory with a release callback, through the same handle type.

#### Scenario: External buffer released once
- **WHEN** an external frame is wrapped and then referenced by three owners that all release it
- **THEN** its release callback runs exactly once, with the context it was registered with

#### Scenario: Pool exhausted
- **WHEN** every slot is in use and a producer requests another
- **THEN** the request returns `PoolExhausted` immediately without blocking or allocating

### Requirement: Exactly-once release under concurrency
Every acquired frame SHALL be released exactly once regardless of how many threads reference it.

#### Scenario: Concurrent stress
- **WHEN** four threads acquire, retain and release frames one million times
- **THEN** ThreadSanitizer and AddressSanitizer report nothing and every slot is free at the end

### Requirement: No allocation after construction
Acquiring, retaining and releasing frames SHALL not allocate or free heap memory.

#### Scenario: Allocation guard
- **WHEN** acquire/retain/release run inside `EXPECT_NO_ALLOCATIONS`
- **THEN** the guard reports zero allocations

### Requirement: Stale handle detection
Using a handle after its frame was released SHALL be detected.

#### Scenario: Use after release
- **WHEN** a handle is used after its slot was released and reused
- **THEN** the access is reported as stale (assertion in debug, error status in release)
