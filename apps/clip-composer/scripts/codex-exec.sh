#!/bin/bash
# Codex CLI Exec Mode (Non-Interactive, Sandboxed)
#
# Usage:
#   ./codex-exec.sh "Write a hello world function"
#   ./codex-exec.sh "Review the code in src/"

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ $# -eq 0 ]; then
    echo "❌ Error: No prompt provided"
    echo ""
    echo "Usage: $0 \"Your prompt here\""
    echo ""
    echo "Examples:"
    echo "  $0 \"Write a hello world function\""
    echo "  $0 \"Review the code in src/\""
    exit 1
fi

if ! command -v codex &> /dev/null; then
    echo "❌ Error: Codex CLI not found in PATH"
    exit 1
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "⚡ CODEX EXEC MODE - Sandboxed"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ Allowed:  $PROJECT_DIR"
echo "🚫 Blocked:  All other directories"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

cd "$PROJECT_DIR" || exit 1

# Run in exec mode (non-interactive)
codex exec "$@"
