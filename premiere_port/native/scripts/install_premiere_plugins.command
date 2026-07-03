#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DIST_DIR="$PORT_DIR/dist"
TARGET="/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"

if [[ ! -d "$DIST_DIR/BuckswoodAIPhotorealizer.bundle" || ! -d "$DIST_DIR/BuckswoodLensPhysics.bundle" ]]; then
    "$SCRIPT_DIR/build_premiere_plugins.sh"
fi

echo "Installing Buckswood Premiere plugins to:"
echo "$TARGET"
echo
echo "macOS may ask for your password because MediaCore is a system-wide Adobe plugin folder."

sudo mkdir -p "$TARGET"
sudo rm -rf "$TARGET/BuckswoodAIPhotorealizer.bundle" "$TARGET/BuckswoodLensPhysics.bundle"
sudo ditto "$DIST_DIR/BuckswoodAIPhotorealizer.bundle" "$TARGET/BuckswoodAIPhotorealizer.bundle"
sudo ditto "$DIST_DIR/BuckswoodLensPhysics.bundle" "$TARGET/BuckswoodLensPhysics.bundle"
sudo xattr -dr com.apple.quarantine "$TARGET/BuckswoodAIPhotorealizer.bundle" "$TARGET/BuckswoodLensPhysics.bundle" 2>/dev/null || true

echo
echo "Done. Restart Premiere Pro, then open Effects > Buckswood."
