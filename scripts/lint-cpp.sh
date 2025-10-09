#!/usr/bin/env bash
set -e

if ! command -v clang-format >/dev/null 2>&1; then
  echo "⚠️  clang-format not found, skipping C++ linting"
  exit 0
fi

echo "→ Linting C++ files..."
find src include -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | \
  xargs clang-format --dry-run --Werror

echo "✓ C++ linting passed"
