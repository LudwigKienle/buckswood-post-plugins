# Buckswood Post Plugins v2.7.1

This focused update makes Buckswood DeJitter easier to place and tune directly
inside the DaVinci Resolve viewer.

## Buckswood DeJitter v1.2

- Native OpenFX Draw Suite overlay in supporting hosts.
- Drag the center cross or rectangle interior to move the tracking point.
- Drag left or right handles to resize width.
- Drag top or bottom handles to resize height.
- Drag corner handles to resize width and height together.
- `Show Viewer Tracking Area` toggles the viewer controls.
- Existing numeric and Fine X/Y controls remain available for exact placement.
- Overlay controls do not appear in renders.

The rectangle selects the rigid image region used to estimate high-frequency
camera motion. It is not a local effect mask: DeJitter stabilizes the full
frame. Use a Resolve node mask when only part of the image should be affected.

## Packages

- Signed and notarized universal macOS PKG and DMG.
- Windows x64 OpenFX ZIP.
- SHA-256 checksum files for both platforms.
