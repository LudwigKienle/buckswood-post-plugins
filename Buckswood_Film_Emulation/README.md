# Buckswood Film Emulation v2.0

Native OpenFX film-finishing plugin for DaVinci Resolve, built for real camera footage and AI-generated footage.

V2 is inspired by the process thinking behind high-end film systems, but it is intentionally not a measured clone of Dehancer, Genesis, or any commercial product. The differentiator is AI-footage-first finishing: the plugin combines a negative/print-style color pipeline with texture-only modes, Rec.709 AI presets, temporal shimmer reduction, and an optional external ML reconstruction path.

## What V2 Adds

- Process pipeline: developer, push/pull, negative stock, film compression, printer lights, print stock, expand.
- Color science controls: interlayer, neutralize curves, bleach bypass, color separation, color boost, subtractive color.
- Texture system: grain profiles, tone-weighted grain, chroma grain, halation profiles, bloom profiles, dust, scratches, gate weave, film breath.
- AI footage controls: `AI Footage Rec.709`, AI corrective stocks, skin protection, temporal reconstruction.
- Process modes: `Full Process`, `Color Only`, `Texture Only`.
- View modes: grain matte, halation/bloom matte, density map, temporal reconstruction map.
- Optional companion backend for BasicVSR++/RealBasicVSR-style preprocessing.

## Preset Strategy

Use `Input Space` first:

- `Rec.709 / Gamma 2.4` for normal SDR camera footage or rendered footage.
- `AI Footage Rec.709` for Midjourney, Runway, Kling, Sora-style, Veo-style, phone exports, and social media clips that are already display-referred.
- `Sony S-Log3`, `ARRI LogC3`, `ARRI LogC4`, `Apple Log`, or `BMD Film Gen 5 / DWG` when the node receives that encoded image.

Then choose a `Film / AI Stock`:

- `35mm Clean Negative` for a restrained base.
- `500T Tungsten Inspired` for night/interior warmth and stronger grain.
- `250D Daylight Inspired` for cleaner daylight.
- `16mm Documentary` for texture and movement.
- `AI Footage Deplastic Film` for the default AI cleanup look.
- `AI Skin Recovery Negative` for waxy face/skin renders.
- `AI Dream Grain` for soft generative footage that needs texture.
- `AI Clean Commercial Film` for product, fashion, and polished AI clips.
- `Large Format Clean` for subtle cinematic finishing.

Finally choose a `Print Stock`, usually:

- `AI Soft Print` for AI footage.
- `2383 Theater Print Inspired` for a stronger theatrical finish.
- `Clean Scan Print` when you want negative character without heavy print contrast.

## Recommended Defaults

For AI footage:

```text
Input Space: AI Footage Rec.709
Film / AI Stock: AI Footage Deplastic Film
Print Stock: AI Soft Print
Subtractive Color: 0.25-0.45
Film Compression: 0.25-0.55
Highlight Rolloff: 0.55-0.80
Halation: 0.05-0.18
Grain Amount: 0.10-0.28
Temporal AI Reconstruction: 0.00 first, 0.15-0.35 for AI shimmer
Output Mix: 0.55-0.85
```

For real Rec.709 camera footage:

```text
Input Space: Rec.709 / Gamma 2.4
Film / AI Stock: 35mm Clean Negative or 250D Daylight Inspired
Print Stock: 2383 Theater Print Inspired or Clean Scan Print
Grain Amount: 0.05-0.20
Temporal AI Reconstruction: 0.00
Output Mix: 0.65-1.00
```

## Build

```bash
make -C Buckswood_Film_Emulation all
```

## Install For Current User

```bash
bash Buckswood_Film_Emulation/scripts/install_ofx.sh
```

Restart DaVinci Resolve after installation.

## Optional Temporal ML Backend

The OFX plugin has a lightweight temporal stabilizer. For heavier reconstruction, use:

```bash
bash Buckswood_Film_Emulation/scripts/setup_temporal_ml_backend.sh
Buckswood_Film_Emulation/.venv_temporal_ml/bin/python \
  Buckswood_Film_Emulation/scripts/temporal_ml_reconstruct.py \
  /path/to/input_frames /path/to/output_frames --mode fallback
```

See `TEMPORAL_ML_BACKEND.md` for BasicVSR++ and external-backend notes.
