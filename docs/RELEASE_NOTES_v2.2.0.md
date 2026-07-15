# Buckswood Post Plugins v2.2.0 Beta

This release expands the Buckswood suite to eight tools across DaVinci Resolve
and Premiere Pro, and adds the new Look DNA reference-matching workflow.

## New: Buckswood Look DNA v2

- Match footage to up to three reference profiles.
- Transfer tone, palette, semantic regions, density, local contrast and texture.
- Protect skin, highlights, gamut and scene identity.
- Use a 3x3 spatial look map and five-frame, cut-aware statistics in Resolve.
- Create portable `.bwlook` profiles from JPEG, PNG and TIFF references.

## Premiere Pro: All Eight Effects

The native Premiere package now includes:

- Fake Diagnostic
- Lens Physics
- AI Photorealizer
- Film Emulation
- Frame Director
- Radiance Recover
- Temporal Integrity
- Look DNA

macOS bundles are universal (`arm64` and `x86_64`) and Developer-ID-signed.
Windows plugins are native x64 `.prm` files with a single graphical installer.

Premiere's PiPL video-filter API supplies the current frame only. Temporal
Integrity therefore uses a documented spatial stability fallback, and Look DNA
loads portable profiles from three fixed slots instead of pretending that a
native file picker or neighboring frames are available.

## Nuke

The Nuke OpenFX ZIP now includes Fake Diagnostic, Lens Physics, AI
Photorealizer, Film Emulation, the three-effect Cinematic Tools bundle and Look
DNA for macOS and Windows.

## Downloads

- Unified Resolve installers for macOS and Windows
- Individual Film Emulation, Cinematic Tools and Look DNA macOS installers
- Nuke OpenFX package for macOS and Windows
- Premiere Pro native package for macOS and Windows
- SHA-256 manifest covering every attached release asset

These plugins remain beta software. Test important projects on duplicate media
and keep a clean bypass node or effect state for comparison.
