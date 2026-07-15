#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN_NAME="BuckswoodCinematicTools.ofx.bundle"
SOURCE="$ROOT_DIR/dist/$PLUGIN_NAME"
DESTINATION="${1:-$HOME/Library/OFX/Plugins}"

make -C "$ROOT_DIR" bundle
mkdir -p "$DESTINATION"
rm -rf "$DESTINATION/$PLUGIN_NAME"
cp -R "$SOURCE" "$DESTINATION/$PLUGIN_NAME"

echo "Installed $DESTINATION/$PLUGIN_NAME"
echo "Restart DaVinci Resolve so the OFX cache is refreshed."
