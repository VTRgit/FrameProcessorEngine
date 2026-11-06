# mailbox Specification

## Purpose

See the archived change that introduced it: `changes/archive/2026-11-06-f2-latest-frame-mailbox`.

## Requirements

### Requirement: Newest value wins
A mailbox SHALL deliver to its consumer the most recently published value that the consumer has not yet taken, and never an older one.

#### Scenario: Several publishes before a take
- **WHEN** the producer publishes A, B and C and the consumer then takes
- **THEN** the consumer receives C, and a second take returns nothing

#### Scenario: Monotonic delivery under load
- **WHEN** a producer publishes increasing frame ids at 240 fps and a slow consumer takes at random intervals
- **THEN** every taken id is greater than the previous taken id

### Requirement: Displaced values are handed back
When a publish displaces a value the consumer never took, the mailbox SHALL return that value to the producer and count it as replaced.

#### Scenario: Displaced frame released
- **WHEN** frames flow through the mailbox from a frame pool and the consumer is slower than the producer
- **THEN** every frame is released exactly once and the replaced counter equals the number of frames never taken

### Requirement: Wait-free and allocation-free
Publish and take SHALL complete in a bounded number of steps without locks, waiting or heap allocation.

#### Scenario: Allocation guard
- **WHEN** publish and take run inside `EXPECT_NO_ALLOCATIONS`
- **THEN** zero allocations are reported

#### Scenario: Thread sanitizer
- **WHEN** the concurrency stress test runs under ThreadSanitizer
- **THEN** no data race is reported
