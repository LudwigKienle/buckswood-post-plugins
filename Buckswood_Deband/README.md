# Buckswood Deband v1.0

Cross-host OpenFX debanding for DaVinci Resolve and FilmLight Baselight.

The effect detects weak false contours in low-detail gradients, reconstructs
the underlying gradient at three spatial scales, protects real edges and
texture, and adds deterministic high-frequency dither only where it helps.

## Highlights

- Float32 pipeline with unclipped HDR and negative-value handling
- 8-bit host support for compatibility
- Luma-first repair with optional chroma reconstruction
- Edge and texture guards for text, silhouettes, grain, skin, and fabric
- Intentional-flat guard so graphic fills are not automatically textured
- Banding Map, Protected Detail, and Difference diagnostic views
- Static deterministic dither for frame-order-independent Baselight renders
- Frame-indexed deterministic dither as an optional moving pattern
- Host-managed CPU parallelism with no hidden frame cache

## Hosts

- DaVinci Resolve on macOS and Windows
- Baselight on macOS and Linux through standard OpenFX
- Other OFX 1.x hosts may work but are not part of the v1 test matrix

Baselight renders frames independently and may render them out of order.
Buckswood Deband therefore produces every frame from only the current source,
the current parameter values, and the requested OFX time. It never relies on
previous-frame state.

## Build

```bash
make -C Buckswood_Deband all benchmark
```

On Linux, build the Baselight bundle with:

```bash
bash Buckswood_Deband/scripts/build_linux_baselight.sh
```

On macOS, build the PKG and DMG with:

```bash
bash Buckswood_Deband/scripts/build_macos_installer.sh
```

On macOS with `llvm-mingw` installed, build Windows with:

```bash
bash Buckswood_Deband/scripts/build_windows.sh
```

## License and research

The plugin code is an independent implementation under this repository's MIT
license. No GPL/LGPL or unlicensed project source is copied into the binary.
See [research/OPEN_SOURCE_SURVEY.md](research/OPEN_SOURCE_SURVEY.md).
