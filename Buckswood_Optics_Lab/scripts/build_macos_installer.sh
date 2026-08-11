#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

APP_NAME="Buckswood Optics Lab v1.3.1"
IDENTIFIER="com.buckswood.optics.lab.installer"
VERSION="1.3.1"
PLUGIN_BUNDLE="BuckswoodOpticsLab.ofx.bundle"
PKGROOT="$ROOT_DIR/packaging/pkgroot"
PKG_SCRIPTS="$ROOT_DIR/packaging/scripts"
RELEASE_DIR="$ROOT_DIR/release"
DMG_STAGE="$ROOT_DIR/packaging/dmg_stage"
PKG_PATH="$RELEASE_DIR/Buckswood_Optics_Lab_v1.3.1_Installer.pkg"
DMG_PATH="$RELEASE_DIR/Buckswood_Optics_Lab_v1.3.1_Installer.dmg"
NOTARY_PROFILE="${NOTARY_PROFILE:-BuckswoodNotary}"

detect_identity() {
    local pattern="$1"
    /usr/bin/security find-identity -v 2>/dev/null |
        /usr/bin/awk -F '"' -v pattern="$pattern" '$2 ~ pattern { print $2; exit }'
}

DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-$(detect_identity "Developer ID Application:")}"
DEVELOPER_ID_INSTALLER="${DEVELOPER_ID_INSTALLER:-$(detect_identity "Developer ID Installer:")}"

can_notarize() {
    [[ -n "$NOTARY_PROFILE" ]] &&
        xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1
}

make clean
make all

if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
    /usr/bin/codesign --force --deep --options runtime --timestamp \
        --sign "$DEVELOPER_ID_APPLICATION" "$ROOT_DIR/dist/$PLUGIN_BUNDLE"
else
    /usr/bin/codesign --force --deep --sign - "$ROOT_DIR/dist/$PLUGIN_BUNDLE"
fi
/usr/bin/codesign --verify --deep --strict --verbose=2 \
    "$ROOT_DIR/dist/$PLUGIN_BUNDLE"

rm -rf "$PKGROOT" "$DMG_STAGE"
mkdir -p \
    "$PKGROOT/Library/OFX/Plugins" \
    "$DMG_STAGE" \
    "$RELEASE_DIR"
/usr/bin/ditto --norsrc --noextattr \
    "$ROOT_DIR/dist/$PLUGIN_BUNDLE" \
    "$PKGROOT/Library/OFX/Plugins/$PLUGIN_BUNDLE"

chmod +x \
    "$PKG_SCRIPTS/preinstall" \
    "$PKG_SCRIPTS/postinstall" \
    "$ROOT_DIR/scripts/install_licensed_assets_only.command"
/usr/bin/xattr -cr "$PKGROOT" "$DMG_STAGE" >/dev/null 2>&1 || true
/usr/bin/find "$PKGROOT" "$DMG_STAGE" -name "._*" -delete

pkgbuild \
    --root "$PKGROOT" \
    --scripts "$PKG_SCRIPTS" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_PATH"

if [[ -n "$DEVELOPER_ID_INSTALLER" ]]; then
    productsign \
        --sign "$DEVELOPER_ID_INSTALLER" \
        "$PKG_PATH" \
        "$RELEASE_DIR/Buckswood_Optics_Lab_v1.3.1_Installer_Signed.pkg"
    mv \
        "$RELEASE_DIR/Buckswood_Optics_Lab_v1.3.1_Installer_Signed.pkg" \
        "$PKG_PATH"
fi

if can_notarize; then
    xcrun notarytool submit \
        "$PKG_PATH" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait
    xcrun stapler staple "$PKG_PATH"
fi

cp "$PKG_PATH" "$DMG_STAGE/"
cp "$ROOT_DIR/packaging/DMG_README.txt" "$DMG_STAGE/README.txt"
cp "$ROOT_DIR/DOCUMENTATION_DE.md" "$DMG_STAGE/"
cp "$ROOT_DIR/DOCUMENTATION_EN.md" "$DMG_STAGE/"
cp \
    "$ROOT_DIR/scripts/install_licensed_assets_only.command" \
    "$DMG_STAGE/Install_Licensed_Glass_Assets.command"
hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$DMG_STAGE" \
    -ov \
    -format UDZO \
    "$DMG_PATH" >/dev/null

if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
    /usr/bin/codesign --force --timestamp \
        --sign "$DEVELOPER_ID_APPLICATION" \
        "$DMG_PATH"
fi
if can_notarize; then
    xcrun notarytool submit \
        "$DMG_PATH" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait
    xcrun stapler staple "$DMG_PATH"
fi

(
    cd "$RELEASE_DIR"
    shasum -a 256 \
        Buckswood_Optics_Lab_v1.3.1_Installer.pkg \
        Buckswood_Optics_Lab_v1.3.1_Installer.dmg \
        > Buckswood_Optics_Lab_v1.3.1_SHA256SUMS.txt
)

echo "Built $PKG_PATH"
echo "Built $DMG_PATH"
