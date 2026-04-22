#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_VERSION="0.1"
RELEASE_VERSION="${1:-$DEFAULT_VERSION}"
BUILD_DIR="$PROJECT_ROOT/build-universal-release"
RELEASE_DIR="$PROJECT_ROOT/release"
RELEASE_NAME="DFAF-XT-v${RELEASE_VERSION}-macOS-universal-vst3"
ZIP_PATH="$RELEASE_DIR/${RELEASE_NAME}.zip"
SHA_PATH="$RELEASE_DIR/${RELEASE_NAME}.sha256.txt"
PLUGIN_BUNDLE="$BUILD_DIR/DFAFXT_artefacts/VST3/DFAF XT.vst3"
PLUGIN_BINARY="$PLUGIN_BUNDLE/Contents/MacOS/DFAF XT"

CMAKE_BIN="${CMAKE_BIN:-/usr/local/bin/cmake}"
if [ ! -x "$CMAKE_BIN" ]; then
    CMAKE_BIN="$(command -v cmake)"
fi

for required_tool in "$CMAKE_BIN" ditto lipo shasum; do
    if ! command -v "$required_tool" >/dev/null 2>&1; then
        echo "Missing required tool: $required_tool" >&2
        exit 1
    fi
done

mkdir -p "$BUILD_DIR" "$RELEASE_DIR"

"$CMAKE_BIN" -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

"$CMAKE_BIN" --build "$BUILD_DIR" --target DFAFXT_VST3

if [ ! -f "$PLUGIN_BINARY" ]; then
    echo "Release build did not produce: $PLUGIN_BINARY" >&2
    exit 1
fi

ARCH_INFO="$(lipo -info "$PLUGIN_BINARY")"
case "$ARCH_INFO" in
    *x86_64*arm64*|*arm64*x86_64*)
        ;;
    *)
        echo "Expected a universal binary with x86_64 and arm64, got:" >&2
        echo "$ARCH_INFO" >&2
        exit 1
        ;;
esac

rm -f "$ZIP_PATH" "$SHA_PATH"
ditto -c -k --sequesterRsrc --keepParent "$PLUGIN_BUNDLE" "$ZIP_PATH"
shasum -a 256 "$ZIP_PATH" > "$SHA_PATH"

echo "✓ DFAF XT universal VST3 release ready"
echo "  Version: $RELEASE_VERSION"
echo "  Bundle:  $PLUGIN_BUNDLE"
echo "  Zip:     $ZIP_PATH"
echo "  SHA256:  $SHA_PATH"
echo "  Arch:    $ARCH_INFO"
