#!/bin/bash
# Orpheus Clip Composer - Launch Script
# Usage: ./launch.sh [debug|release]

set -e

# Determine build type (default: debug)
BUILD_TYPE="${1:-debug}"
BUILD_TYPE_UPPER=$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP_PATH="$REPO_ROOT/build/apps/clip-composer/orpheus_clip_composer_app_artefacts/$BUILD_TYPE_UPPER/OrpheusClipComposer.app"
EXECUTABLE="$APP_PATH/Contents/MacOS/OrpheusClipComposer"
LOG_FILE="/tmp/occ.log"

echo "Orpheus Clip Composer Launcher"
echo "==============================="
echo "Build Type: $BUILD_TYPE_UPPER"
echo "App Path: $APP_PATH"
echo ""

# Check if app exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ Error: OrpheusClipComposer not found at:"
    echo "   $EXECUTABLE"
    echo ""
    echo "Build the app first with:"
    echo "   cmake --build build --target orpheus_clip_composer_app"
    exit 1
fi

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
