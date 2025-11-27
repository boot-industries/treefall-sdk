#!/bin/bash
# Codex CLI Full-Auto Mode (Sandboxed)
#
# Convenience wrapper for low-friction sandboxed automatic execution.
# Network-disabled sandbox that can write to current directory and TMPDIR.
#
# Usage:
#   ./codex-auto.sh "Refactor this function"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if ! command -v codex &> /dev/null; then
    echo "❌ Error: Codex CLI not found in PATH"
    exit 1
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🤖 CODEX FULL-AUTO MODE - Sandboxed"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Allowed:  $PROJECT_DIR + TMPDIR"
echo "🚫 Network:  Disabled"
echo "⚡ Auto:     Enabled"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

cd "$PROJECT_DIR" || exit 1

# Run with --full-auto flag
codex sandbox macos --full-auto codex "$@"
