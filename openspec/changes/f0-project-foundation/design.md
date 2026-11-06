# Design

## Context

Restart on `main`. Mirrors the conventions that already work in VTR Software's other C++ repos
(CMake options, sanitizer jobs, allocation guard in tests), without depending on them: FPE is public.

## Goals / Non-Goals

**Goals:** a clean, warning-free build on three compilers; sanitizers in CI from day one; a reusable
way to assert "no allocation" that every later change uses.

**Non-Goals:** any engine functionality; packaging (vcpkg/Conan) — later, once the API settles.

## Decisions

- **Header-first library, compiled target kept.** `fpe::fpe` is an `INTERFACE` target for now; it
  becomes `STATIC` when the first `.cpp` appears (worker pool in F3). Consumers link the same name.
- **GoogleTest via FetchContent** (pinned tag), only when `FPE_BUILD_TESTS=ON` (default ON when FPE is
  the top-level project, OFF when consumed with `add_subdirectory`).
- **Allocation guard:** global `operator new`/`delete` replacements in the test executable count
  allocations per thread while an `AllocGuard` is alive; `EXPECT_NO_ALLOCATIONS({...})` macro.
  Per-thread because worker threads will be checked too (F3): the guard can be armed for a set of
  threads via a thread-local flag set by the engine's test hook.
- **TSan in a separate job** (incompatible with ASan).

## Risks / Trade-offs

- [FetchContent needs network in CI] → acceptable; offline builds can pass `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST`.
