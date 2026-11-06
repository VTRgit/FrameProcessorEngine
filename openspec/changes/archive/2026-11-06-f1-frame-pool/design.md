# Design

## Context

Producers are capture threads (camera callback, file decoder, test generator); consumers are
worker threads running tasks (F3). One frame is read by several tasks at once and must be released
when the last of them finishes, on whatever thread that is.

## Goals / Non-Goals

**Goals:** zero copies, zero allocations after construction, exactly-once release, cheap handles.

**Non-Goals:** pixel format conversion, GPU upload, resizing (those are tasks, not engine features).

## Decisions

- **Unified slot table.** Pool slots and external frames live in one fixed table of `N` slots,
  sized at construction (`EngineConfig::maxFramesInFlight`, default 8). An external frame occupies a
  slot that stores the pointer + release callback instead of owning memory. Handles look the same
  to tasks.
- **Handle = 32-bit index + 32-bit generation.** Fits in 8 bytes, trivially copyable inside the
  engine; `FrameRef` is the RAII wrapper for users.
- **Reference count per slot (atomic `uint32_t`).** Acquire sets it to 1; each task dispatch adds
  one; release decrements with `acq_rel`; the thread that reaches 0 invokes the release callback
  (external) or returns the slot to the free list (pool).
- **Free list:** lock-free index stack with ABA protection via the generation counter (tagged
  index in a 64-bit atomic). Pool exhaustion returns `Status::PoolExhausted` to the producer
  instead of blocking; the producer drops the frame and the counter increments.
- **Release callback contract:** `noexcept`, must not block for long; documented as "may run on any
  worker thread".
- **Reuse from v0.1.x:** none of the ring buffer; its tests' style (typed tests) is reused.

## Risks / Trade-offs

- [Release callback runs on a worker thread; some platform APIs want a specific thread] → adapters
  (F6/F7) can post the release to their own thread; documented.
- [Fixed slot count] → configurable; exhaustion is visible in metrics (F4).

## Outcome (apply notes)

- `FrameHandle` is the 8-byte trivially copyable handle used inside the engine; `FrameRef` is the
  user-facing RAII owner (store pointer + handle, 16 bytes). The proposal's "8-byte FrameRef" is
  therefore `FrameHandle`; the spec (handle ≤ 8 bytes) holds.
- One slot table, two free lists (pool, external), so wrapping external buffers never steals pool
  slots and vice versa.
- Pixel formats: Opaque, RGBA8, BGRA8, NV12, NV21, Gray8; up to three planes.
- Verified: 18/18 tests on GCC and Clang (Werror), ASan+UBSan, TSan; stress test = 4 threads ×
  250 000 operations.
