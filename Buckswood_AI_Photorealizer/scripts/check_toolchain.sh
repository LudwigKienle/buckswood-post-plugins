#!/usr/bin/env bash
set -euo pipefail

echo "Checking Apple C++ toolchain..."
if ! command -v clang++ >/dev/null 2>&1; then
  echo "clang++ was not found."
  exit 1
fi

if ! clang++ --version >/dev/null 2>&1; then
  echo "clang++ exists but cannot run. Install Apple's Command Line Tools:"
  echo "  xcode-select --install"
  exit 2
fi

echo "clang++ OK"
clang++ --version | head -1
