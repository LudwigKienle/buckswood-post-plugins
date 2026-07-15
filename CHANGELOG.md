# Changelog

## v2.1.0 - 2026-07-15

- Added Buckswood Film Emulation v2.0 with an AI-footage-first negative, print,
  texture, halation, grain, and temporal reconstruction pipeline.
- Added Buckswood Cinematic Tools v2.0: Frame Director, Radiance Recover, and
  Temporal Integrity in one multi-effect OFX bundle.
- Expanded the unified Resolve release to seven effects on macOS and Windows.
- Added signed and notarized macOS installers, Windows OFX builds, Nuke Film
  Emulation builds, companion ML adapters, documentation, and regression tests.

## Film Emulation v2.0.0 - 2026-07-15

- Upgraded `Buckswood Film Emulation` to v2.0 with AI-footage-first film finishing.
- Added Rec.709, AI Rec.709, S-Log3, LogC3, LogC4, Apple Log, and BMD Film Gen 5 / DWG input-space options.
- Added real-camera film-style presets and AI-footage-specific presets: Deplastic Film, Skin Recovery, Dream Grain, and Clean Commercial Film.
- Added process-mode separation for `Full Process`, `Color Only`, and `Texture Only`.
- Added developer controls, Push/Pull, Interlayer, Neutralize Curves, Bleach Bypass, Film Compression, Expand, CMY-style Printer Lights, tone-weighted grain, halation/bloom profiles, scratches, Film Breath, and Temporal AI Reconstruction.
- Added companion temporal backend documentation and adapter for fallback reconstruction, BasicVSR++, and external ML tools.
- Added macOS build/install scripts, documentation, OFX descriptor tests, and core smoke tests.

## Cinematic Tools v2.0.0 - 2026-07-09

- Upgraded all Cinematic Tools effects to five-frame temporal access.
- Added Subject Lock, framing confidence and shot-cut-aware smoothing to Frame Director.
- Added specular reconstruction and long-term radiance consistency.
- Added an optional HDRTVNet++ AGCM companion running on Apple MPS, CUDA or CPU.
- Added the open `BWRCV2` half-float RGB and confidence cache format.
- Added cache version, dimensions and frame-index validation in the OFX reader.
- Added ML cache result and confidence views with safe native fallback.
- Added long-term temporal consensus, repair confidence and Ghost Risk Matte to Temporal Integrity.
- Added Python cache, model inference and native cache-reader regression tests.

## Cinematic Tools v1.0.0 - 2026-07-09

- Added `Buckswood Frame Director v1.0` with seven target aspect ratios, saliency-based subject analysis, composition guides, crop mattes, manual offsets, and temporal framing smoothing.
- Added `Buckswood Radiance Recover v1.0` with floating-point highlight headroom, shadow reconstruction, dequantization, temporal consistency, clipping maps, and recovery confidence mattes.
- Added `Buckswood Temporal Integrity v1.0` with flicker and texture repair, motion protection, edge protection, chroma stabilization, error mattes, and motion mattes.
- Added a three-effect multi-plugin OFX bundle for macOS and Windows.
- Added signed and notarized macOS PKG and DMG installers.
- Added the Cinematic Tools suite to the unified Windows setup EXE and manual ZIP.
- Added core regression tests and an OFX descriptor loader test.

## v2.0.1 - 2026-07-03

- Updated `Buckswood Lens Physics` to v0.4 with an overdrive-aware edge guard for high-contrast silhouettes.
- Added `Overdrive Edge Guard`, which automatically reduces CA, fringing, bloom/coma spill, and sharpening on hard edges while preserving the stronger lens look in texture and highlights.
- Added a regression smoketest for high-overdrive hard-edge halos.
- Updated the shared Lens Physics core default used by the Premiere native port.

## v2 - 2026-07-03

- Added `Buckswood Fake Diagnostic v2.1`.
- Updated `Buckswood Lens Physics` to v0.3 with anti-ghosting and stronger silhouette protection.
- Updated `Buckswood AI Photorealizer` to v0.2 with safer adjustment-layer handling.
- Added unified macOS release DMG/ZIP.
- Added unified Windows setup EXE and manual ZIP with all three Resolve OFX plugins.
- Added Nuke OFX v2 package with Fake Diagnostic, Lens Physics, and AI Photorealizer.
- Rebuilt Premiere Pro native macOS and Windows packages.

## Earlier Builds

- Initial DCTL film-emulation experiments.
- Initial Resolve OFX builds for Lens Physics and AI Photorealizer.
- Experimental Premiere and Nuke packaging work.
