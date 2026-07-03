#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="${1:-$HOME/Library/OFX/Plugins}"

make -C "$PROJECT_DIR" bundle
mkdir -p "$TARGET_DIR"
rm -rf "$TARGET_DIR/BuckswoodFakeDiagnostic.ofx.bundle"
cp -R "$PROJECT_DIR/dist/BuckswoodFakeDiagnostic.ofx.bundle" "$TARGET_DIR/BuckswoodFakeDiagnostic.ofx.bundle"
echo "Installed $TARGET_DIR/BuckswoodFakeDiagnostic.ofx.bundle"
