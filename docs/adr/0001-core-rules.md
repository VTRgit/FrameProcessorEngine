# ADR 0001 — Core rules

Status: Accepted · 2026-11-06

## Context

FPE exists to run vision tasks on live video with bounded latency on phones and desktops. Latency
spikes in practice come from heap allocation, lock contention, logging and copying large buffers,
and from queues that grow when a consumer falls behind.

## Decision

1. **No allocation, free, blocking lock, I/O or logging on the frame path after `start()`.**
   Everything is sized and allocated at construction or `start()`. The test suite replaces the
   global allocator and checks it (`tests/support/alloc_guard.h`), on every engine thread.
2. **The engine never copies pixels.** Frames live in preallocated slots or in external buffers the
   engine only references by handle with a release callback.
3. **Latest frame wins.** No unbounded queue anywhere; when a consumer is slower than the producer,
   older frames are released and counted, never hidden.
4. **The core knows no platforms.** Capture and buffer adapters live under `platform/`.
5. **Offline mode is deterministic and drops nothing.**
6. **No domain code.** Pose, music or any application logic belongs to consumers.

## Consequences

- APIs take sizes up front (`maxFramesInFlight`, output sizes, worker count).
- Some conveniences (`std::function` on the hot path, dynamic graphs) are not available after `start()`.
- Every change touching the frame path ships with an allocation-guard test and runs under ASan and TSan in CI.
