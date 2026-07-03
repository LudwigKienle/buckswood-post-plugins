# Buckswood AI Photorealizer

AI/CGI/phone-footage de-plastic and photoreal pass for DaVinci Resolve.

This project contains three layers:

- `dctl/Buckswood_AI_Photorealizer_v01.dctl`: immediately usable DCTL fallback.
- `src/PhotorealizerCore.cpp`: reusable C++ image-processing core.
- `src/BuckswoodAIPhotorealizerOFX.cpp`: CPU OpenFX plugin wrapper for Resolve.

The current version is intentionally conservative. It avoids temporal inference,
hallucinated detail, and heavyweight ML dependencies inside Resolve. It focuses on:

- plastic-skin reduction
- highlight softening
- gamut/saturation compression
- subtle sensor texture
- skin tone naturalization
- shadow grounding
- output mix control

## Install the DCTL

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AI_Photorealizer
./scripts/install_dctl.sh
```

Then refresh LUTs or restart Resolve and load `Buckswood_AI_Photorealizer_v01.dctl`.

## Normal macOS Installer

For a giveaway/release-style install, build the package and DMG:

```bash
scripts/build_macos_installer.sh
```

Release files:

```text
release/Buckswood_AI_Photorealizer_Installer.pkg
release/Buckswood_AI_Photorealizer_Installer.dmg
```

## Build the OFX Plugin

Apple Command Line Tools are required:

```bash
xcode-select --install
```

After the toolchain is available:

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in/Buckswood_AI_Photorealizer
make smoketest
make bundle
make install
```

The bundle will be installed to:

```text
/Library/OFX/Plugins/BuckswoodAIPhotorealizer.ofx.bundle
```

If Resolve does not pick it up, remove its OFX cache:

```bash
rm -f "$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve/OFXPluginCache.xml"
```

Then restart Resolve.

## ML Roadmap

The shipped OFX plugin is deterministic and CPU-based. The ML extension is scaffolded
through `include/MLMaskProvider.h` and `src/CoreMLMaskProvider.mm`.

Recommended next stages:

1. Train/export a tiny Core ML mask model that outputs skin, plastic, highlight,
   edge, and detail masks.
2. Run the model once per tile/frame, not per parameter, and cache masks.
3. Keep the deterministic core as a fallback when Core ML is unavailable.
4. Add Metal acceleration only after the CPU OFX is stable in Resolve.

Do not ship a PyTorch dependency inside Resolve. Use Core ML or ONNX/OpenVINO only
after the native OFX plugin is proven stable.
