#!/usr/bin/env bash
set -euo pipefail

GLASS_SOURCE="${GLASS_SOURCE:-$HOME/Downloads/glass}"
ASSET_TARGET="$HOME/Library/Application Support/Buckswood/OpticsLab/GlassAssets"

if [[ ! -d "$GLASS_SOURCE/apertures" || ! -d "$GLASS_SOURCE/dirt_textures" ]]; then
    if command -v osascript >/dev/null 2>&1; then
        SELECTED="$(
            osascript -e \
                'POSIX path of (choose folder with prompt "Choose your licensed Glass folder")' \
                2>/dev/null || true
        )"
        GLASS_SOURCE="${SELECTED%/}"
    fi
fi

if [[ ! -d "$GLASS_SOURCE/apertures" || ! -d "$GLASS_SOURCE/dirt_textures" ]]; then
    echo "No compatible licensed Glass folder was selected."
    echo "Expected subfolders: apertures and dirt_textures"
    read -r -p "Press Return to close..."
    exit 1
fi

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
    if [[ -f "$GLASS_SOURCE/dirt_textures/$texture" ]]; then
        /usr/bin/ditto \
            "$GLASS_SOURCE/dirt_textures/$texture" \
            "$ASSET_TARGET/dirt_textures/$texture"
    fi
done

echo
echo "Licensed assets installed locally:"
echo "  $ASSET_TARGET"
echo
echo "Restart DaVinci Resolve before using the asset controls."
read -r -p "Press Return to close..."
