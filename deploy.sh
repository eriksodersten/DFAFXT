#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CMAKE_BIN="${CMAKE_BIN:-/usr/local/bin/cmake}"
AU_BUNDLE="$BUILD_DIR/DFAFXT_artefacts/AU/DFAF XT.component"
VST3_BUNDLE="$BUILD_DIR/DFAFXT_artefacts/VST3/DFAF XT.vst3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components/DFAF XT.component"
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3/DFAF XT.vst3"

if [ ! -x "$CMAKE_BIN" ]; then
    CMAKE_BIN="$(command -v cmake)"
fi

for required_tool in "$CMAKE_BIN" ditto; do
    if ! command -v "$required_tool" >/dev/null 2>&1; then
        echo "Missing required tool: $required_tool" >&2
        exit 1
    fi
done

mkdir -p "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"

"$CMAKE_BIN" -S "$PROJECT_ROOT" -B "$BUILD_DIR"
"$CMAKE_BIN" --build "$BUILD_DIR" --target DFAFXT_AU DFAFXT_VST3

if [ ! -d "$AU_BUNDLE" ]; then
    echo "AU build did not produce: $AU_BUNDLE" >&2
    exit 1
fi

if [ ! -d "$VST3_BUNDLE" ]; then
    echo "VST3 build did not produce: $VST3_BUNDLE" >&2
    exit 1
fi

ditto "$AU_BUNDLE" "$AU_DEST"
ditto "$VST3_BUNDLE" "$VST3_DEST"
echo "✓ DFAF XT deployed"
