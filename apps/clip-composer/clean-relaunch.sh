#!/bin/bash
# Orpheus Clip Composer - Quick Relaunch
# Usage: ./relaunch.sh [debug|release]

set -e

BUILD_TYPE="${1:-debug}"
BUILD_TYPE_UPPER=$(echo "$BUILD_TYPE" | tr '[:lower:]' '[:upper:]')

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
APP_PATH="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$BUILD_TYPE_UPPER/OrpheusClipComposer.app"
EXECUTABLE="$APP_PATH/Contents/MacOS/OrpheusClipComposer"
LOG_FILE="/tmp/occ.log"

echo "🔄 Orpheus Clip Composer - Quick Relaunch"
echo "========================================"

# Verify app exists
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ App not built. Run build-and-launch.sh first"
    exit 1
fi

# Kill existing
echo "🛑 Stopping existing instances..."
killall -9 OrpheusClipComposer 2>/dev/null || true
sleep 0.5

# Clear caches
echo "🧹 Clearing caches..."
rm -rf "$APP_PATH/Contents/_CodeSignature" 2>/dev/null || true
xattr -cr "$APP_PATH" 2>/dev/null || true

# Clear log
> "$LOG_FILE"

# Launch
echo "🚀 Launching..."
"$EXECUTABLE" > "$LOG_FILE" 2>&1 &
APP_PID=$!

sleep 1

if ps -p $APP_PID > /dev/null 2>&1; then
    echo "✅ Running (PID: $APP_PID) | Logs: tail -f $LOG_FILE"
else
    echo "❌ Failed. Logs: cat $LOG_FILE"
    exit 1
fi