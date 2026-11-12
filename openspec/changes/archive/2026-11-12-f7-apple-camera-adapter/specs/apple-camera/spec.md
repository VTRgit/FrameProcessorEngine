# Spec Delta

## ADDED Requirements

### Requirement: Zero-copy pixel buffer frames
The Apple adapter SHALL wrap a `CVPixelBufferRef` as an external frame whose planes point at the buffer's own memory, keeping the buffer retained and locked for reading for the frame's lifetime, and SHALL unlock and release it exactly once, when the last reference is released.

#### Scenario: Bi-planar 4:2:0 buffer
- **WHEN** a `420f` pixel buffer is wrapped
- **THEN** the frame reports format `NV12`, the buffer's width, height and the given timestamp, two planes with the buffer's row strides and pixel strides 1 and 2, and plane pointers equal to the buffer's plane addresses

#### Scenario: Shared owners
- **WHEN** a wrapped buffer is shared by three owners that all release it
- **THEN** the buffer is unlocked and released exactly once, after the last release

#### Scenario: Store exhausted
- **WHEN** no external slot is free
- **THEN** wrapping returns `PoolExhausted` and the buffer is not retained (the caller still owns it)

#### Scenario: GPU-only buffer
- **WHEN** a buffer in a format without CPU planes support is wrapped
- **THEN** the frame has no CPU planes and exposes the pixel buffer as its native buffer

### Requirement: Capture samples feeding an engine stream
The adapter SHALL submit the pixel buffer of each capture sample to a given engine stream with the sample's presentation time, without allocating or blocking.

#### Scenario: Samples reach the engine
- **WHEN** samples are submitted while the engine runs
- **THEN** the engine delivers results for them with their timestamps, and no pixel buffer stays retained after the engine stops
