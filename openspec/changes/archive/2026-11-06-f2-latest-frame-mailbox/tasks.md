# Tasks

## 1. Mailbox

- [x] 1.1 `LatestMailbox<T>` triple buffer with packed atomic index + fresh bit; `static_assert` on `T`
- [x] 1.2 Unit tests: take on empty → nothing; publish then take → value; publish ×3 then take → newest; displaced values returned to the producer; counters

## 2. Concurrency

- [x] 2.1 Stress: producer at 240 fps (and unthrottled) with a consumer sleeping 5–50 ms; the consumer never sees a value older than one it already saw; `published == taken + replaced + pending`; TSan clean
- [x] 2.2 Integration with F1: frames published through the mailbox; every displaced or taken frame released exactly once; all slots free at the end
- [x] 2.3 `EXPECT_NO_ALLOCATIONS` around publish/take loops

## 3. Wrap-up

- [x] 3.1 Document in `docs/architecture.md`; note in BACKLOG that PR #1 can be closed after merge
