# Status

Update at the end of every work session (most recent first).

## 2026-11-06 — F0–F3, F6, F8

**Done**
- F0 foundation (CMake, GoogleTest, allocation guard, CI), F1 frame store (zero copy), F2
  latest-wins mailbox, F3 task graph engine (real time + deterministic offline), F6 Android camera
  adapter (`platform/android`, example app), F8 pinned tasks on lane threads (GPU contexts).
- 54 tests on GCC/Clang (Werror), ASan+UBSan, TSan.
- Pixel 9 Pro (example app, 640×480 YUV, 60 fps camera): 0.39 ms in FPE, 44.7 ms sensor → result
  (camera pipeline), 0 drops in 1 652 frames.

**Next**
- F4 (deadline policies, latency percentiles), F5 (coroutine tasks), F7 (Apple adapter) — after
  a consumer shows which of them it needs.
- PR #1 (v0.1.x) can be closed.

**Blockers**
- None. Public repo: nothing from private consumers goes here (models, application logic).
