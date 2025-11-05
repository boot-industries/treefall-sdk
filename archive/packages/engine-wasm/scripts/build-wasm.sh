#!/usr/bin/env bash
# Build script for Orpheus WASM module with version enforcement and SRI generation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
PKG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PKG_DIR/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "🔨 Building Orpheus WASM module..."

# Check if Emscripten is installed
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}Error: Emscripten (emcc) not found in PATH${NC}"
    echo "Please install Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html"
    exit 1
fi

# Verify Emscripten version
REQUIRED_VERSION=$(cat "$REPO_ROOT/.emscripten-version")
CURRENT_VERSION=$(emcc --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n1)

if [ "$CURRENT_VERSION" != "$REQUIRED_VERSION" ]; then
    echo -e "${RED}Error: Emscripten version mismatch${NC}"
    echo "Required: $REQUIRED_VERSION"
    echo "Found:    $CURRENT_VERSION"
    echo ""
    echo "To switch versions with emsdk:"
    echo "  emsdk install $REQUIRED_VERSION"
    echo "  emsdk activate $REQUIRED_VERSION"
    echo "  source \$(emsdk_env.sh)"
    exit 1
fi

echo -e "${GREEN}✓${NC} Emscripten version $CURRENT_VERSION (matches required)"

# Create build directory
mkdir -p "$BUILD_DIR"

# Build with CMake (Emscripten toolchain)
echo "Configuring CMake..."
emcmake cmake -S "$PKG_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DORPHEUS_ROOT="$REPO_ROOT"

echo "Building WASM module..."
cmake --build "$BUILD_DIR"

# Verify outputs
if [ ! -f "$BUILD_DIR/orpheus.js" ] || [ ! -f "$BUILD_DIR/orpheus.wasm" ]; then
    echo -e "${RED}Error: Build failed - output files not found${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} WASM module built successfully"

# Generate SRI integrity hashes
echo "Generating SRI integrity hashes..."

if ! command -v openssl &> /dev/null; then
    echo -e "${YELLOW}Warning: openssl not found, skipping SRI generation${NC}"
else
    WASM_HASH=$(openssl dgst -sha384 -binary "$BUILD_DIR/orpheus.wasm" | openssl base64 -A)
    JS_HASH=$(openssl dgst -sha384 -binary "$BUILD_DIR/orpheus.js" | openssl base64 -A)

    cat > "$BUILD_DIR/integrity.json" <<EOF
{
  "version": "0.1.0",
  "generated": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "emscripten": "$CURRENT_VERSION",
  "files": {
    "orpheus.wasm": "sha384-$WASM_HASH",
    "orpheus.js": "sha384-$JS_HASH"
  }
}
EOF

    echo -e "${GREEN}✓${NC} SRI hashes generated: $BUILD_DIR/integrity.json"
fi

# Print build summary
WASM_SIZE=$(du -h "$BUILD_DIR/orpheus.wasm" | cut -f1)
JS_SIZE=$(du -h "$BUILD_DIR/orpheus.js" | cut -f1)

echo ""
echo "Build Summary:"
echo "  WASM: $WASM_SIZE ($BUILD_DIR/orpheus.wasm)"
echo "  JS:   $JS_SIZE ($BUILD_DIR/orpheus.js)"
echo "  SRI:  $BUILD_DIR/integrity.json"
echo ""
echo -e "${GREEN}✅ Build complete!${NC}"
