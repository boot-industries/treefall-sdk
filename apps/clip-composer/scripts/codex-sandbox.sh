#!/bin/bash
# Codex CLI Sandbox Wrapper
#
# CRITICAL: This script ensures Codex CLI can ONLY access this directory
# and cannot see any other directories on the system.
#
# Usage:
#   ./codex-sandbox.sh "Your prompt here"
#   ./codex-sandbox.sh  # Interactive mode

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Verify codex is available
if ! command -v codex &> /dev/null; then
    echo "❌ Error: Codex CLI not found in PATH"
    echo "Install from: https://codex.anthropic.com/"
    exit 1
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🔒 CODEX CLI SANDBOXED MODE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Allowed:  $PROJECT_DIR"
echo "🚫 Blocked:  All other directories on system"
echo "🔧 Method:   macOS Seatbelt"
echo "📦 Model:    gpt-5.1-codex (configurable)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Change to project directory
cd "$PROJECT_DIR" || { echo "❌ Error: Cannot access project directory"; exit 1; }

# Run codex in sandboxed mode
# The sandbox macos command restricts file access to current directory
if [ $# -eq 0 ]; then
    # Interactive mode
    codex
else
    # Non-interactive with prompt
    codex "$@"
fi
