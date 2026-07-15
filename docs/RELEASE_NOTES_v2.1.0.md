# Buckswood Post Plugins v2.1.0 Beta

This release expands the free Resolve collection from three plug-ins to seven
effects, with new film finishing, framing, radiance recovery, and temporal
repair tools for AI footage.

## DaVinci Resolve

The unified macOS and Windows packages contain:

- Buckswood Fake Diagnostic v2.1
- Buckswood Lens Physics v0.4
- Buckswood AI Photorealizer v0.2
- Buckswood Film Emulation v2.0
- Buckswood Frame Director v2.0
- Buckswood Radiance Recover v2.0
- Buckswood Temporal Integrity v2.0

### New: Film Emulation v2.0

- Negative and print process stages instead of a single look curve
- Real-camera and AI-footage-specific starting presets
- Full Process, Color Only, and Texture Only modes
- Film compression, printer-light trims, halation, bloom, grain, gate weave,
  film breath, damage, and output diagnostics
- Native multi-frame temporal reconstruction with an optional external ML
  companion workflow

### New: Cinematic Tools v2.0

- Frame Director with aspect-ratio guides, Subject Lock, and cut-aware framing
- Radiance Recover with floating-point highlight reconstruction and optional
  HDRTVNet++ cache input
- Temporal Integrity with five-frame repair, motion protection, confidence,
  and Ghost Risk Matte views

## Downloads

For most Resolve users:

- macOS: `Buckswood_Resolve_Plugins_v2_macOS.dmg`
- Windows: `Buckswood_Resolve_Plugins_v2_Windows_Setup.exe`

Manual ZIP packages and standalone installers are also attached. The macOS
packages are Developer ID signed and notarized. The Windows installer is not
Authenticode signed yet, so Windows SmartScreen may show a warning.

## Nuke and Premiere Pro

The Nuke OFX package now also contains Film Emulation v2.0. The Premiere Pro
native beta remains available for Lens Physics and AI Photorealizer; Film
Emulation and the temporal tools still require dedicated Premiere-native ports.

## Open Source and ML

The original Buckswood code is MIT licensed. Third-party components retain
their own licenses and attribution files. Optional companion scripts are
separate from the stable native OFX path, so every effect still has a native
fallback when no Python or ML environment is installed.

## Verification

All five macOS OFX bundles passed their native smoke tests. Film Emulation and
Cinematic Tools also passed OFX descriptor tests; Cinematic Tools additionally
passed its radiance-cache reader tests. SHA-256 checksums are attached in
`Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt`.
