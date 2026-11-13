# Tasks

- [x] 1.1 `FPE_TEST_ALLOC_GUARD` compile definition on `fpe_tests` (0 under `FPE_TSAN`, else 1)
- [x] 1.2 `alloc_guard.cpp`: global replacements only when enabled; `allocGuardAvailable()`
- [x] 1.3 `test_alloc_guard.cpp`: skip the counting tests when the guard is unavailable
- [ ] 1.4 Local runs: default build and TSan build (macOS); CI `Linux TSan` green
