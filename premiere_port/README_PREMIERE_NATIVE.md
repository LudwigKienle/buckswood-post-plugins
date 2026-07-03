# Buckswood Premiere Native Plugins

This folder contains native Adobe Premiere Pro video-filter builds for:

- Buckswood AI Photorealizer
- Buckswood Lens Physics

The plugins use Premiere's native C++ video-filter entry point (`xFilter`) and expose animatable Effect Controls through PiPL `ANIM_ParamAtom` resources.

## Built Files

- `dist/BuckswoodAIPhotorealizer.bundle`
- `dist/BuckswoodLensPhysics.bundle`
- `dist/BuckswoodAIPhotorealizer_Premiere_macOS.zip`
- `dist/BuckswoodLensPhysics_Premiere_macOS.zip`
- `release/Buckswood_Premiere_Native_macOS.zip`

## Install

Double-click:

```bash
native/scripts/install_premiere_plugins.command
```

Or copy both `.bundle` folders manually to:

```bash
/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/
```

Restart Premiere Pro after installation. The effects should appear under:

```text
Effects > Buckswood
```

## Build

```bash
native/scripts/build_premiere_plugins.sh "/Users/ludwig.kienle/Downloads/Premiere Pro 26.0 C++ SDK 2"
```

The packaged release zip is built as a universal macOS binary (`arm64` + `x86_64`). By default, a fresh local build creates Apple Silicon (`arm64`) bundles. To build for multiple macOS architectures:

```bash
ARCHS="arm64 x86_64" native/scripts/build_premiere_plugins.sh "/Users/ludwig.kienle/Downloads/Premiere Pro 26.0 C++ SDK 2"
```

## Parameters

Buckswood AI Photorealizer:

- Plastic Reduction
- Skin Realism
- Highlight Realism
- Color Naturalize
- Sensor Texture
- Micro Contrast
- Gamut Guard
- Shadow Depth
- Smoothness Breakup
- Output Mix
- Texture Seed

Buckswood Lens Physics:

- Lens Preset
- Effect Strength
- Distortion Trim
- Chromatic Aberration
- High Contrast Fringe
- Edge Coma
- Highlight Bloom
- Bloom Threshold
- Vignette
- Corner Color Cast
- Swirl
- F-Stop Sharpener
- Overdrive
- Output Mix

## Notes

This build intentionally uses a CPU-only `BGRA_4444_32f` path for stability and quality. It now mirrors the DaVinci/OpenFX parameter surface much more closely while staying inside Premiere's native video-filter API.
