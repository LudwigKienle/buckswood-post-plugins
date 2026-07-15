#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NUKE_DIR="$ROOT_DIR/nuke_release"
PACKAGE_NAME="Buckswood_Nuke_OFX_v2"
PACKAGE_DIR="$NUKE_DIR/$PACKAGE_NAME"
ZIP_PATH="$NUKE_DIR/$PACKAGE_NAME.zip"
DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-}"

detect_identity() {
    /usr/bin/security find-identity -v 2>/dev/null |
        /usr/bin/awk -F '"' '$2 ~ /Developer ID Application:/ { print $2; exit }'
}

if [[ -z "$DEVELOPER_ID_APPLICATION" ]]; then
    DEVELOPER_ID_APPLICATION="$(detect_identity || true)"
fi

mkdir -p "$NUKE_DIR"

make -C "$ROOT_DIR/Buckswood_Fake_Diagnostic" bundle
make -C "$ROOT_DIR/Buckswood_Lens_Physics" bundle
make -C "$ROOT_DIR/Buckswood_AI_Photorealizer" bundle
make -C "$ROOT_DIR/Buckswood_Film_Emulation" bundle
make -C "$ROOT_DIR/Buckswood_Cinematic_Tools" bundle
make -C "$ROOT_DIR/Buckswood_Look_DNA" bundle

if [[ ! -d "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins" ]]; then
    bash "$ROOT_DIR/scripts/build_windows_release.sh"
fi

rm -rf "$PACKAGE_DIR" "$ZIP_PATH"
mkdir -p "$PACKAGE_DIR/macOS" "$PACKAGE_DIR/Windows"

/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_Fake_Diagnostic/dist/BuckswoodFakeDiagnostic.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodFakeDiagnostic.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_Lens_Physics/dist/BuckswoodLensPhysics.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodLensPhysics.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_AI_Photorealizer/dist/BuckswoodAIPhotorealizer.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodAIPhotorealizer.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_Film_Emulation/dist/BuckswoodFilmEmulation.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodFilmEmulation.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_Cinematic_Tools/dist/BuckswoodCinematicTools.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodCinematicTools.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/Buckswood_Look_DNA/dist/BuckswoodLookDNA.ofx.bundle" "$PACKAGE_DIR/macOS/BuckswoodLookDNA.ofx.bundle"

/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodFakeDiagnostic.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodFakeDiagnostic.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodLensPhysics.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodLensPhysics.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodAIPhotorealizer.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodAIPhotorealizer.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodFilmEmulation.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodFilmEmulation.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodCinematicTools.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodCinematicTools.ofx.bundle"
/usr/bin/ditto --norsrc "$ROOT_DIR/windows_release/Buckswood_Resolve_Plugins_Windows/OFX/Plugins/BuckswoodLookDNA.ofx.bundle" "$PACKAGE_DIR/Windows/BuckswoodLookDNA.ofx.bundle"

for bundle in "$PACKAGE_DIR"/macOS/*.ofx.bundle; do
    if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
        /usr/bin/codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$bundle"
    else
        /usr/bin/codesign --force --deep --sign - "$bundle" >/dev/null 2>&1 || true
    fi
    /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle"
done

cat > "$PACKAGE_DIR/README_NUKE_OFX.txt" <<'TXT'
Buckswood OFX for Nuke v2
=========================

Included OFX plug-ins:

- Buckswood Fake Diagnostic v2.1
- Buckswood Lens Physics v0.4 Overdrive Edge Guard
- Buckswood AI Photorealizer v0.2
- Buckswood Film Emulation v2.0
- Buckswood Frame Director v2.0
- Buckswood Radiance Recover v2.0
- Buckswood Temporal Integrity v2.0
- Buckswood Look DNA v2.0

Nuke can load OpenFX plug-ins directly. This package is experimental because
the plug-ins are primarily tested in DaVinci Resolve first.

macOS install:

Copy the .ofx.bundle folders from:

macOS/

to:

/Library/OFX/Plugins

Then restart Nuke.

Windows install:

Copy the .ofx.bundle folders from:

Windows/

to:

C:\Program Files\Common Files\OFX\Plugins

Then restart Nuke.

Alternative custom path:

Set OFX_PLUGIN_PATH to the folder that contains the .ofx.bundle folders.

Notes:

- DCTL files are Resolve-only and are not used by Nuke.
- If Nuke cached an older plug-in description, restart Nuke after replacing the bundle.
- Fake Diagnostic temporal modes depend on host frame access. Use spatial diagnostic modes if temporal access is limited.
- Look DNA V2 can blend three stills, use a 3x3 spatial map, and analyze five frames. Portable BWLOOK profiles use the compatible global mode.
TXT

cp "$ROOT_DIR/Buckswood_Look_DNA/scripts/analyze_reference.py" "$PACKAGE_DIR/"
cp "$ROOT_DIR/Buckswood_Look_DNA/requirements-companion.txt" "$PACKAGE_DIR/requirements-look-dna.txt"
cp "$ROOT_DIR/Buckswood_Look_DNA/LOOK_PROFILE_FORMAT.md" "$PACKAGE_DIR/"

/usr/bin/xattr -cr "$PACKAGE_DIR" >/dev/null 2>&1 || true
/usr/bin/find "$PACKAGE_DIR" -name ".DS_Store" -delete
/usr/bin/find "$PACKAGE_DIR" -name "._*" -delete

(
    cd "$NUKE_DIR"
    zip -r "$ZIP_PATH" "$PACKAGE_NAME" -x "*/.DS_Store" "__MACOSX/*" "*/._*" >/dev/null
    shasum -a 256 "$(basename "$ZIP_PATH")" > SHA256SUMS.txt
)

echo "Built Nuke OFX release:"
echo "$ZIP_PATH"
