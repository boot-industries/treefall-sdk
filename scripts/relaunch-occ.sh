#!/bin/bash
# Relaunch Orpheus Clip Composer with clean cache and rebuild

set -e  # Exit on error

SDK_ROOT="/Users/chrislyons/dev/orpheus-sdk"
BUILD_DIR="$SDK_ROOT/build"
APP_PATH="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app"
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

echo "==> Checking build configuration..."
cd "$SDK_ROOT"

# Check if CMake build is configured
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "==> CMake not configured, running initial configuration..."
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
  echo "✓ CMake configuration complete"
else
  # Ensure Clip Composer is enabled in existing build
  if ! grep -q "ORPHEUS_ENABLE_APP_CLIP_COMPOSER:BOOL=ON" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
    echo "==> Enabling Clip Composer in existing build..."
    cmake -B build -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
    echo "✓ CMake reconfigured"
  fi
fi

echo "==> Building SDK libraries..."
cmake --build build --target orpheus_transport orpheus_audio_io orpheus_routing orpheus_audio_driver_coreaudio

echo "==> Rebuilding Clip Composer..."
cmake --build build --target orpheus_clip_composer_app

# Verify app bundle exists before launching
if [ ! -d "$APP_PATH" ]; then
  echo "ERROR: App bundle not found at: $APP_PATH"
  echo "Build may have failed. Check the output above for errors."
  exit 1
fi

echo "==> Launching new instance..."
"$APP_PATH/Contents/MacOS/OrpheusClipComposer" > /tmp/occ_output.log 2>&1 &

echo "✓ Orpheus Clip Composer relaunched (PID: $!)"
echo "  Logs: /tmp/occ_output.log, /tmp/audio_callback.txt"
