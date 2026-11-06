# Proposal

## Why

Backlog **F2** — "Latest-frame mailbox per stream", acceptance criteria: *"Wait-free
single-producer/single-consumer mailbox per stream where the consumer always gets the newest
frame; overwritten frames are released and counted; no data race under TSan with a producer at
240 fps and a slow consumer."*

The prototype's `RingBuffer` overwrites the oldest element by advancing `tail` from the producer,
while the consumer also writes `tail`: a data race, and an element can be read while it is being
overwritten. It also keeps old frames queued, which is the opposite of what real time needs: a
consumer that falls behind should see the newest frame, not the backlog.

## What Changes

- `fpe::LatestMailbox<T>`: triple-buffer SPSC exchange of trivially copyable `T` (frame handles,
  later `FrameResult` handles). `publish(T)` never blocks; `take()` returns the newest value
  published since the last take, or nothing.
- When `publish` replaces a value the consumer never took, the replaced value is returned to the
  producer so it can release it (`FrameRef` → slot back to the pool), and a `replaced` counter
  increments.
- The `v0.1.x` ring buffer is not ported. A separate bounded SPSC FIFO (no overwrite) is out of
  scope until a consumer needs ordered delivery (offline mode in F3 uses its own path).

## Capabilities

### New Capabilities
- `mailbox`: latest-value exchange between one producer and one consumer.

### Modified Capabilities
<!-- none -->

## Impact

- `include/fpe/latest_mailbox.hpp`, tests.
- **Touches the frame path**: requires human approval before apply.
