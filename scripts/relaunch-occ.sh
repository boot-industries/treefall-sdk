#!/bin/bash
# Relaunch Orpheus Clip Composer with clean cache and rebuild

set -e  # Exit on error

SDK_ROOT="/Users/chrislyons/dev/orpheus-sdk"
APP_PATH="$SDK_ROOT/build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app"
APP_BUNDLE_ID="com.orpheus.clipcomposer"

echo "==> Killing existing instance..."
killall OrpheusClipComposer 2>/dev/null || true

echo "==> Clearing macOS caches..."
# Clear macOS application cache
rm -rf ~/Library/Caches/$APP_BUNDLE_ID 2>/dev/null || true

# Clear preferences (optional - comment out if you want to preserve settings)
# defaults delete $APP_BUNDLE_ID 2>/dev/null || true

# Clear diagnostic logs
rm -f /tmp/audio_callback.txt /tmp/occ_output.log

echo "==> Rebuilding Clip Composer..."
cd "$SDK_ROOT"
cmake --build build --target orpheus_clip_composer_app

echo "==> Launching new instance..."
"$APP_PATH/Contents/MacOS/OrpheusClipComposer" > /tmp/occ_output.log 2>&1 &

echo "✓ Orpheus Clip Composer relaunched (PID: $!)"
echo "  Logs: /tmp/occ_output.log, /tmp/audio_callback.txt"
