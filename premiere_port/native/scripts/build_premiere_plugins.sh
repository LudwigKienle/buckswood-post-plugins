#!/usr/bin/env bash
set -euo pipefail
export COPYFILE_DISABLE=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NATIVE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT_DIR="$(cd "$NATIVE_DIR/.." && pwd)"
ROOT_DIR="$(cd "$PORT_DIR/.." && pwd)"

DEFAULT_SDK="/Users/ludwig.kienle/Downloads/Premiere Pro 26.0 C++ SDK 2"
PREMIERE_SDK_PATH="${1:-${PREMIERE_SDK_PATH:-$DEFAULT_SDK}}"
if [[ ! -d "$PREMIERE_SDK_PATH/Examples/Headers" ]]; then
    echo "Premiere SDK headers not found: $PREMIERE_SDK_PATH" >&2
    exit 1
fi

MACOS_SDK="$(xcrun --sdk macosx --show-sdk-path)"
CLANGXX="$(xcrun -find clang++)"
REZ="$(xcrun -find Rez)"
RESMERGER="$(xcrun -find ResMerger)"
CODESIGN="$(xcrun -find codesign)"
DEVELOPER_ID_APPLICATION="${DEVELOPER_ID_APPLICATION:-}"

detect_identity() {
    /usr/bin/security find-identity -v 2>/dev/null |
        /usr/bin/awk -F '"' '$2 ~ /Developer ID Application:/ { print $2; exit }'
}

if [[ -z "$DEVELOPER_ID_APPLICATION" ]]; then
    DEVELOPER_ID_APPLICATION="$(detect_identity || true)"
fi

BUILD_DIR="$PORT_DIR/build/native_premiere"
DIST_DIR="$PORT_DIR/dist"
rm -rf "$BUILD_DIR" "$DIST_DIR"
mkdir -p "$BUILD_DIR" "$DIST_DIR" "$PORT_DIR/release"

ARCHS="${ARCHS:-arm64}"
ARCH_FLAGS=()
for arch in $ARCHS; do
    ARCH_FLAGS+=("-arch" "$arch")
done
RES_ARCH="${ARCHS%% *}"

COMMON_SOURCES=(
    "$NATIVE_DIR/src/BuckswoodPremiereNative.cpp"
    "$PORT_DIR/src/BuckswoodPremiereAdapter.cpp"
    "$ROOT_DIR/Buckswood_AI_Photorealizer/src/PhotorealizerCore.cpp"
    "$ROOT_DIR/Buckswood_Lens_Physics/src/LensPhysicsCore.cpp"
    "$ROOT_DIR/Buckswood_Fake_Diagnostic/src/FakeDiagnosticCore.cpp"
    "$ROOT_DIR/Buckswood_Film_Emulation/src/FilmEmulationCore.cpp"
    "$ROOT_DIR/Buckswood_Cinematic_Tools/src/CinematicToolsCore.cpp"
    "$ROOT_DIR/Buckswood_Look_DNA/src/LookDNACore.cpp"
)

COMMON_INCLUDES=(
    "-I$PREMIERE_SDK_PATH/Examples/Headers"
    "-I$PREMIERE_SDK_PATH/Examples/Headers/SP"
    "-I$ROOT_DIR/Buckswood_AI_Photorealizer/include"
    "-I$ROOT_DIR/Buckswood_Lens_Physics/include"
    "-I$ROOT_DIR/Buckswood_Fake_Diagnostic/include"
    "-I$ROOT_DIR/Buckswood_Film_Emulation/include"
    "-I$ROOT_DIR/Buckswood_Cinematic_Tools/include"
    "-I$ROOT_DIR/Buckswood_Look_DNA/include"
    "-I$PORT_DIR/src"
)

write_info_plist() {
    local bundle="$1"
    local executable="$2"
    local identifier="$3"
    cat > "$bundle/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>$executable</string>
    <key>CFBundleIdentifier</key>
    <string>$identifier</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$executable</string>
    <key>CFBundlePackageType</key>
    <string>BNDL</string>
    <key>CFBundleShortVersionString</key>
    <string>2.0.0</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>2.0.0</string>
    <key>CSResourcesFileMapped</key>
    <true/>
</dict>
</plist>
PLIST
}

build_effect() {
    local name="$1"
    local define="$2"
    local resource="$3"
    local identifier="$4"

    local bundle="$DIST_DIR/$name.bundle"
    mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
    write_info_plist "$bundle" "$name" "$identifier"

    "$CLANGXX" \
        -std=c++17 \
        -O2 \
        -fvisibility=hidden \
        -fvisibility-inlines-hidden \
        -fno-exceptions \
        -fno-rtti \
        -mmacosx-version-min=11.0 \
        -isysroot "$MACOS_SDK" \
        "${ARCH_FLAGS[@]}" \
        "$define" \
        "${COMMON_INCLUDES[@]}" \
        "${COMMON_SOURCES[@]}" \
        -bundle \
        -o "$bundle/Contents/MacOS/$name"

    local preprocessed_resource="$BUILD_DIR/$name.preprocessed.r"
    "$CLANGXX" \
        -E \
        -x c \
        -P \
        -DSystemSevenOrLater=1 \
        -I"$NATIVE_DIR/resources" \
        -I"$PREMIERE_SDK_PATH/Examples/Headers" \
        -I"$PREMIERE_SDK_PATH/Examples/Headers/SP" \
        -isysroot "$MACOS_SDK" \
        "$resource" \
        -o "$preprocessed_resource"

    local rez_object="$BUILD_DIR/$name.rsrc"
    "$REZ" \
        -o "$rez_object" \
        -useDF \
        -script Roman \
        -arch "$RES_ARCH" \
        -isysroot "$MACOS_SDK" \
        "$preprocessed_resource"

    "$RESMERGER" "$rez_object" -dstIs DF -o "$bundle/Contents/Resources/$name.rsrc"
    if [[ -n "$DEVELOPER_ID_APPLICATION" ]]; then
        "$CODESIGN" --force --deep --options runtime --timestamp --sign "$DEVELOPER_ID_APPLICATION" "$bundle"
    else
        "$CODESIGN" --force --deep --sign - "$bundle" >/dev/null
    fi
    "$CODESIGN" --verify --deep --strict --verbose=2 "$bundle"
    echo "Built $bundle"
}

build_effect \
    "BuckswoodAIPhotorealizer" \
    "-DBUCKSWOOD_PREMIERE_PHOTOREALIZER=1" \
    "$NATIVE_DIR/resources/BuckswoodAIPhotorealizerPiPL.r" \
    "com.buckswood.premiere.ai-photorealizer"

build_effect \
    "BuckswoodLensPhysics" \
    "-DBUCKSWOOD_PREMIERE_LENS=1" \
    "$NATIVE_DIR/resources/BuckswoodLensPhysicsPiPL.r" \
    "com.buckswood.premiere.lens-physics"

build_effect \
    "BuckswoodFakeDiagnostic" \
    "-DBUCKSWOOD_PREMIERE_FAKE_DIAGNOSTIC=1" \
    "$NATIVE_DIR/resources/BuckswoodFakeDiagnosticPiPL.r" \
    "com.buckswood.premiere.fake-diagnostic"

build_effect \
    "BuckswoodFilmEmulation" \
    "-DBUCKSWOOD_PREMIERE_FILM_EMULATION=1" \
    "$NATIVE_DIR/resources/BuckswoodFilmEmulationPiPL.r" \
    "com.buckswood.premiere.film-emulation"

build_effect \
    "BuckswoodFrameDirector" \
    "-DBUCKSWOOD_PREMIERE_FRAME_DIRECTOR=1" \
    "$NATIVE_DIR/resources/BuckswoodFrameDirectorPiPL.r" \
    "com.buckswood.premiere.frame-director"

build_effect \
    "BuckswoodRadianceRecover" \
    "-DBUCKSWOOD_PREMIERE_RADIANCE_RECOVER=1" \
    "$NATIVE_DIR/resources/BuckswoodRadianceRecoverPiPL.r" \
    "com.buckswood.premiere.radiance-recover"

build_effect \
    "BuckswoodTemporalIntegrity" \
    "-DBUCKSWOOD_PREMIERE_TEMPORAL_INTEGRITY=1" \
    "$NATIVE_DIR/resources/BuckswoodTemporalIntegrityPiPL.r" \
    "com.buckswood.premiere.temporal-integrity"

build_effect \
    "BuckswoodLookDNA" \
    "-DBUCKSWOOD_PREMIERE_LOOK_DNA=1" \
    "$NATIVE_DIR/resources/BuckswoodLookDNAPiPL.r" \
    "com.buckswood.premiere.look-dna"

/usr/bin/xattr -cr "$DIST_DIR" >/dev/null 2>&1 || true
/usr/bin/find "$DIST_DIR" -name ".DS_Store" -delete
/usr/bin/find "$DIST_DIR" -name "._*" -delete

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
    ditto -c -k --norsrc --keepParent "$DIST_DIR/$name.bundle" "$DIST_DIR/${name}_Premiere_macOS.zip"
done

PACKAGE_DIR="$PORT_DIR/release/Buckswood_Premiere_Native_macOS"
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/dist" "$PACKAGE_DIR/native/scripts"
for name in "${PLUGIN_NAMES[@]}"; do
    ditto "$DIST_DIR/$name.bundle" "$PACKAGE_DIR/dist/$name.bundle"
done
cp "$PORT_DIR/README_PREMIERE_NATIVE.md" "$PACKAGE_DIR/README_PREMIERE_NATIVE.md"
cp "$PORT_DIR/PREMIERE_EFFECT_GUIDE.md" "$PACKAGE_DIR/PREMIERE_EFFECT_GUIDE.md"
cp "$NATIVE_DIR/scripts/install_premiere_plugins.command" "$PACKAGE_DIR/native/scripts/install_premiere_plugins.command"
cp "$NATIVE_DIR/scripts/build_premiere_plugins.sh" "$PACKAGE_DIR/native/scripts/build_premiere_plugins.sh"
mkdir -p "$PACKAGE_DIR/look_dna"
cp "$ROOT_DIR/Buckswood_Look_DNA/scripts/analyze_reference.py" "$PACKAGE_DIR/look_dna/analyze_reference.py"
/usr/bin/xattr -cr "$PACKAGE_DIR" >/dev/null 2>&1 || true
/usr/bin/find "$PACKAGE_DIR" -name ".DS_Store" -delete
/usr/bin/find "$PACKAGE_DIR" -name "._*" -delete
ditto -c -k --norsrc --keepParent "$PACKAGE_DIR" "$PORT_DIR/release/Buckswood_Premiere_Native_macOS.zip"

(
    cd "$PORT_DIR/release"
    shasum -a 256 Buckswood_Premiere_Native_macOS.zip > Buckswood_Premiere_Native_macOS_SHA256SUMS.txt
)

echo "Premiere plugins are in: $DIST_DIR"
