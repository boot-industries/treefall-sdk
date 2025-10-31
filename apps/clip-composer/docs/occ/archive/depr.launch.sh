#!/bin/bash
# Orpheus Clip Composer - Launch Script
# Usage: ./launch.sh [debug|release] [--rebuild|--clean]

set -e

# Parse arguments
BUILD_TYPE="${1:-debug}"
REBUILD_FLAG="${2:-}"
BUILD_TYPE_UPPER=$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
APP_PATH="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$BUILD_TYPE_UPPER/OrpheusClipComposer.app"
EXECUTABLE="$APP_PATH/Contents/MacOS/OrpheusClipComposer"
LOG_FILE="/tmp/occ.log"

echo "Orpheus Clip Composer Launcher"
echo "==============================="
echo "Build Type: $BUILD_TYPE_UPPER"
echo ""

# Handle clean rebuild if requested
if [ "$REBUILD_FLAG" = "--clean" ]; then
    echo "🧹 Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo ""
fi

# Check if app exists or rebuild flag set
if [ ! -f "$EXECUTABLE" ] || [ "$REBUILD_FLAG" = "--rebuild" ] || [ "$REBUILD_FLAG" = "--clean" ]; then
    echo "🔨 Building OrpheusClipComposer..."
    echo ""

    # Configure if needed (check for CMakeCache.txt)
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        echo "⚙️  Configuring CMake..."
        cd "$REPO_ROOT"
        cmake -S . -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE_UPPER -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
        echo ""
    fi

    # Build
    echo "🔨 Compiling..."
    cd "$REPO_ROOT"
    cmake --build build -j$(sysctl -n hw.ncpu) 2>&1 | grep -E "(Building|Linking|Built target|Error|error:|warning:)" || true
    echo ""

    # Verify build succeeded
    if [ ! -f "$EXECUTABLE" ]; then
        echo "❌ Build failed - executable not found"
        exit 1
    fi

    echo "✅ Build complete"
    echo ""
fi

echo "App Path: $APP_PATH"
echo ""

# Kill existing instances
echo "🔄 Stopping existing instances..."
killall OrpheusClipComposer 2>/dev/null || true
sleep 1

# Clear macOS app cache to force reload
echo "🧹 Clearing macOS app cache..."
rm -rf "$APP_PATH/Contents/_CodeSignature" 2>/dev/null || true
xattr -cr "$APP_PATH" 2>/dev/null || true
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -r -domain local -domain system -domain user 2>/dev/null || true

# Clear old log
> "$LOG_FILE"

# Launch app
echo "🚀 Launching OrpheusClipComposer..."
echo "📝 Logs: $LOG_FILE"
echo ""

"$EXECUTABLE" > "$LOG_FILE" 2>&1 &
APP_PID=$!

# Wait a moment to check if it started
sleep 1

if ps -p $APP_PID > /dev/null 2>&1; then
    echo "✅ OrpheusClipComposer is running (PID: $APP_PID)"
    echo ""
    echo "To view logs:"
    echo "   tail -f $LOG_FILE"
    echo ""
    echo "To stop:"
    echo "   killall OrpheusClipComposer"
else
    echo "❌ Failed to start. Check logs:"
    echo "   cat $LOG_FILE"
    exit 1
fi
