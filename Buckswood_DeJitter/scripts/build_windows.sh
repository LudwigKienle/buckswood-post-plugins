#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/.." && pwd)"
TOOLCHAIN_DIR="${LLVM_MINGW_ROOT:-$REPO_ROOT/third_party/toolchains/llvm-mingw-20260616-ucrt-macos-universal}"
CXX="$TOOLCHAIN_DIR/bin/clang++"
BUILD_DIR="$ROOT_DIR/build/windows"
RELEASE_DIR="$ROOT_DIR/release/windows"
BUNDLE_NAME="BuckswoodDeJitter.ofx.bundle"
BUNDLE_DIR="$RELEASE_DIR/$BUNDLE_NAME"
ZIP_PATH="$RELEASE_DIR/Buckswood_DeJitter_v1.2_Windows.zip"

[[ -x "$CXX" ]] || {
    echo "Missing Windows cross-compiler: $CXX" >&2
    echo "Set LLVM_MINGW_ROOT to the llvm-mingw toolchain directory." >&2
    exit 1
}

rm -rf "$BUILD_DIR" "$RELEASE_DIR"
mkdir -p "$BUILD_DIR" "$BUNDLE_DIR/Contents/Win64"

"$CXX" \
    --target=x86_64-w64-windows-gnu \
    -std=c++17 \
    -O3 \
    -DWIN32 \
    -D_WINDOWS \
    -static \
    -static-libgcc \
    -static-libstdc++ \
    -I"$ROOT_DIR/include" \
    -I"$REPO_ROOT/shared" \
    -I"$REPO_ROOT/third_party/openfx/include" \
    -shared \
    "$ROOT_DIR/src/DeJitterCore.cpp" \
    "$ROOT_DIR/src/DeJitterOverlay.cpp" \
    "$ROOT_DIR/src/BuckswoodDeJitterOFX.cpp" \
    -o "$BUILD_DIR/BuckswoodDeJitter.ofx"

cp "$BUILD_DIR/BuckswoodDeJitter.ofx" \
    "$BUNDLE_DIR/Contents/Win64/BuckswoodDeJitter.ofx"
cp "$ROOT_DIR/Info.plist" "$BUNDLE_DIR/Contents/Info.plist"
cp "$ROOT_DIR/DOCUMENTATION_EN.md" "$RELEASE_DIR/"

(
    cd "$RELEASE_DIR"
    rm -f "$(basename "$ZIP_PATH")"
    zip -r "$(basename "$ZIP_PATH")" \
        "$BUNDLE_NAME" \
        DOCUMENTATION_EN.md >/dev/null
    shasum -a 256 "$(basename "$ZIP_PATH")" > SHA256SUMS.txt
)

echo "Built $ZIP_PATH"
