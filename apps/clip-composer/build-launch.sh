#!/bin/bash
# Orpheus Clip Composer - Build & Launch
# Usage: ./build-and-launch.sh [debug|release] [app-args...]

set -e

BUILD_TYPE="${1:-debug}"
BUILD_TYPE_UPPER=$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')
shift || true
APP_ARGS=("$@")

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
APP_PATH="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$BUILD_TYPE_UPPER/OrpheusClipComposer.app"
EXECUTABLE="$APP_PATH/Contents/MacOS/OrpheusClipComposer"
LOG_FILE="/tmp/occ.log"

echo "🏗️  Orpheus Clip Composer - Build & Launch"
echo "=========================================="
echo "Build Type: $BUILD_TYPE_UPPER"
echo ""

# Kill existing instances
echo "🔄 Stopping existing instances..."
killall -9 OrpheusClipComposer 2>/dev/null || true
sleep 0.5

# Configure if needed
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "⚙️  First-time setup (configuring CMake)..."
    
    # Pre-fetch JUCE to avoid FetchContent hang
    mkdir -p "$BUILD_DIR/_deps/juce-src"
    if [ ! -d "$BUILD_DIR/_deps/juce-src/.git" ]; then
        echo "📦 Downloading JUCE 8.0.4..."
        git clone --depth 1 --branch 8.0.4 \
          https://github.com/juce-framework/JUCE.git \
          "$BUILD_DIR/_deps/juce-src"
    fi
    
    cd "$REPO_ROOT"
    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE_UPPER \
      -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON \
      -DFETCHCONTENT_SOURCE_DIR_JUCE="$BUILD_DIR/_deps/juce-src"
    echo ""
fi

# Build SDK libraries first
echo "🔨 Building Orpheus SDK libraries..."
cd "$REPO_ROOT"
cmake --build build \
  --target orpheus_transport orpheus_audio_io orpheus_routing orpheus_audio_driver_coreaudio \
  -j$(sysctl -n hw.ncpu)

# Build app
echo "🔨 Building OrpheusClipComposer..."
cmake --build build --target orpheus_clip_composer_app -j$(sysctl -n hw.ncpu)

if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Build failed - executable not found"
    exit 1
fi

echo "✅ Build complete"
echo ""

if [[ "$BUILD_TYPE_UPPER" == "DEBUG" && "$(uname -s)" == "Darwin" && -z "${ASAN_OPTIONS:-}" ]]; then
    export ASAN_OPTIONS="halt_on_error=0:detect_leaks=0:abort_on_error=0"
    echo "🩺 Using macOS GUI ASan policy: $ASAN_OPTIONS"
fi

# Clear macOS caches
echo "🧹 Clearing macOS app cache..."
rm -rf "$APP_PATH/Contents/_CodeSignature" 2>/dev/null || true
xattr -cr "$APP_PATH" 2>/dev/null || true

# Clear log
> "$LOG_FILE"

# Launch
echo "🚀 Launching OrpheusClipComposer..."
echo "📝 Logs: $LOG_FILE"
echo ""

"$EXECUTABLE" "${APP_ARGS[@]}" > "$LOG_FILE" 2>&1 &
APP_PID=$!

sleep 1

if ps -p $APP_PID > /dev/null 2>&1; then
    echo "✅ Running (PID: $APP_PID)"
    echo ""
    echo "Logs: tail -f $LOG_FILE"
else
    echo "❌ Failed to start. Logs: cat $LOG_FILE"
    exit 1
fi
