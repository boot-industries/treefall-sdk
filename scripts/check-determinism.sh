#!/usr/bin/env bash
set -e

# Orpheus SDK Determinism Validation Script
#
# This script validates that audio rendering is deterministic by:
# 1. Rendering a test audio file using the minhost adapter
# 2. Computing a SHA-256 checksum of the output
#
# For cross-platform determinism validation, run this script on multiple
# platforms (macOS, Linux, Windows) and compare the checksums. Identical
# checksums indicate bit-identical output, which is a core requirement
# for broadcast-safe, deterministic audio rendering.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
MINHOST_BIN="./build/adapters/minhost/orpheus_minhost"
SESSION_FILE="tools/fixtures/solo_click.json"
OUTPUT_FILE="/tmp/orpheus_render_test.wav"
# Calculate range in beats: 2 bars at 4/4 = 8 beats
RANGE="0:8"

# Helper functions
print_step() {
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "  $1"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

print_success() {
  echo -e "${GREEN}✓${NC} $1"
}

print_error() {
  echo -e "${RED}✗${NC} $1"
}

print_warning() {
  echo -e "${YELLOW}⚠${NC}  $1"
}

# Step 1: Check prerequisites
print_step "Checking prerequisites"

if [ ! -f "$MINHOST_BIN" ]; then
  print_error "orpheus_minhost not found at $MINHOST_BIN"
  echo "Please build the project first: cmake --build build"
  exit 1
fi
print_success "Found orpheus_minhost"

if [ ! -f "$SESSION_FILE" ]; then
  print_error "Session fixture not found at $SESSION_FILE"
  exit 1
fi
print_success "Found session fixture"

# Determine checksum command (macOS uses shasum, Linux uses sha256sum)
if command -v sha256sum >/dev/null 2>&1; then
  CHECKSUM_CMD="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  CHECKSUM_CMD="shasum -a 256"
else
  print_error "Neither sha256sum nor shasum found"
  exit 1
fi
print_success "Using checksum command: $CHECKSUM_CMD"

# Step 2: Render click track
print_step "Rendering test audio (range: ${RANGE} beats)"

if ! "$MINHOST_BIN" render-click \
  --session "$SESSION_FILE" \
  --out "$OUTPUT_FILE" \
  --range "$RANGE"; then
  print_error "Render failed"
  exit 1
fi

print_success "Render completed: $OUTPUT_FILE"

# Step 3: Generate SHA-256 checksum
print_step "Computing SHA-256 checksum"

CHECKSUM=$($CHECKSUM_CMD "$OUTPUT_FILE" | awk '{print $1}')

if [ -z "$CHECKSUM" ]; then
  print_error "Failed to compute checksum"
  exit 1
fi

print_success "Checksum computed successfully"

# Step 4: Print result
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✓ Determinism check completed${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "SHA-256: $CHECKSUM"
echo "File:    $OUTPUT_FILE"
echo ""
echo "To validate cross-platform determinism:"
echo "  1. Run this script on different platforms (macOS, Linux, Windows)"
echo "  2. Compare the SHA-256 checksums"
echo "  3. Identical checksums = bit-identical output = deterministic ✓"
echo ""

exit 0
