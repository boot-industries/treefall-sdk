#!/bin/bash
# OCC Quick Rebuild - Fast incremental build and launch
set -e

cd /Users/chrislyons/dev/orpheus-sdk

# Kill any existing instances
killall OrpheusClipComposer 2>/dev/null || true
killall cmake 2>/dev/null || true
sleep 1

# Build if needed (uses existing build dir for speed)
if [ ! -f "build/CMakeCache.txt" ]; then
    echo "⚙️  First-time setup (configuring CMake)..."
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
fi

echo "🔨 Building (incremental)..."
cmake --build build -j12 2>&1 | tail -20

APP="build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app"

if [ ! -f "$APP/Contents/MacOS/OrpheusClipComposer" ]; then
    echo "❌ Build failed"
    exit 1
fi

# Launch
echo "🚀 Launching..."
open "$APP"
sleep 2

if pgrep -f OrpheusClipComposer > /dev/null; then
    echo "✅ Running"
else
    echo "❌ Crashed - check Console.app"
    exit 1
fi
