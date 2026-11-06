# Tasks

## 1. Types

- [ ] 1.1 `FrameInfo`, `PixelFormat` (RGBA8, BGRA8, NV12, NV21, Gray8, Opaque), `Status`; verify compile + unit tests for defaults
- [ ] 1.2 Slot table with tagged lock-free free list; tests: acquire until exhausted returns `PoolExhausted`, release makes the slot available again

## 2. Handles

- [ ] 2.1 `FrameRef` (move-only, 8 bytes, `static_assert`), `retain()` for fan-out, release on destruction; tests: exactly-once release counted by a fake callback with 1, 2 and 16 owners
- [ ] 2.2 Stale-handle detection via generation; test: using a handle after release is reported
- [ ] 2.3 `ExternalFrame` wrap with release function pointer + context; test: callback receives its context exactly once

## 3. Guarantees

- [ ] 3.1 `EXPECT_NO_ALLOCATIONS` around acquire/retain/release loops (pool and external)
- [ ] 3.2 Stress test: 4 threads acquiring/retaining/releasing 1 M times; ASan and TSan clean; final state all slots free
- [ ] 3.3 ADR 0002 "Frames are referenced, never copied"; update `docs/architecture.md` if anything changed
