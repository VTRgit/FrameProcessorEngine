# ADR 0002 — Frames are referenced, never copied

Status: Accepted · 2026-11-06

## Context

A 1080p RGBA camera frame is ~8 MB. Copying it once per hop (capture → queue → task) costs more
than most vision tasks, and camera stacks already hand out buffers (`AHardwareBuffer`,
`CVPixelBuffer`) that must be returned to them.

## Decision

- Frames live in a fixed `FrameStore` table created up front: **pool slots** (memory owned by the
  store, aligned) and **external slots** (memory owned by the platform, with a `noexcept` release
  function pointer + context).
- Inside the engine a frame is an 8-byte `FrameHandle` (index + generation). Users hold
  `FrameRef`, a move-only RAII owner; `share()` adds an owner.
- Each slot has an atomic reference count; the thread that drops the last reference bumps the
  generation (invalidating old handles), calls the release callback (external) and returns the slot
  to a lock-free, ABA-tagged free list.
- A stale handle is an assertion in debug builds and `Status::StaleHandle` in release builds.
- Exhaustion never blocks: `PoolExhausted` is returned and the producer drops the frame.

## Consequences

- Release callbacks may run on any engine thread; platform adapters that need a specific thread
  post the release there.
- The number of frames in flight is a configuration decision, visible as `PoolExhausted` drops.
