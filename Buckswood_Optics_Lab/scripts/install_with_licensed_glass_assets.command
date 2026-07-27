#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GLASS_SOURCE="${GLASS_SOURCE:-$HOME/Downloads/glass}"
PLUGIN_TARGET="$HOME/Library/OFX/Plugins/BuckswoodOpticsLab.ofx.bundle"
ASSET_TARGET="$HOME/Library/Application Support/Buckswood/OpticsLab/GlassAssets"

if [[ ! -d "$GLASS_SOURCE/apertures" || ! -d "$GLASS_SOURCE/dirt_textures" ]]; then
    echo "Glass assets were not found at:"
    echo "  $GLASS_SOURCE"
    echo
    echo "Set GLASS_SOURCE to your licensed Glass folder and run again."
    exit 1
fi

make -C "$ROOT_DIR" clean all

mkdir -p "$(dirname "$PLUGIN_TARGET")" "$ASSET_TARGET"
rm -rf "$PLUGIN_TARGET"
/usr/bin/ditto "$ROOT_DIR/dist/BuckswoodOpticsLab.ofx.bundle" "$PLUGIN_TARGET"

rm -rf "$ASSET_TARGET/apertures" "$ASSET_TARGET/dirt_textures"
mkdir -p "$ASSET_TARGET/apertures" "$ASSET_TARGET/dirt_textures"
/usr/bin/ditto "$GLASS_SOURCE/apertures" "$ASSET_TARGET/apertures"

for texture in \
    acg_fingerprints_01.png \
    acg_fingerprints_02.png \
    acg_fingerprints_03.png \
    acg_imperfections_01.png \
    acg_imperfections_02.png \
    acg_imperfections_03.png \
    acg_imperfections_04.png \
    acg_imperfections_05.png; do
    /usr/bin/ditto \
        "$GLASS_SOURCE/dirt_textures/$texture" \
        "$ASSET_TARGET/dirt_textures/$texture"
done

/usr/bin/codesign --force --deep --sign - "$PLUGIN_TARGET"

echo
echo "Installed:"
echo "  $PLUGIN_TARGET"
echo "Licensed local assets:"
echo "  $ASSET_TARGET"
echo
echo "Restart DaVinci Resolve, then open:"
echo "  Effects > Buckswood > Buckswood Optics Lab v1.1"
