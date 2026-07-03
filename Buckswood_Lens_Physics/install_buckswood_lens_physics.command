#!/usr/bin/env bash
set -euo pipefail

PATH="/usr/bin:/bin:/usr/sbin:/sbin:/opt/homebrew/bin:/usr/local/bin:$PATH"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_NAME="BuckswoodLensPhysics"
BUNDLE_NAME="${PLUGIN_NAME}.ofx.bundle"
USER_OFX_DIR="$HOME/Library/OFX/Plugins"
USER_BUNDLE="$USER_OFX_DIR/$BUNDLE_NAME"
SYSTEM_OFX_DIR="/Library/OFX/Plugins"
SYSTEM_BUNDLE="$SYSTEM_OFX_DIR/$BUNDLE_NAME"
DCTL_SRC="$ROOT_DIR/dctl/Buckswood_Lens_Physics_v01.dctl"
DCTL_DIR="/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT"
DCTL_DST="$DCTL_DIR/Buckswood_Lens_Physics_v01.dctl"
CACHE_DIR="$HOME/Library/Application Support/Blackmagic Design/DaVinci Resolve"

clear
echo "Buckswood Lens Physics Installer"
echo "================================="
echo
echo "Project: $ROOT_DIR"
echo

if ! command -v make >/dev/null 2>&1; then
    echo "Fehler: 'make' wurde nicht gefunden."
    echo "Installiere zuerst die Apple Command Line Tools:"
    echo "xcode-select --install"
    read -r -p "Enter zum Schliessen..."
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "Fehler: 'clang++' wurde nicht gefunden."
    echo "Installiere zuerst die Apple Command Line Tools:"
    echo "xcode-select --install"
    read -r -p "Enter zum Schliessen..."
    exit 1
fi

echo "1/6 Build + Smoke-Test..."
cd "$ROOT_DIR"
make smoketest
make bundle
echo

echo "2/6 OFX in deinen User-Plugin-Ordner installieren..."
mkdir -p "$USER_OFX_DIR"
rm -rf "$USER_BUNDLE"
cp -R "$ROOT_DIR/dist/$BUNDLE_NAME" "$USER_BUNDLE"
echo "Installiert: $USER_BUNDLE"
echo

echo "3/6 Plugin ad-hoc signieren..."
if codesign --force --deep --sign - "$USER_BUNDLE" >/dev/null 2>&1; then
    echo "Signatur OK."
else
    echo "Warnung: codesign ist fehlgeschlagen. Resolve kann das Plugin trotzdem eventuell laden."
fi
echo

echo "4/6 DCTL-Fallback installieren..."
if mkdir -p "$DCTL_DIR" 2>/dev/null && cp "$DCTL_SRC" "$DCTL_DST" 2>/dev/null; then
    echo "Installiert: $DCTL_DST"
else
    echo "DCTL-Systemordner braucht Admin-Rechte. macOS fragt jetzt nach deinem Passwort."
    sudo mkdir -p "$DCTL_DIR"
    sudo cp "$DCTL_SRC" "$DCTL_DST"
    echo "Installiert: $DCTL_DST"
fi
echo

echo "5/6 Resolve OFX-Cache leeren..."
rm -f "$CACHE_DIR/OFXPluginCache.xml" \
      "$CACHE_DIR/OFXPluginCacheV2.xml" \
      "$CACHE_DIR/OFXPluginCacheV3.xml" 2>/dev/null || true
echo "Cache geleert, falls vorhanden."
echo

echo "6/6 Optional systemweit installieren"
echo "Normalerweise reicht der User-Ordner. Falls Resolve das Plugin nicht findet,"
echo "installiere es systemweit nach /Library/OFX/Plugins."
echo
read -r -p "OFX auch systemweit installieren? [y/N] " INSTALL_SYSTEM
case "$INSTALL_SYSTEM" in
    y|Y|j|J)
        echo "macOS fragt jetzt nach deinem Admin-Passwort."
        sudo mkdir -p "$SYSTEM_OFX_DIR"
        sudo rm -rf "$SYSTEM_BUNDLE"
        sudo cp -R "$USER_BUNDLE" "$SYSTEM_BUNDLE"
        sudo codesign --force --deep --sign - "$SYSTEM_BUNDLE" >/dev/null 2>&1 || true
        echo "Systemweit installiert: $SYSTEM_BUNDLE"
        ;;
    *)
        echo "Systemweite Installation uebersprungen."
        ;;
esac

echo
echo "Fertig."
echo "Jetzt DaVinci Resolve komplett schliessen und neu starten."
echo "OFX: Color Page > OpenFX > Buckswood > Buckswood Lens Physics"
echo "DCTL: Color Page > DCTL > Buckswood_Lens_Physics_v01"
echo
read -r -p "Enter zum Schliessen..."
