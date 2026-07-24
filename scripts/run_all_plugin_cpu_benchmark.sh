#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/buckswood-cpu-benchmark.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

CXX="${CXX:-c++}"
COMMON_FLAGS=(
    -std=c++17
    -O3
    -I"$ROOT_DIR/Buckswood_Fake_Diagnostic/include"
    -I"$ROOT_DIR/Buckswood_Film_Emulation/include"
    -I"$ROOT_DIR/Buckswood_Cinematic_Tools/include"
    -I"$ROOT_DIR/Buckswood_Look_DNA/include"
)
SOURCES=(
    "$ROOT_DIR/tests/all_plugin_cpu_probe.cpp"
    "$ROOT_DIR/Buckswood_Fake_Diagnostic/src/FakeDiagnosticCore.cpp"
    "$ROOT_DIR/Buckswood_Film_Emulation/src/FilmEmulationCore.cpp"
    "$ROOT_DIR/Buckswood_Cinematic_Tools/src/CinematicToolsCore.cpp"
    "$ROOT_DIR/Buckswood_Look_DNA/src/LookDNACore.cpp"
)

"$CXX" "${COMMON_FLAGS[@]}" "${SOURCES[@]}" \
    -o "$BUILD_DIR/reference"
"$CXX" "${COMMON_FLAGS[@]}" -DBW_OPTIMIZED=1 "${SOURCES[@]}" \
    -o "$BUILD_DIR/prepared"

"$BUILD_DIR/reference" "$BUILD_DIR/reference-warm.bin" >/dev/null
"$BUILD_DIR/prepared" "$BUILD_DIR/prepared-warm.bin" >/dev/null
"$BUILD_DIR/reference" "$BUILD_DIR/reference.bin" >/dev/null
"$BUILD_DIR/prepared" "$BUILD_DIR/prepared.bin" >/dev/null

if cmp -s "$BUILD_DIR/reference.bin" "$BUILD_DIR/prepared.bin"; then
    echo "Parity: exact Float32 output match"
else
    echo "Parity check failed" >&2
    exit 1
fi

python3 - "$BUILD_DIR" <<'PY'
import pathlib
import statistics
import subprocess
import sys

build = pathlib.Path(sys.argv[1])
executables = {
    "reference": build / "reference",
    "prepared": build / "prepared",
}
samples = {
    name: {
        "fake_ms": [],
        "film_ms": [],
        "radiance_ms": [],
        "frame_director_ms": [],
        "look_dna_ms": [],
    }
    for name in executables
}

for iteration in range(10):
    order = ("reference", "prepared")
    if iteration % 2:
        order = tuple(reversed(order))
    for name in order:
        output = subprocess.check_output(
            [
                str(executables[name]),
                str(build / f"{name}-{iteration}.bin"),
            ],
            text=True,
        )
        for line in output.splitlines():
            key, separator, value = line.partition("=")
            if separator and key in samples[name]:
                samples[name][key].append(float(value))

print()
print("Median of 10 alternating warm runs (320x180 Float32):")
print(f"{'Plugin':24} {'Reference':>12} {'Prepared':>12} {'Ratio':>9}")
for key, label in (
    ("fake_ms", "Fake Diagnostic"),
    ("film_ms", "Film Emulation"),
    ("radiance_ms", "Radiance Recover"),
    ("frame_director_ms", "Frame Director"),
    ("look_dna_ms", "Look DNA pixel pass"),
):
    reference = statistics.median(samples["reference"][key])
    prepared = statistics.median(samples["prepared"][key])
    ratio = reference / prepared
    print(f"{label:24} {reference:9.3f} ms {prepared:9.3f} ms {ratio:8.2f}x")

print()
print("Look DNA/Frame Director OFX tile-cache gains are not included here.")
PY
