# Buckswood Look DNA v2.2

Reference-still look matching for OpenFX hosts. Look DNA analyzes the tonal,
chromatic, local-contrast, texture, and broad semantic character of a reference
image and transfers that character conservatively to the current shot.

The plug-in is designed for AI footage as well as camera footage. It preserves
the source composition and does not attempt to copy people, objects, lighting,
or scene content from the reference.

## Highlights

- Native Browse / Load buttons for all three reference slots in DaVinci Resolve
- Direct JPEG, PNG, TIFF, BMP, and BWLOOK loading on macOS and Windows
- Three-reference DNA blending with automatic shot compatibility weighting
- Overlapping 3x3 spatial look maps for location-sensitive transfer without hard seams
- Five-frame cut-aware analysis with selectable off, three-frame, and five-frame modes
- Independent shadow, midtone, highlight, and color-density transfer
- Portable `.bwlook` analysis profiles for repeatable looks and Nuke workflows
- Percentile-based tone matching and covariance-based palette matching
- Native semantic zones for skin, sky, foliage, warm light, shadows, and highlights
- Skin, highlight, exposure, scene-identity, and gamut protection
- Cut-aware temporal stabilization of analysis statistics without pixel blending
- Result, split, difference, confidence, protection, tone, and gamut views
- Float and byte RGBA OpenFX rendering

## Build And Test On macOS

```bash
make clean all
make companion-test
```

Install for the current user:

```bash
bash scripts/install_ofx.sh
```

Build the signed/notarized installer when Developer ID identities and the
`BuckswoodNotary` keychain profile are available:

```bash
bash scripts/build_macos_installer.sh
```

## Create A Portable Look Profile

The optional local companion requires Python 3.9+, NumPy, and Pillow:

```bash
python3 -m pip install -r requirements-companion.txt
python3 scripts/analyze_reference.py reference.jpg reference.bwlook \
  --reference-space srgb --quality high --report reference.json
```

No network service or cloud upload is used.

In DaVinci Resolve, click `Browse / Load Reference A...` directly below the
reference path. The operating-system file chooser writes the selected local file
into the path field automatically. The same workflow is available for optional
references B and C. Files stay on the workstation and are never uploaded.

## Documentation

- `DOCUMENTATION_EN.md`
- `DOCUMENTATION_DE.md`
- `LOOK_PROFILE_FORMAT.md`

## License

Original Buckswood source code is released under the repository MIT License.
