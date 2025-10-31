#!/bin/bash
# Update repo-commands.html with current timestamp
# Usage: ./scripts/update-commands-page.sh

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HTML_FILE="$REPO_ROOT/docs/repo-commands.html"

if [ ! -f "$HTML_FILE" ]; then
    echo "Error: $HTML_FILE not found"
    exit 1
fi

# Update timestamp
CURRENT_DATE=$(date +%Y-%m-%d)
sed -i '' "s/Last updated: [0-9-]*/Last updated: $CURRENT_DATE/" "$HTML_FILE"

echo "✓ Updated $HTML_FILE with current date: $CURRENT_DATE"
echo "  Open in browser: file://$HTML_FILE"
