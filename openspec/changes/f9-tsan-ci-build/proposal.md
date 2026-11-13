# Proposal

## Why

Backlog **F9** — "ThreadSanitizer CI job builds", acceptance criteria:
*"The Linux TSan CI job builds and runs every test: the test allocation guard does not replace
the global operator new/delete under TSan (the sanitizer runtime owns them), allocation-counting
checks are skipped there and still run in every other job."*

The `Linux TSan` job fails at link time: `tests/support/alloc_guard.cpp` replaces the global
`operator new`/`delete`, and clang's TSan runtime on Linux (`libclang_rt.tsan_cxx`) defines them
too ("multiple definition of operator new"). So no test runs under TSan in CI, and F8's
"TSan clean" criterion is not checked there.

## What Changes

- When the existing `FPE_TSAN` option is on, the test executable is compiled with
  `FPE_TEST_ALLOC_GUARD=0` and `alloc_guard.cpp` leaves out its global replacements.
- `fpe::test::allocGuardAvailable()` tells whether counting works. Without it the guard counts
  nothing: `EXPECT_NO_ALLOCATIONS` and the engines' zero-allocation checks pass trivially, and the
  guard's own tests (which expect counts above zero) are skipped with a reason.
- Every other job (Release, ASan+UBSan, macOS, Android) keeps the allocation checks.

Test infrastructure only: no change to the library or the frame path.
