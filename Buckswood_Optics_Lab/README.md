# Buckswood Optics Lab

`Buckswood Optics Lab` is an independent OpenFX lens and sensor finishing tool for
DaVinci Resolve. It translates the useful workflow categories of high-end Nuke lens
toolkits into a single Resolve Color Page effect without copying proprietary kernels,
presets, source code, or paid image assets.

## Included processing stages

1. Lens state: focal length, f-stop, focus distance, sensor width, anamorphic squeeze.
2. Geometry: radial distortion and focus breathing.
3. Aberrations: lateral/axial chromatic aberration, coma, astigmatism, field
   curvature, spherical aberration, and swirl.
4. Defocus: uniform focus offset or source alpha as a depth channel, cat-eye shaping,
   and optional licensed aperture-image weighting.
5. Finishing: bloom, diffusion, halation, flare ghosts, anamorphic streak, starburst,
   and vignette.
6. Sensor: debayer character, chroma detail smear, and animated sensor grain.
7. Assets: directory browser plus aperture and dirt indices. Images are decoded once
   and cached for subsequent frames.

Float renders preserve scene-linear/HDR values above `1.0`. The effect keeps source
alpha unchanged and includes an edge guard to reduce doubled contours.

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

## Install with locally licensed Glass assets

Quit DaVinci Resolve, then double-click:

```text
scripts/install_with_licensed_glass_assets.command
```

The script installs:

```text
~/Library/OFX/Plugins/BuckswoodOpticsLab.ofx.bundle
~/Library/Application Support/Buckswood/OpticsLab/GlassAssets
```

It copies the aperture and dirt images only on the licensed user's machine. Those
paid assets are intentionally excluded from this repository and from public releases.

On Windows, install the bundle into `C:\Program Files\Common Files\OFX\Plugins` and
select the licensed Glass folder with `Licensed Glass Asset Folder` inside Resolve.

## Attribution and independence

This project is not affiliated with or endorsed by Arvid Schneider, Glass, or
CG Lounge. `stb_image` is used for local PNG/JPEG decoding under its public-domain
or MIT option. See [ASSET_NOTES.md](ASSET_NOTES.md).
