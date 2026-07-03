# Buckswood Premiere Pro Port

Premiere Pro does not load these OpenFX plug-ins directly as OFX. Nuke and
DaVinci Resolve can use the same OFX bundles, but Premiere needs a separate
Adobe plug-in build.

## Real Premiere paths

There are three practical options:

1. Adobe C++ effect plug-in
   - Best match for Lens Physics / Photorealizer.
   - Uses the After Effects / Premiere Pro SDK effect API.
   - Can share the existing C++ image-processing cores:
     - `Buckswood_Lens_Physics/src/LensPhysicsCore.cpp`
     - `Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp`

2. Premiere panel / UXP extension
   - Useful for UI, presets, applying LUTs or automation.
   - Not enough by itself for per-pixel lens physics rendering.

3. LUT-only fallback
   - Works through Lumetri.
   - Only suitable for color looks.
   - Cannot reproduce spatial effects like bloom, coma, chromatic aberration,
     distortion or edge behavior.

## Proposed Premiere plug-ins

- `Buckswood Lens Physics.aex` / platform equivalent
- `Buckswood AI Photorealizer.aex` / platform equivalent

## Porting work needed

1. Download/install the Adobe Premiere Pro / After Effects C++ SDK.
2. Create an effect plug-in wrapper around the existing C++ cores.
3. Map Adobe params to Buckswood controls.
4. Add 8-bit, 16-bit and 32-bit float render paths.
5. Build separate macOS and Windows Adobe plug-ins.
6. Package them separately from the OFX installers.

## Current status

Prepared:

- Shared C++ adapter around the existing Buckswood processing cores.
- Smoke-tested processing path independent of Adobe SDK.
- CMake project scaffold.
- Direct clang smoke-test build script.

Blocked for final `.plugin` binary:

- Adobe C++ SDK headers/libraries are not installed locally.
- The SDK must be downloaded from Adobe Developer Console.

Official notes:

- Adobe's Premiere Pro developer page lists UXP, CEP and C++ SDK paths.
- For low-level pixel effects, use the C++ SDK path, not UXP.
- Adobe's AE SDK guide says Premiere Pro supports the After Effects effect API,
  with host-specific limitations.
- Premiere SDK docs list the shared MediaCore install paths:
  - macOS: `/Library/Application Support/Adobe/Common/Plugins/7.0/MediaCore/`
  - Windows: `[Program Files]\Adobe\Common\Plugins\7.0\MediaCore\`

## Build the prepared adapter smoke test

```bash
cd /Users/ludwig.kienle/Downloads/Davinci_plug_in
./premiere_port/scripts/build_adapter_smoketest.sh
```

Expected output includes:

```text
photorealizer ...
lens ...
```

## Next build step after SDK download

1. Download the Adobe Premiere Pro C++ SDK or After Effects SDK from:
   `https://developer.adobe.com/premiere-pro/`
2. Unzip it locally, for example:
   `/Users/ludwig.kienle/Downloads/Adobe_SDKs/AfterEffectsSDK`
3. Build the Adobe wrapper against the SDK.

The current project intentionally does not vendor Adobe SDK files because those
headers are Adobe-copyrighted SDK material.
