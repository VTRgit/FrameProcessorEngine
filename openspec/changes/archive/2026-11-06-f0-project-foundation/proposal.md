# Proposal

## Why

Backlog **F0** — "Project foundation", acceptance criteria: *"CMake project with library, tests
(GoogleTest) and example; CI on Linux (gcc, clang) and macOS with warnings as errors, ASan+UBSan
and TSan; ADR 0001 with the core rules; a test helper that fails if the frame path allocates."*

`main` holds only a README and the licence. The `v0.1.x` prototype has a CMake layout but no CI,
sanitizers only on the example, tests commented out, and non-inline function definitions in a
public header. Every later issue needs a build, tests and a way to prove "no allocation on the
frame path", so that comes first.

## What Changes

- CMake ≥ 3.21 project `fpe`, header-first library target `fpe::fpe` (C++20), options
  `FPE_WARNINGS_AS_ERRORS`, `FPE_SANITIZE` (ASan+UBSan), `FPE_TSAN`, `FPE_BUILD_TESTS`,
  `FPE_BUILD_EXAMPLES`.
- GoogleTest through `FetchContent` pinned to a tag (tests only; the library has no dependencies).
- `tests/support/alloc_guard.{h,cpp}`: replaces global `operator new/delete` in the test binary and
  fails the test if any allocation happens inside an `AllocGuard` scope.
- `examples/hello_fpe.cpp`: builds and runs (placeholder until F3).
- GitHub Actions `ci.yml`: ubuntu gcc + clang, macOS AppleClang; Release with warnings as errors;
  ASan+UBSan job; TSan job; all run ctest.
- `docs/adr/0001-core-rules.md` with the rules in `CLAUDE.md`.
- Removes nothing from `v0.1.x`; that branch stays as reference.

## Capabilities

### New Capabilities
- `build-and-ci`: how the library is built, tested and checked on every push.

### Modified Capabilities
<!-- none -->

## Impact

- New files only: `CMakeLists.txt`, `include/fpe/`, `tests/`, `examples/`, `.github/workflows/ci.yml`, `docs/adr/`.
- Does not touch the frame path (there is none yet).
