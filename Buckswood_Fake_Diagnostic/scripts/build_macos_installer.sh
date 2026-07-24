#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

APP_NAME="Buckswood Fake Diagnostic v2.2"
IDENTIFIER="com.buckswood.fake.diagnostic.installer"
VERSION="2.2.0"
PLUGIN_BUNDLE="BuckswoodFakeDiagnostic.ofx.bundle"
PKGROOT="$ROOT_DIR/packaging/pkgroot"
PKG_SCRIPTS="$ROOT_DIR/packaging/scripts"
RELEASE_DIR="$ROOT_DIR/release"
DMG_STAGE="$ROOT_DIR/packaging/dmg_stage"
PKG_PATH="$RELEASE_DIR/Buckswood_Fake_Diagnostic_Installer.pkg"
DMG_PATH="$RELEASE_DIR/Buckswood_Fake_Diagnostic_Installer.dmg"
NOTARY_PROFILE="${NOTARY_PROFILE:-BuckswoodNotary}"

detect_identity() {
    local pattern="$1"
    /usr/bin/security find-identity -v 2>/dev/null |
        /usr/bin/awk -F '"' -v pattern="$pattern" '$2 ~ pattern { print $2; exit }'
}

DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-$(detect_identity "Developer ID Application:")}"
DEVELOPER_ID_INSTALLER="${DEVELOPER_ID_INSTALLER:-$(detect_identity "Developer ID Installer:")}"

can_notarize() {
    [[ -n "$NOTARY_PROFILE" ]] && xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" >/dev/null 2>&1
}

echo "Building $APP_NAME installer..."

make clean
make smoketest
make bundle

if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
    /usr/bin/codesign --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$ROOT_DIR/dist/$PLUGIN_BUNDLE"
else
    /usr/bin/codesign --force --deep --sign - "$ROOT_DIR/dist/$PLUGIN_BUNDLE" >/dev/null 2>&1 || true
fi

/usr/bin/codesign --verify --deep --strict --verbose=2 "$ROOT_DIR/dist/$PLUGIN_BUNDLE"

rm -rf "$PKGROOT" "$DMG_STAGE"
mkdir -p "$PKGROOT/Library/OFX/Plugins"
mkdir -p "$DMG_STAGE" "$RELEASE_DIR"

/usr/bin/ditto --norsrc --noextattr "$ROOT_DIR/dist/$PLUGIN_BUNDLE" "$PKGROOT/Library/OFX/Plugins/$PLUGIN_BUNDLE"
chmod +x "$PKG_SCRIPTS/preinstall" "$PKG_SCRIPTS/postinstall"
/usr/bin/xattr -cr "$PKGROOT" "$DMG_STAGE" >/dev/null 2>&1 || true
/usr/bin/find "$PKGROOT" "$DMG_STAGE" -name "._*" -delete

pkgbuild \
    --root "$PKGROOT" \
    --scripts "$PKG_SCRIPTS" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    --filter '(^|/)\.DS_Store$' \
    --filter '(^|/)\._.*' \
    "$PKG_PATH"

if [[ -n "$DEVELOPER_ID_INSTALLER" ]]; then
    SIGNED_PKG_PATH="$RELEASE_DIR/Buckswood_Fake_Diagnostic_Installer_Signed.pkg"
    productsign --sign "$DEVELOPER_ID_INSTALLER" "$PKG_PATH" "$SIGNED_PKG_PATH"
    mv "$SIGNED_PKG_PATH" "$PKG_PATH"
fi

if can_notarize; then
    echo "Submitting package for notarization..."
    xcrun notarytool submit "$PKG_PATH" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG_PATH"
    xcrun stapler validate "$PKG_PATH"
else
    echo "Skipping notarization: notarytool profile '$NOTARY_PROFILE' is not available."
fi

cp "$PKG_PATH" "$DMG_STAGE/"
cp "$ROOT_DIR/packaging/DMG_README.txt" "$DMG_STAGE/README.txt"

hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$DMG_STAGE" \
    -ov \
    -format UDZO \
    "$DMG_PATH" >/dev/null

if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
    /usr/bin/codesign --force --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$DMG_PATH"
fi

if can_notarize; then
    echo "Submitting DMG for notarization..."
    xcrun notarytool submit "$DMG_PATH" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$DMG_PATH"
    xcrun stapler validate "$DMG_PATH"
fi

(
    cd "$RELEASE_DIR"
    cp "$ROOT_DIR/README.md" Buckswood_Fake_Diagnostic_README.md
    cp "$ROOT_DIR/DOCUMENTATION_DE.md" Buckswood_Fake_Diagnostic_Dokumentation_DE.md
    cp "$ROOT_DIR/DOCUMENTATION_EN.md" Buckswood_Fake_Diagnostic_Documentation_EN.md
    shasum -a 256 \
        Buckswood_Fake_Diagnostic_Installer.pkg \
        Buckswood_Fake_Diagnostic_Installer.dmg \
        > Buckswood_Fake_Diagnostic_Installer_SHA256SUMS.txt
)

echo
echo "Built:"
echo "$PKG_PATH"
echo "$DMG_PATH"
