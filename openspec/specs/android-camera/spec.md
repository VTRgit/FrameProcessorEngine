# android-camera Specification

## Purpose

See the archived change that introduced it: `changes/archive/2026-11-06-f6-android-camera-adapter`.

## Requirements

### Requirement: Zero-copy AImage frames
The Android adapter SHALL wrap an `AImage` as an external frame whose planes point at the image's own memory, and SHALL delete the image exactly once, when the last reference is released.

#### Scenario: YUV image
- **WHEN** a `YUV_420_888` image is wrapped
- **THEN** the frame reports format `YUV420`, the image's width, height and timestamp, three planes with the image's row and pixel strides, and plane pointers equal to the image's plane data

#### Scenario: Shared owners
- **WHEN** a wrapped image is shared by three owners that all release it
- **THEN** `AImage_delete` is called exactly once, after the last release

#### Scenario: Store exhausted
- **WHEN** no external slot is free
- **THEN** wrapping returns `PoolExhausted` and the image is not deleted (the caller still owns it)

#### Scenario: GPU-only image
- **WHEN** a `PRIVATE` image is wrapped
- **THEN** the frame has no CPU planes and exposes the image's hardware buffer as its native buffer

### Requirement: Hardware buffer frames
The adapter SHALL wrap an `AHardwareBuffer` as a frame, acquiring it on wrap and releasing it exactly once at the end.

#### Scenario: Balanced references
- **WHEN** a hardware buffer is wrapped and the frame is released
- **THEN** the buffer has received exactly one acquire and one release from the adapter

### Requirement: Camera target surface feeding an engine stream
The adapter SHALL provide an image reader whose window (or Java Surface) can be used as a camera output target, and SHALL submit each newest image to a given engine stream without allocating.

#### Scenario: Images reach the engine
- **WHEN** the camera produces images into the reader's window while the engine runs
- **THEN** the engine delivers results for those images with their timestamps, and the count of outstanding images returns to zero after the engine stops

#### Scenario: Allocation-free callback
- **WHEN** the image-available callback runs
- **THEN** the adapter performs no heap allocation

### Requirement: Example on device
An example Android app SHALL show the frame rate, latency and dropped frames of camera frames processed by the engine, and SHALL build with the NDK in CI.

#### Scenario: CI build
- **WHEN** CI runs
- **THEN** the example app builds with the NDK
