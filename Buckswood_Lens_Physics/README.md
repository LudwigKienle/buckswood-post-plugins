# Buckswood Lens Physics

OpenFX/DCTL lens-character simulation for DaVinci Resolve.

This project is designed to add missing optical texture to clean digital or AI footage:

- radial distortion
- chromatic aberration / TCA
- high-contrast fringing
- vignette and corner color bias
- highlight bloom
- coma-like edge highlight smear
- subtle swirl / vintage edge behavior
- optional sharpness trim

## Real Lens Data

The project includes an importer for the open Lensfun lens database.

Local raw data:

```text
assets/lensfun_repo/data
```

Generated compact profiles:

```text
assets/lens_profiles/lensfun_profiles.json
assets/lens_profiles/lensfun_summary.json
```

Current imported Lensfun snapshot:

- Profiles: 1558
- Makers: 78
- Distortion profiles: 1516
- TCA profiles: 1004
- Vignetting profiles: 709

## License Note

The Lensfun database is licensed under CC BY-SA 3.0. Keep:

```text
assets/LENSFUN_ATTRIBUTION.md
assets/lensfun_repo/data/COPYING.CC_BY-SA_3.0
```

with any redistributed Lensfun-derived profile data.

Do not import commercial plugin data, Adobe lens profiles, manufacturer private tables, or scraped paid assets unless the license clearly allows redistribution.

## Refresh Lensfun Data

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_Lens_Physics
scripts/fetch_lensfun_data.sh
```

## What Lensfun Gives Us

Lensfun is useful for:

- real distortion coefficients
- real transverse chromatic aberration coefficients
- real vignetting coefficients
- focal/aperture/distance-specific calibration samples

Lensfun does not usually provide:

- artistic bloom profile
- coma shape
- bokeh kernel
- swirly edge blur kernel
- cine-lens-specific Cooke/LOMO/Asahi profiles

Those extra characteristics need either our own calibration captures or handcrafted Buckswood models.

## Build And Install

The native OFX now builds with Apple clang:

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_Lens_Physics
make clean
make smoketest
make bundle
scripts/install_ofx.sh
scripts/install_dctl.sh
```

Installed user OFX path:

```text
/Users/ludwig.kienle/Library/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle
```

DCTL fallback path:

```text
/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/Buckswood_Lens_Physics_v01.dctl
```

If Resolve does not show the plugin from the user OFX folder, install system-wide with an admin password:

```bash
sudo cp -R "/Users/ludwig.kienle/Library/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle" "/Library/OFX/Plugins/"
```

Then restart Resolve. The plugin should appear under the `Buckswood` OFX group as `Buckswood Lens Physics`.

## One-Click Installer

For normal distribution, build the macOS package and DMG:

```bash
scripts/build_macos_installer.sh
```

Release files:

```text
release/Buckswood_Lens_Physics_Installer.pkg
release/Buckswood_Lens_Physics_Installer.dmg
```

Close DaVinci Resolve, then double-click:

```text
install_buckswood_lens_physics.command
```

If macOS blocks it, right-click the file and choose `Open`.

German step-by-step guide:

```text
INSTALL_STEP_BY_STEP.md
```

## Current OFX Presets

- Neutral / Manual
- Clean Cinema Prime
- Vintage Spherical
- Petzval Swirl
- Anamorphic Soft
- Soviet Glow
- AI Deplastic Glass
- Lensfun Helios-44 58/2
- Lensfun MC Helios-44M-4 58/2
- Lensfun Zeiss Batis 25/2
- Lensfun Voigtlander Nokton 58/1.4
- Lensfun Voigtlander Nokton 28/1.5
- Dune P1 H-Series IMAX 1.43
- Dune P1 Ultra Vista 1.6x
- Dune P2 Moviecam Spherical
- Dune P2 Soviet Glass
- IMAX 15/70 50mm Sweet Spot
- IMAX 15/70 80mm Portrait
- Dune P3 Atlas Golden IMAX

## Industry Reference Presets

The Dune/IMAX presets are original Buckswood approximations based on public
interviews, manufacturer pages, and format notes. They do not copy commercial
or proprietary lens profile coefficients.

Research notes:

```text
research/IMAX_Dune_Lens_Research.md
assets/industry_profiles/imax_dune_public_profiles.json
```
