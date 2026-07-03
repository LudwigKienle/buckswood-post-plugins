#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PUBLIC_DIR="$ROOT_DIR/public_release"
BUILD_DIR="$PUBLIC_DIR/build"
STAGE_DIR="$PUBLIC_DIR/Buckswood_Resolve_Plugins"
FINAL_PKG="$PUBLIC_DIR/Buckswood_Resolve_Plugins_Installer.pkg"
FINAL_DMG="$PUBLIC_DIR/Buckswood_Resolve_Plugins.dmg"
NOTARY_PROFILE="${NOTARY_PROFILE:-BuckswoodNotary}"
DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-}"
DEVELOPER_ID_INSTALLER="${DEVELOPER_ID_INSTALLER:-}"
TEAM_ID="${TEAM_ID:-}"

die() {
    echo "Error: $*" >&2
    exit 1
}

require_identity() {
    local label="$1"
    local identity="$2"
    [[ -n "$identity" ]] || die "$label is not set."
    security find-identity -v -p codesigning | grep -F "$identity" >/dev/null || die "$label not found in keychain: $identity"
}

require_certificate() {
    local label="$1"
    local certificate="$2"
    [[ -n "$certificate" ]] || die "$label is not set."
    security find-certificate -c "$certificate" -a -Z 2>/dev/null | grep -q "SHA-1 hash" || die "$label not found in keychain: $certificate"
}

detect_application_identity() {
    security find-identity -v -p codesigning |
        awk -F '"' -v team="$TEAM_ID" '$2 ~ /Developer ID Application:/ && (team == "" || $2 ~ team) { print $2; exit }'
}

detect_installer_certificate() {
    security find-certificate -c "Developer ID Installer" -a -Z 2>/dev/null \
        | awk -F '"' -v team="$TEAM_ID" '$2 ~ /Developer ID Installer:/ && (team == "" || $2 ~ team) { print $2; exit }'
}

if [[ -z "$DEVELOPER_ID_APPLICATION" ]]; then
    DEVELOPER_ID_APPLICATION="$(detect_application_identity)"
fi
if [[ -z "$DEVELOPER_ID_INSTALLER" ]]; then
    DEVELOPER_ID_INSTALLER="$(detect_installer_certificate)"
fi

require_identity "DEVELOPER_ID_APPLICATION" "$DEVELOPER_ID_APPLICATION"
require_certificate "DEVELOPER_ID_INSTALLER" "$DEVELOPER_ID_INSTALLER"

echo "Checking notarytool profile: $NOTARY_PROFILE"
xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null

rm -rf "$BUILD_DIR" "$STAGE_DIR" "$FINAL_PKG" "$FINAL_DMG"
mkdir -p "$BUILD_DIR" "$STAGE_DIR"

echo "Building signed component packages..."
(
    cd "$ROOT_DIR/Buckswood_Lens_Physics"
    DEVELOPER_ID_APPLICATION="$DEVELOPER_ID_APPLICATION" \
    DEVELOPER_ID_INSTALLER="$DEVELOPER_ID_INSTALLER" \
    ./scripts/build_macos_installer.sh
)
(
    cd "$ROOT_DIR/Buckswood_AI_Photorealizer"
    DEVELOPER_ID_APPLICATION="$DEVELOPER_ID_APPLICATION" \
    DEVELOPER_ID_INSTALLER="$DEVELOPER_ID_INSTALLER" \
    ./scripts/build_macos_installer.sh
)

cp "$ROOT_DIR/Buckswood_Lens_Physics/release/Buckswood_Lens_Physics_Installer.pkg" "$BUILD_DIR/"
cp "$ROOT_DIR/Buckswood_AI_Photorealizer/release/Buckswood_AI_Photorealizer_Installer.pkg" "$BUILD_DIR/"

cat > "$STAGE_DIR/README.txt" <<'README'
Buckswood Resolve Plugins
=========================

Free experimental DaVinci Resolve tools for AI footage, lens physics and
cinematic look development.

Install:

1. Close DaVinci Resolve completely.
2. Double-click Buckswood_Resolve_Plugins_Installer.pkg.
3. Enter your Mac admin password when asked.
4. Restart DaVinci Resolve.

Resolve:

Color Page > OpenFX > Buckswood > Buckswood Lens Physics
Color Page > OpenFX > Buckswood AI > Buckswood AI Photorealizer
Color Page > DCTL > Buckswood_Lens_Physics_v01
Color Page > DCTL > Buckswood_AI_Photorealizer_v01
README

productbuild --synthesize \
    --package "$BUILD_DIR/Buckswood_Lens_Physics_Installer.pkg" \
    --package "$BUILD_DIR/Buckswood_AI_Photorealizer_Installer.pkg" \
    "$BUILD_DIR/distribution.xml"

productbuild \
    --distribution "$BUILD_DIR/distribution.xml" \
    --package-path "$BUILD_DIR" \
    --sign "$DEVELOPER_ID_INSTALLER" \
    "$FINAL_PKG"

pkgutil --check-signature "$FINAL_PKG"

echo "Submitting package for notarization..."
xcrun notarytool submit "$FINAL_PKG" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$FINAL_PKG"
xcrun stapler validate "$FINAL_PKG"

cp "$FINAL_PKG" "$STAGE_DIR/"
hdiutil create \
    -volname "Buckswood Resolve Plugins" \
    -srcfolder "$STAGE_DIR" \
    -ov \
    -format UDZO \
    "$FINAL_DMG" >/dev/null

codesign --force --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$FINAL_DMG"

echo "Submitting DMG for notarization..."
xcrun notarytool submit "$FINAL_DMG" --keychain-profile "$NOTARY_PROFILE" --wait
xcrun stapler staple "$FINAL_DMG"
xcrun stapler validate "$FINAL_DMG"
hdiutil verify "$FINAL_DMG"

echo
echo "Ready for public distribution:"
echo "$FINAL_DMG"
echo "$FINAL_PKG"
