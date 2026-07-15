#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DIST_DIR="$PORT_DIR/dist"
TARGET="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"
PLUGIN_NAMES=(
    BuckswoodAIPhotorealizer
    BuckswoodLensPhysics
    BuckswoodFakeDiagnostic
    BuckswoodFilmEmulation
    BuckswoodFrameDirector
    BuckswoodRadianceRecover
    BuckswoodTemporalIntegrity
    BuckswoodLookDNA
)

for name in "${PLUGIN_NAMES[@]}"; do
    if [[ ! -d "$DIST_DIR/$name.bundle" ]]; then
        "$SCRIPT_DIR/build_premiere_plugins.sh"
        break
    fi
done

echo "Installing Buckswood Premiere plugins to:"
echo "$TARGET"
echo
echo "macOS may ask for your password because MediaCore is a system-wide Adobe plugin folder."

sudo mkdir -p "$TARGET"
for name in "${PLUGIN_NAMES[@]}"; do
    sudo rm -rf "$TARGET/$name.bundle"
    sudo ditto "$DIST_DIR/$name.bundle" "$TARGET/$name.bundle"
    sudo xattr -dr com.apple.quarantine "$TARGET/$name.bundle" 2>/dev/null || true
done

echo
echo "Done. Restart Premiere Pro, then open Effects > Buckswood."
