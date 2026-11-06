# Design

## Context

One mailbox per input stream (capture thread → scheduler) and one for results (engine → consumer).
Both sides must never wait for each other: the camera callback cannot block, and the consumer
(UI or audio-control thread) cannot either.

## Goals / Non-Goals

**Goals:** wait-free publish and take, newest-value semantics, explicit ownership of replaced values.

**Non-Goals:** multiple producers or consumers per mailbox; ordered FIFO delivery.

## Decisions

- **Classic triple buffer.** Three slots of `T`; the producer owns `back`, the consumer owns
  `front`, `middle` is exchanged through one `std::atomic<uint8_t>` packing the middle index and a
  "fresh" bit. Publish: write `back`, `exchange` with middle (acq_rel), the old middle becomes the
  new `back`. Take: if fresh bit set, `exchange` front with middle.
- **Replaced values returned, not destroyed inside.** `publish` returns `std::optional<T>` with the
  value it displaced (if the consumer never saw it) so the producer releases it. Keeps `T` trivially
  copyable and the mailbox free of release logic.
- **Counters:** `published`, `taken`, `replaced` as relaxed atomics, readable from any thread.
- **`T` constraint:** `std::is_trivially_copyable_v<T> && sizeof(T) <= 16`.
- **Cache-line separation** of producer and consumer fields (`alignas(std::hardware_destructive_interference_size)`
  with a 64-byte fallback).

## Risks / Trade-offs

- [A value can be returned for release only on the producer side] → fine for frames (producer owns
  the pool slot until the consumer takes it); documented.

## Outcome (apply notes)

- Cache-line size fixed at 64 bytes instead of `std::hardware_destructive_interference_size`
  (GCC warns that its value is not ABI-stable across flags).
- The producer/consumer roles may move between threads if each hand-over is ordered by an
  acquire/release flag; F3 relies on this (one frame in flight per stream).
- Verified: 25/25 tests on GCC and Clang (Werror), ASan+UBSan, TSan; 240 fps and unthrottled
  producers against a randomly sleeping consumer; frames through the mailbox all released.
