# Buckswood Optics Lab

`Buckswood Optics Lab` is an independent OpenFX lens and sensor finishing tool for
DaVinci Resolve. It translates the useful workflow categories of high-end Nuke lens
toolkits into a single Resolve Color Page effect without copying proprietary kernels,
presets, source code, or paid image assets.

## Included processing stages

1. Lens state: focal length, f-stop, focus distance, sensor width, anamorphic squeeze,
   and a rotatable anamorphic axis.
2. Geometry: radial distortion and focus breathing.
3. Aberrations: lateral/axial chromatic aberration, coma, astigmatism, field
   curvature, spherical aberration, and swirl.
4. Defocus: uniform focus offset or calibrated source alpha as a depth channel,
   near/far/gamma/invert controls, cat-eye shaping, and optional licensed
   aperture-image weighting.
5. Finishing: bloom, diffusion, halation, flare ghosts, anamorphic streak, starburst,
   and vignette.
6. Sensor: debayer character, chroma detail smear, and ISO-linked animated grain.
7. Assets: in-plug-in Glass, Dirt, and Smudge selectors with no path entry. The
   built-in textures are generated once, cached, and reused across frames.

Float renders preserve scene-linear/HDR values above `1.0`. The effect keeps source
alpha unchanged and includes an edge guard to reduce doubled contours.

V1.3 adds modular zero-cost stage switches, Preview/Full quality, physical scene
units, a shared procedural iris, f-stop-dependent starbursts, corrected foreground
bokeh orientation, and twelve new independently authored lens recipes. Lossless
Float32 Metal rendering remains the default macOS path; Windows keeps the optimized
Resolve worker-pool CPU fallback.

## Build and test

```bash
cd Buckswood_Optics_Lab
make clean all
make licensed-asset-test GLASS_ASSET_ROOT="$HOME/Downloads/glass"
```

The macOS build is universal (`arm64` and `x86_64`).

Build the Windows x64 OFX ZIP with:

```bash
./scripts/build_windows.sh
```

If the toolchain is outside the repository, set `LLVM_MINGW_ROOT` to its directory.

## Optional legacy licensed Glass assets

Quit DaVinci Resolve, then double-click:

```text
scripts/install_with_licensed_glass_assets.command
```

The script installs:

```text
~/Library/OFX/Plugins/BuckswoodOpticsLab.ofx.bundle
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

The v1.3 UI needs no external folder. This optional helper only preserves access to
the older 157-image local aperture library for projects that already used its legacy
indices. Those paid assets remain excluded from public releases.

On Windows, install the bundle into `C:\Program Files\Common Files\OFX\Plugins`.

## Attribution and independence

This project is not affiliated with or endorsed by Arvid Schneider, Glass, or
CG Lounge. `stb_image` is used for local PNG/JPEG decoding under its public-domain
or MIT option. See [ASSET_NOTES.md](ASSET_NOTES.md).
