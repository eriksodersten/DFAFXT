#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CMAKE_BIN="${CMAKE_BIN:-/usr/local/bin/cmake}"

if [ ! -x "$CMAKE_BIN" ]; then
    CMAKE_BIN="$(command -v cmake)"
fi

"$CMAKE_BIN" --build "$BUILD_DIR"
cp -r "$BUILD_DIR/DFAFXT_artefacts/Debug/AU/DFAFXT.component" ~/Library/Audio/Plug-Ins/Components/
cp -r "$BUILD_DIR/DFAFXT_artefacts/Debug/VST3/DFAFXT.vst3" ~/Library/Audio/Plug-Ins/VST3/
echo "✓ DFAF XT deployed"
