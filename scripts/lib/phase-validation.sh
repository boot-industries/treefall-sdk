#!/usr/bin/env bash
# Shared helpers for phase validation scripts.

if [[ -n ${BASH_VERSINFO:-} && ${BASH_VERSINFO[0]} -lt 4 ]]; then
  echo "Phase validation scripts require Bash >= 4.0" >&2
  exit 1
fi

: "${REPO_ROOT:?REPO_ROOT must be set before sourcing phase-validation.sh}"

# Ensure Puppeteer never attempts to download Chromium as part of validation
# workflows. These exports can be overridden by callers, but default to skip
# downloads for CI and bootstrap scripts that source this helper.
export PUPPETEER_SKIP_DOWNLOAD=${PUPPETEER_SKIP_DOWNLOAD:-1}
export PUPPETEER_SKIP_CHROMIUM_DOWNLOAD=${PUPPETEER_SKIP_CHROMIUM_DOWNLOAD:-1}

CHECK_MARK="✓"
CROSS_MARK="✗"
ARROW="→"
HEADER_LINE="==="

print_header() {
  local title="$1"
  echo "${HEADER_LINE} ${title} ${HEADER_LINE}"
}

print_success() {
  local message="$1"
  echo "${CHECK_MARK} ${message}"
}

print_section() {
  local title="$1"
  echo
  echo "${title}"
  printf '%s\n' "$(printf '%*s' "${#title}" '' | tr ' ' '-')"
}

require_command() {
  local cmd="$1"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "${CROSS_MARK} Required command '$cmd' was not found in PATH." >&2
    exit 1
  fi
}

run_step() {
  local description="$1"
  shift
  local -a cmd=("$@")

  echo "${ARROW} ${description}" 

  set +e
  "${cmd[@]}"
  local status=$?
  set -e

  if [[ $status -eq 0 ]]; then
    echo "${CHECK_MARK} ${description}"
  else
    echo "${CROSS_MARK} ${description} (exit code ${status})" >&2
    exit $status
  fi
}

phase0_baseline_checks() {
  print_section "Phase 0 – Repository Baseline"

  require_command pnpm
  require_command cmake
  require_command ctest

  run_step "Install workspace dependencies" pnpm install --frozen-lockfile
  run_step "Inspect workspace packages" pnpm list --depth 0

  local build_dir="$REPO_ROOT/build/phase-validation-debug"
  run_step "Configure CMake (Debug)" cmake -S "$REPO_ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
  run_step "Build Orpheus C++ (Debug)" cmake --build "$build_dir" --config Debug
  run_step "Run C++ tests (Debug)" ctest --test-dir "$build_dir" --output-on-failure --build-config Debug

  run_step "Build Shmui workspace package" pnpm --filter @orpheus/shmui build
  run_step "Run repository lint suite" pnpm run lint
}

phase1_tooling_checks() {
  print_section "Phase 1 – Tooling Normalization"

  run_step "Build native engine workspace package" pnpm --filter @orpheus/engine-native build
  run_step "Run workspace test suites" pnpm -r test
}

phase2_feature_checks() {
  print_section "Phase 2 – Feature Integration"

  run_step "Build all workspace packages" pnpm -r build
  run_step "Run Shmui package tests" pnpm --filter @orpheus/shmui test
}

phase3_governance_checks() {
  print_section "Phase 3 – Governance & Quality"

  run_step "Verify performance budgets manifest" test -f "$REPO_ROOT/budgets.json"
  run_step "Ensure documentation Quickstart section exists" grep -q '^### Quickstart' "$REPO_ROOT/README.md"
  run_step "Ensure bootstrap script is executable" test -x "$REPO_ROOT/scripts/bootstrap-dev.sh"
}

phase4_release_checks() {
  print_section "Phase 4 – Release Readiness"

  local build_dir="$REPO_ROOT/build/phase-validation-release"
  run_step "Configure CMake (Release)" cmake -S "$REPO_ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
  run_step "Build Orpheus C++ (Release)" cmake --build "$build_dir" --config Release
  run_step "Run C++ tests (Release)" ctest --test-dir "$build_dir" --output-on-failure --build-config Release
}
