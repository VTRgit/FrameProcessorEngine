## ADDED Requirements

### Requirement: Allocation guard under sanitizers that own operator new
The test allocation guard SHALL replace the global `operator new`/`delete` only in builds where
the sanitizer runtime does not define them. In ThreadSanitizer builds (`FPE_TSAN`) the guard SHALL
report itself unavailable, count nothing, and the tests that need counting SHALL be skipped, so
the TSan build links and runs every other test.

#### Scenario: TSan build links and runs the tests
- **WHEN** the project is configured with `FPE_TSAN=ON` on Linux clang
- **THEN** `fpe_tests` links and every test runs or is skipped with a reason, none fails

#### Scenario: Allocation checks still run in the other builds
- **WHEN** the project is configured without `FPE_TSAN`
- **THEN** `allocGuardAvailable()` is true and the guard catches a heap allocation on an armed thread
