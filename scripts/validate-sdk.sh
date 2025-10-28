#!/usr/bin/env bash
set -e

# Orpheus SDK Validation Script
# This script runs a comprehensive validation of the SDK before submitting PRs.
# It checks C++ builds, tests, formatting, and JavaScript linting.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track overall success
VALIDATION_FAILED=0

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

# Step 1: Check dependencies
print_step "Checking dependencies"

if ! command -v cmake >/dev/null 2>&1; then
  print_error "cmake not found"
  VALIDATION_FAILED=1
else
  print_success "cmake found ($(cmake --version | head -1))"
fi

if ! command -v pnpm >/dev/null 2>&1; then
  print_warning "pnpm not found, skipping JavaScript checks"
  SKIP_JS=1
else
  print_success "pnpm found ($(pnpm --version))"
  SKIP_JS=0
fi

if ! command -v clang-format >/dev/null 2>&1; then
  print_warning "clang-format not found, skipping C++ formatting check"
  SKIP_FORMAT=1
else
  print_success "clang-format found"
  SKIP_FORMAT=0
fi

# Step 2: Configure CMake build
print_step "Configuring CMake (Debug with sanitizers)"

if [ ! -d "build" ]; then
  if ! cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug; then
    print_error "CMake configuration failed"
    VALIDATION_FAILED=1
  else
    print_success "CMake configured successfully"
  fi
else
  print_success "Build directory exists, skipping configuration"
fi

# Step 3: Build all targets
print_step "Building all targets"

if ! cmake --build build; then
  print_error "Build failed"
  VALIDATION_FAILED=1
else
  print_success "Build completed successfully"
fi

# Step 4: Run all tests
print_step "Running all tests"

if ! ctest --test-dir build --output-on-failure; then
  print_error "Tests failed"
  VALIDATION_FAILED=1
else
  print_success "All tests passed"
fi

# Step 5: Check clang-format compliance
if [ "$SKIP_FORMAT" -eq 0 ]; then
  print_step "Checking C++ formatting (clang-format)"

  # Find all C++ source files
  CPP_FILES=$(find src include -name "*.cpp" -o -name "*.h" -o -name "*.hpp" 2>/dev/null || true)

  if [ -z "$CPP_FILES" ]; then
    print_warning "No C++ files found to check"
  else
    if ! echo "$CPP_FILES" | xargs clang-format --dry-run --Werror; then
      print_error "C++ formatting check failed. Run 'pnpm run format:cpp' to fix."
      VALIDATION_FAILED=1
    else
      print_success "C++ formatting is compliant"
    fi
  fi
fi

# Step 6: Run JavaScript linters
if [ "$SKIP_JS" -eq 0 ]; then
  print_step "Running JavaScript linters"

  if ! pnpm run lint; then
    print_error "JavaScript linting failed"
    VALIDATION_FAILED=1
  else
    print_success "JavaScript linting passed"
  fi
fi

# Step 7: Run JavaScript tests
if [ "$SKIP_JS" -eq 0 ]; then
  print_step "Running JavaScript tests"

  if ! pnpm test; then
    print_error "JavaScript tests failed"
    VALIDATION_FAILED=1
  else
    print_success "JavaScript tests passed"
  fi
fi

# Final summary
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ "$VALIDATION_FAILED" -eq 0 ]; then
  echo -e "${GREEN}✓ SDK validation passed${NC}"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  exit 0
else
  echo -e "${RED}✗ SDK validation failed${NC}"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo ""
  echo "Please fix the errors above before submitting your PR."
  exit 1
fi
