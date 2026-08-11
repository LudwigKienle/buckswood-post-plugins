#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/.." && pwd)"
CXX="${CXX:-g++}"
BUILD_DIR="$ROOT_DIR/build/linux-x86-64"
RELEASE_DIR="$ROOT_DIR/release/linux"
BUNDLE_NAME="BuckswoodDeband.ofx.bundle"
BUNDLE_DIR="$RELEASE_DIR/$BUNDLE_NAME"
ZIP_PATH="$RELEASE_DIR/Buckswood_Deband_v1.0_Linux_Baselight.zip"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "This script builds a native ELF binary and must run on Linux." >&2
    exit 1
fi

rm -rf "$BUILD_DIR" "$RELEASE_DIR"
mkdir -p "$BUILD_DIR" "$BUNDLE_DIR/Contents/Linux-x86-64"

"$CXX" \
    -std=c++17 \
    -O3 \
    -fPIC \
    -fvisibility=hidden \
    -Wall \
    -Wextra \
    -I"$ROOT_DIR/include" \
    -I"$REPO_ROOT/shared" \
    -I"$REPO_ROOT/third_party/openfx/include" \
    -shared \
    "$ROOT_DIR/src/DebandCore.cpp" \
    "$ROOT_DIR/src/BuckswoodDebandOFX.cpp" \
    -o "$BUILD_DIR/BuckswoodDeband.ofx"

cp "$BUILD_DIR/BuckswoodDeband.ofx" \
    "$BUNDLE_DIR/Contents/Linux-x86-64/BuckswoodDeband.ofx"
cp "$ROOT_DIR/Info.plist" "$BUNDLE_DIR/Contents/Info.plist"
cp "$ROOT_DIR/DOCUMENTATION_EN.md" "$RELEASE_DIR/"
cp "$ROOT_DIR/DOCUMENTATION_DE.md" "$RELEASE_DIR/"

(
    cd "$RELEASE_DIR"
    zip -rq "$(basename "$ZIP_PATH")" \
        "$BUNDLE_NAME" \
        DOCUMENTATION_EN.md \
        DOCUMENTATION_DE.md
    sha256sum "$(basename "$ZIP_PATH")" \
        > Buckswood_Deband_v1.0_Linux_Baselight_SHA256SUMS.txt
)

echo "Built $ZIP_PATH"
