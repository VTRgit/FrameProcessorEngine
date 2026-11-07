# FrameProcessorEngine

Public C++20 library (MIT). It runs user-defined tasks on every frame of one or more video
streams, in parallel, and delivers one synchronized, timestamped result per frame with bounded
latency. **Only that.** Read `README.md` and `docs/architecture.md` before touching anything.

## Public repo

This repository is public. Never commit anything from private VTRsoftware repositories, and never
name them or their plans: no models, no feature extraction, no application logic, no product docs.
Examples and tests use synthetic frames only.

## Language

**Everything in this repo is in English**: code, comments, docs, ADRs, OpenSpec artifacts and
change names, commit messages, branches, issues and PRs.

## Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFPE_WARNINGS_AS_ERRORS=ON
cmake --build build && ctest --test-dir build --output-on-failure
cmake -S . -B build-asan -DFPE_SANITIZE=ON && cmake --build build-asan && ctest --test-dir build-asan
cmake -S . -B build-tsan -DFPE_TSAN=ON && cmake --build build-tsan && ctest --test-dir build-tsan
```

(Available once F0 is applied.)

## Rules (non-negotiable)

1. No allocation, free, blocking lock, I/O or logging on the frame path after `start()`. A test checks it.
2. The engine never copies pixels: preallocated slots or external buffers referenced by handle.
3. Latest frame wins: no unbounded queues; dropped frames are counted, never hidden.
4. The core knows no platforms; capture adapters live in `platform/`.
5. Offline mode is deterministic and drops nothing.
6. Every change goes through an OpenSpec change in `openspec/changes/` (see `../CLAUDE.md`), named after its backlog ID in `docs/BACKLOG.md`. Every behaviour change has its test. New design decisions → an ADR in `docs/adr/`.
