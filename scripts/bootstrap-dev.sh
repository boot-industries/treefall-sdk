#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

echo "=== Orpheus SDK Bootstrap ==="

# Version checks
command -v node >/dev/null 2>&1 || { echo "❌ Node.js ≥18 required. Install: https://nodejs.org"; exit 1; }
command -v pnpm >/dev/null 2>&1 || { echo "❌ PNPM ≥8 required. Install: npm install -g pnpm"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "❌ CMake ≥3.20 required. Install: https://cmake.org/download/"; exit 1; }

# Check C++ compiler
if ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
  echo "❌ C++ compiler required (g++, clang++, or MSVC)"
  exit 1
fi

echo "✓ All required tools found"

# Create .env.example if missing
if [ ! -f .env.example ]; then
  cat > .env.example << 'EOF_ENV'
# Orpheus SDK Configuration
NODE_ENV=development
ORPHEUS_DRIVER=auto
ENABLE_ORPHEUS_FEATURES=true
ORPHEUS_TELEMETRY=false
EOF_ENV
  echo "✓ Created .env.example"
fi

echo ""
echo "→ Installing dependencies..."
pnpm install

echo ""
echo "→ Building C++ core..."
if [ ! -d build ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
fi
cmake --build build

echo ""
echo "→ Running C++ tests..."
ctest --test-dir build --output-on-failure

echo ""
echo "→ Building all packages..."
pnpm run build

echo ""
echo "→ Installing Git hooks (Husky)..."
pnpm run prepare || echo "⚠️  Husky not configured yet (will be added in Phase 3)"

echo ""
echo "→ Running validation..."
"$REPO_ROOT/scripts/validate-phase0.sh"

echo ""
echo "✅ Bootstrap complete!"
echo ""
echo "Next steps:"
echo "  • Run 'pnpm dev' to start development server"
echo "  • See docs/GETTING_STARTED.md for detailed guide"
echo ""
echo "Troubleshooting: https://github.com/orpheus-sdk/docs/TROUBLESHOOTING.md"
