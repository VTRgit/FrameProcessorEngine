# Spec Delta

## ADDED Requirements

### Requirement: Plane layout and native buffers
A frame's planes SHALL describe, per plane, a data pointer, a row stride and a pixel stride, and SHALL be able to carry a platform-native buffer pointer for frames without CPU-visible planes.

#### Scenario: Interleaved chroma
- **WHEN** a YUV 4:2:0 frame has interleaved chroma (pixel stride 2)
- **THEN** its U and V planes report pixel stride 2 and their own data pointers

#### Scenario: Native-only frame
- **WHEN** a frame wraps a GPU-only buffer
- **THEN** it reports zero CPU planes and a non-null native buffer pointer
