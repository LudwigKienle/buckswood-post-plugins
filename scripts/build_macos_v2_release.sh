#!/usr/bin/env bash
# Assemble the unified "Buckswood Resolve Plugins v2" macOS release from the
# per-plugin installers, then build the public DMG/ZIP and refresh checksums.
# Run the three per-plugin build_macos_installer.sh scripts first.
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RELEASE_DIR="$ROOT_DIR/release"
V2_DIR="$RELEASE_DIR/Buckswood_Resolve_Plugins_v2"
VOLNAME="Buckswood Resolve Plugins v2"
DMG_PATH="$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_macOS.dmg"
ZIP_PATH="$RELEASE_DIR/Buckswood_Resolve_Plugins_v2_macOS.zip"
NOTARY_PROFILE="${NOTARY_PROFILE:-BuckswoodNotary}"

detect_identity() {
    local pattern="$1"
    /usr/bin/security find-identity -v 2>/dev/null |
        /usr/bin/awk -F '"' -v pattern="$pattern" '$2 ~ pattern { print $2; exit }'
}

DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-$(detect_identity "Developer ID Application: Ludwig Kienle")}"

can_notarize() {
    [[ -n "$NOTARY_PROFILE" ]] && xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1
}

declare -a SRC_PKGS=(
    "Buckswood_Fake_Diagnostic/release/Buckswood_Fake_Diagnostic_Installer.pkg|Buckswood_Fake_Diagnostic_v2.1_Installer.pkg"
    "Buckswood_AI_Photorealizer/release/Buckswood_AI_Photorealizer_Installer.pkg|Buckswood_AI_Photorealizer_v0.2_Installer.pkg"
    "Buckswood_Lens_Physics/release/Buckswood_Lens_Physics_Installer.pkg|Buckswood_Lens_Physics_v0.3_AntiGhosting_Installer.pkg"
)

mkdir -p "$V2_DIR/Installers" "$V2_DIR/Checksums"

for entry in "${SRC_PKGS[@]}"; do
    src="$ROOT_DIR/${entry%%|*}"
    dst="$V2_DIR/Installers/${entry##*|}"
    [[ -f "$src" ]] || { echo "Missing installer: $src" >&2; exit 1; }
    xcrun stapler validate "$src" >/dev/null 2>&1 || echo "WARNING: $src is not stapled/notarized" >&2
    cp "$src" "$dst"
done

for f in \
    "Buckswood_Fake_Diagnostic/release/Buckswood_Fake_Diagnostic_Installer_SHA256SUMS.txt" \
    "Buckswood_AI_Photorealizer/release/Buckswood_AI_Photorealizer_Installer_SHA256SUMS.txt" \
    "Buckswood_Lens_Physics/release/Buckswood_Lens_Physics_Installer_SHA256SUMS.txt"; do
    [[ -f "$ROOT_DIR/$f" ]] && cp "$ROOT_DIR/$f" "$V2_DIR/Checksums/"
done

(
    cd "$V2_DIR"
    {
        shasum -a 256 Installers/*.pkg
        [[ -d Windows ]] && shasum -a 256 Windows/*.exe Windows/*.zip 2>/dev/null
        shasum -a 256 README.md
    } > SHA256SUMS.txt
)

# Public DMG/ZIP carry only the macOS payload; Windows ships as its own
# top-level setup EXE and manual ZIP.
DMG_STAGE="$(mktemp -d)/$VOLNAME"
mkdir -p "$DMG_STAGE"
cp -R "$V2_DIR/Installers" "$V2_DIR/Checksums" "$V2_DIR/README.md" "$V2_DIR/SHA256SUMS.txt" "$DMG_STAGE/"
/usr/bin/xattr -cr "$DMG_STAGE" >/dev/null 2>&1 || true
/usr/bin/find "$DMG_STAGE" -name "._*" -delete

rm -f "$ZIP_PATH" "$DMG_PATH"
(
    cd "$(dirname "$DMG_STAGE")"
    mv "$VOLNAME" "Buckswood_Resolve_Plugins_v2"
    zip -rq "$ZIP_PATH" "Buckswood_Resolve_Plugins_v2" -x '*.DS_Store' -x '*/._*'
    mv "Buckswood_Resolve_Plugins_v2" "$VOLNAME"
)

hdiutil create -volname "$VOLNAME" -srcfolder "$DMG_STAGE" -ov -format UDZO "$DMG_PATH" >/dev/null

if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
    /usr/bin/codesign --force --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$DMG_PATH"
fi

if can_notarize; then
    echo "Submitting v2 DMG for notarization..."
    xcrun notarytool submit "$DMG_PATH" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$DMG_PATH"
    xcrun stapler validate "$DMG_PATH"
else
    echo "Skipping notarization: notarytool profile '$NOTARY_PROFILE' is not available."
fi

(
    cd "$RELEASE_DIR"
    {
        shasum -a 256 Buckswood_Resolve_Plugins_v2_macOS.dmg Buckswood_Resolve_Plugins_v2_macOS.zip
        [[ -f Buckswood_Resolve_Plugins_v2_Windows_Setup.exe ]] && shasum -a 256 Buckswood_Resolve_Plugins_v2_Windows_Setup.exe
        [[ -f Buckswood_Resolve_Plugins_v2_Windows.zip ]] && shasum -a 256 Buckswood_Resolve_Plugins_v2_Windows.zip
    } > Buckswood_Resolve_Plugins_v2_SHA256SUMS.txt

    {
        cat Buckswood_Resolve_Plugins_v2_SHA256SUMS.txt
        for f in Buckswood_Nuke_OFX_v2.zip Buckswood_Premiere_Native_macOS.zip Buckswood_Premiere_Native_Windows.zip Buckswood_Premiere_Plugins_Setup.exe; do
            [[ -f "$f" ]] && shasum -a 256 "$f"
        done
    } > Buckswood_Post_Plugins_v2_AllHosts_SHA256SUMS.txt
)

echo
echo "v2 release rebuilt:"
echo "$DMG_PATH"
echo "$ZIP_PATH"
