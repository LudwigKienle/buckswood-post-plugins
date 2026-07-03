# Buckswood Post Plugins

Free cinematic post-production tools for DaVinci Resolve, with experimental Premiere Pro and Nuke packages.

This repository contains the source for three main Buckswood Resolve tools:

- **Buckswood Fake Diagnostic v2.1** - a supervisor-style diagnostic overlay for AI, CG, and compositing shots that feel fake.
- **Buckswood Lens Physics v0.4** - lens-character simulation with overdrive edge protection, chromatic aberration, bloom, coma, vignette, and IMAX/Dune-inspired presets.
- **Buckswood AI Photorealizer v0.2** - a conservative de-plastic and photoreal assist pass for AI/CG/phone footage.

The primary release target is DaVinci Resolve/OpenFX. Premiere Pro and Nuke builds are packaged as separate host downloads.

Experimental adjacent work is kept in separate folders:

- `premiere_port/` - native Premiere adapter experiments.
- `Buckswood_AutoGrade_Assistant/` - companion script concept for AI-assisted grading recipes.

## Host Status

| Host | Package | Status |
| --- | --- | --- |
| DaVinci Resolve | OpenFX + DCTL installers | Stable v2 target |
| Nuke | OpenFX bundle ZIP | Experimental, uses the same OFX cores |
| Premiere Pro | Native `.prm` / `.bundle` plugins | Beta, separate native port |

## Downloads

Use GitHub Releases for public distribution once the repository is published.
Current local v2 release files are built in:

```text
release/
```

Recommended public assets:

- `Buckswood_Resolve_Plugins_v2_macOS.dmg`
- `Buckswood_Resolve_Plugins_v2_macOS.zip`
- `Buckswood_Resolve_Plugins_v2_Windows_Setup.exe`
- `Buckswood_Resolve_Plugins_v2_Windows.zip`
- `Buckswood_Nuke_OFX_v2.zip`
- `Buckswood_Premiere_Native_macOS.zip`
- `Buckswood_Premiere_Native_Windows.zip`
- `Buckswood_Premiere_Plugins_Setup.exe`
- `Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt`

## Build

macOS smoke tests:

```bash
make -C Buckswood_Fake_Diagnostic smoketest
make -C Buckswood_Lens_Physics smoketest
make -C Buckswood_AI_Photorealizer smoketest
```

macOS bundles:

```bash
make -C Buckswood_Fake_Diagnostic bundle
make -C Buckswood_Lens_Physics bundle
make -C Buckswood_AI_Photorealizer bundle
```

Windows Resolve package from macOS:

```bash
bash scripts/build_windows_release.sh
```

Nuke OFX package:

```bash
bash scripts/build_nuke_ofx_release.sh
```

Premiere native packages require the Adobe Premiere Pro C++ SDK:

```bash
bash premiere_port/native/scripts/build_premiere_plugins.sh "/path/to/Premiere Pro C++ SDK"
bash premiere_port/native/scripts/build_premiere_windows_plugins.sh "/path/to/Premiere Pro C++ SDK"
```

That script downloads nothing by itself. It expects the llvm-mingw toolchain at:

```text
third_party/toolchains/llvm-mingw-20260616-ucrt-macos-universal
```

The GitHub Actions release workflow downloads that toolchain automatically.

## Distribution Notes

Do not commit local build outputs, notarization credentials, private LUT packs, rendered videos, or the local Windows toolchain. Those are intentionally excluded by `.gitignore`.

For GitHub, publish clean source in the repository and attach installers to GitHub Releases.

## License

License is not finalized yet. Before making the repository public, choose one of:

- MIT or Apache-2.0 for open-source distribution.
- Source-available freeware terms if the plugins should be free to use but not freely resold or rebranded.

Lensfun-derived profile data requires attribution. Keep `Buckswood_Lens_Physics/assets/LENSFUN_ATTRIBUTION.md` with any redistributed Lensfun-derived files.
