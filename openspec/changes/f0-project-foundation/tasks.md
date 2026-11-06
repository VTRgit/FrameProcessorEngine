# Tasks

## 1. Build

- [ ] 1.1 Top-level `CMakeLists.txt` with target `fpe::fpe`, C++20, the five options; verify `cmake --build` on gcc and clang with `-DFPE_WARNINGS_AS_ERRORS=ON`
- [ ] 1.2 `include/fpe/version.hpp` (version macros) as the first public header; verify it compiles in a consumer test with `add_subdirectory`

## 2. Tests

- [ ] 2.1 GoogleTest via FetchContent (pinned), `tests/CMakeLists.txt`, `ctest` discovers tests
- [ ] 2.2 `tests/support/alloc_guard` + `EXPECT_NO_ALLOCATIONS`; tests: guard catches a `new`, guard ignores allocations outside scope, guard catches an allocation on another armed thread
- [ ] 2.3 ASan+UBSan and TSan builds pass locally

## 3. Example and CI

- [ ] 3.1 `examples/hello_fpe.cpp` builds and runs
- [ ] 3.2 `.github/workflows/ci.yml`: ubuntu gcc, ubuntu clang, macOS; Release+Werror, ASan+UBSan, TSan; verify green on a push

## 4. Docs

- [ ] 4.1 `docs/adr/0001-core-rules.md`; README "Build" section; verify by reading
