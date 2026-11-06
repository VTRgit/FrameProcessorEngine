# Proposal

## Why

Backlog **F1** — "Frame pool and external buffers", acceptance criteria: *"Frames are preallocated
slots or external buffers wrapped with a release callback; the engine moves 8-byte handles, never
pixels; every slot is released exactly once (tested under ASan and TSan); zero allocations after
start()."*

The prototype stores whole `Image<W,H>` objects inside the ring buffer and copies them on every
push and pop (~400 KB per frame at 320×320 RGBA). On a phone camera at 1080p that is ~8 MB per
frame: copying it is the latency budget. Camera stacks already hand out buffers
(`AHardwareBuffer`, `CVPixelBuffer`); the engine must reference them, not copy them.

## What Changes

- `fpe::FrameInfo {streamId, frameId, timestampNs, width, height, format, strides}`.
- `fpe::FramePool<Capacity>`: fixed number of slots allocated at construction (owned pixel
  memory, aligned), lock-free acquire/release.
- `fpe::ExternalFrame`: wraps a pointer to platform memory plus a release function pointer and
  context (`void(*)(void*) noexcept`), no `std::function`.
- `fpe::FrameRef`: move-only 8-byte handle (pool index + generation, or external slot index)
  that releases its frame exactly once when the last owner drops it; a fixed-size reference
  count per slot for fan-out to several tasks (F3).
- Generation counters to detect use of a stale handle (assert in debug, error code in release).

## Capabilities

### New Capabilities
- `frames`: how frames are owned, referenced and released.

### Modified Capabilities
<!-- none -->

## Impact

- `include/fpe/frame.hpp`, `frame_pool.hpp`, tests.
- **Touches the frame path** (it defines it): requires human approval before apply.
