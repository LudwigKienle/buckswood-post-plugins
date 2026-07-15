# Buckswood Film Emulation v2.0

`Buckswood Film Emulation` is a native OpenFX film-finishing plugin for DaVinci Resolve. It is designed for two use cases: giving real camera footage a controlled negative/print finish, and making AI-generated footage feel less plastic, less digital, and more photographically coherent.

It is not a measured clone of Dehancer or Genesis. The goal of V2 is different: a practical, open Buckswood tool that combines film-process controls with AI-footage repair controls.

## Recommended Node Order

```text
1. Technical correction / CST / base balance
2. Buckswood Photorealizer or Lens Physics if needed
3. Buckswood Film Emulation
4. Shot matching / small secondaries
```

If the plugin receives Log footage, select the matching `Input Space`. If the clip is already display-referred, use `Rec.709 / Gamma 2.4` or `AI Footage Rec.709`.

## V2 Pipeline

The internal image path is:

```text
Input Decode
Temporal Reconstruction
Exposure / Push-Pull / Film Breath
Film Developer
Negative Stock
Film Compression
Halation / Bloom
Printer Lights
Print Stock
Expand / Rolloff
Grain / Damage / Gate Weave
Output Encode
```

This matters because the controls are meant to behave like stages, not like random sliders.

## Main Controls

`Input Space` controls how the plugin interprets image values internally.

`Film / AI Stock` is the main negative-style look. The AI presets are corrective:

- `AI Footage Deplastic Film`: default for generative footage.
- `AI Skin Recovery Negative`: gentler handling for faces and skin.
- `AI Dream Grain`: texture and motion for overly clean AI imagery.
- `AI Clean Commercial Film`: polished product, fashion, architecture, and commercial clips.

`Print Stock` is the second image-formation stage. For AI footage, `AI Soft Print` is usually the safest start.

`Process Mode` changes what the plugin outputs:

- `Full Process`: color pipeline plus optical texture.
- `Color Only`: developer, stock, compression, printer lights, and print without grain/halation/damage.
- `Texture Only`: halation, bloom, grain, dust, scratches, gate weave, and breath without print color shaping.

`Push / Pull` shifts exposure and development energy. Positive values feel denser and more stressed; negative values feel softer and more open.

`Film Compression` redistributes hard digital highlights into a softer film-like shoulder. Use this before pushing bloom or halation.

`Printer Cyan / Magenta / Yellow` is the analog-style color trim. Small moves are better than big ones.

`Subtractive Color` makes color feel denser and less digital. For AI footage, 0.25 to 0.45 is usually enough.

`Halation` and `Bloom` add analog light behavior around bright areas. Keep them low for realistic results.

`Grain Amount`, `Grain Profile`, and `Grain Chroma` control the film texture. AI footage often benefits from fine grain more than heavy grain.

`Temporal AI Reconstruction` blends neighbor frames with a motion guard. It is off by default because multi-frame rendering is heavier. Start at 0.15 to 0.35 only when AI footage has shimmer, flicker, or unstable micro-texture.

`Skin Protect` prevents skin from being pushed too hard by temperature, tint, subtractive color, and color separation.

`Output Mix` is the safety control. If the look is good but too strong, lower Mix first.

## View Modes

- `Final Image`: normal output.
- `Grain Matte`: generated grain pattern.
- `Halation / Bloom Matte`: where optical glow is active.
- `Density Map`: how strongly density and print stages affect the image.
- `Temporal Reconstruction Map`: blue means little temporal change; bright values show where the temporal pass is changing the image.

## Good Starting Point For AI Footage

```text
Input Space: AI Footage Rec.709
Film / AI Stock: AI Footage Deplastic Film
Print Stock: AI Soft Print
Subtractive Color: 0.34
Film Compression: 0.38
Highlight Rolloff: 0.62
Halation: 0.08-0.16
Grain Amount: 0.12-0.24
Temporal AI Reconstruction: 0.00 first, then 0.15-0.35 if shimmer is visible
Skin Protect: 0.70
Output Mix: 0.65-0.80
```

## How It Stands Apart

Compared with classic film-emulation tools, V2 is less about offering hundreds of measured film identities and more about fast, useful correction for modern footage. Its strongest area is generative footage: temporal shimmer, waxy skin, digital highlights, clean texture, and plastic color.

For deeper temporal repair, use the companion `scripts/temporal_ml_reconstruct.py` before or after Resolve. That script can run a lightweight fallback method or call an external BasicVSR++-style backend.
