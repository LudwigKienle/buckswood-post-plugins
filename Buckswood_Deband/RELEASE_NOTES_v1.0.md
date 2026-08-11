# Buckswood Deband v1.0

## New effect

Buckswood Deband is a cross-host OpenFX repair tool for false contours caused
by low bit depth, compression, aggressive grading, synthetic gradients, and AI
footage.

## Core pipeline

- weak-contour detection separated from reconstruction
- three spatial scales for narrow and broad bands
- edge, texture, and intentional-flat guards
- luminance-first repair with an independent chroma amount
- zero-mean gradient-targeted dither
- Float32 HDR and negative-value preservation
- exact alpha preservation

## Diagnostics

- Result
- Banding Map
- Protected Detail
- Difference x12

## Host releases

- macOS universal OFX for DaVinci Resolve and Baselight
- Windows x64 OFX for DaVinci Resolve
- Linux x86-64 OFX for Baselight

The effect does not require sequential rendering or previous-frame state.
Static and frame-indexed dither are both deterministic under out-of-order
rendering.
