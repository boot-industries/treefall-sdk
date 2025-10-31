#!/bin/bash
# Start Claude Code instance for Clip Composer development
# This script ensures the correct working directory and provides context-appropriate guidance

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLIP_COMPOSER_DIR="$REPO_ROOT/apps/clip-composer"

# Verify directory exists
if [ ! -d "$CLIP_COMPOSER_DIR" ]; then
    echo "Error: Clip Composer directory not found at $CLIP_COMPOSER_DIR"
    exit 1
fi

# Change to Clip Composer directory
cd "$CLIP_COMPOSER_DIR"

echo "========================================"
echo "Starting Clip Composer Claude Code Instance"
echo "========================================"
echo ""
echo "Working Directory: $CLIP_COMPOSER_DIR"
echo ""
echo "Focus Areas:"
echo "  - Tauri desktop application"
echo "  - JUCE UI components"
echo "  - Application-specific features"
echo ""
echo "Documentation:"
echo "  - Primary docs: apps/clip-composer/docs/occ/"
echo "  - Progress tracker: apps/clip-composer/.claude/implementation_progress.md"
echo ""
echo "Active Skills:"
echo "  - orpheus.doc.gen (OCC documentation generation)"
echo "  - test.analyzer (Application test analysis)"
echo "  - ui.component.validator (UI component validation)"
echo ""
echo "========================================"
echo ""

# Launch Claude Code in the Clip Composer directory
exec claude-code
