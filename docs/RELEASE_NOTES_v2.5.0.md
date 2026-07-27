# Buckswood Post Plugins v2.5.0

## New: Buckswood DeJitter v1.0

- Adds a true temporal OpenFX stabilizer for DaVinci Resolve.
- Tracks a user-selected point inside an adjustable rectangular image region.
- Compares previous and next frames symmetrically so constant camera movement
  is preserved while high-frequency micro-jitter is isolated.
- Supports one- or two-frame-pair analysis, subpixel tracking, confidence
  gating, scene-cut rejection, and a maximum-correction safety limit.
- Includes Tracking Region, Motion Vector, Confidence, and Difference views.
- Includes Bicubic, Lanczos 3, and Bilinear resampling plus Auto Zoom, Reflect,
  and Edge Extend frame handling.
- Uses Resolve's host-managed thread pool and a bounded per-frame analysis
  cache.

## Packages

- Standalone signed and notarized macOS PKG/DMG
- Unified macOS Resolve suite
- Windows x64 Resolve setup EXE and manual ZIP
- Experimental Nuke OpenFX package for macOS and Windows

Premiere Pro is not included for DeJitter v1 because the current PiPL adapter
does not provide the neighboring frames required by the temporal tracker.
