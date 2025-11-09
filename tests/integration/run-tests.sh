#!/usr/bin/env bash
# Integration Test Runner (C++ SDK)
# Run with: ./tests/integration/run-tests.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$ROOT_DIR"

echo "========================================="
echo "Orpheus SDK Integration Tests"
echo "========================================="
echo ""

# Check if build directory exists
if [ ! -d "build" ]; then
  echo "⚠ Error: Build directory not found"
  echo "Please run: cmake -S . -B build && cmake --build build"
  exit 1
fi

# Run C++ tests via CTest
echo "Running C++ SDK tests..."
ctest --test-dir build --output-on-failure

echo ""
echo "========================================="
echo "✓ Integration tests completed"
echo "========================================="
