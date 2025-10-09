#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/lib/phase-validation.sh"

cd "$REPO_ROOT"

print_header "Phase 2 Validation"
phase0_baseline_checks
phase1_tooling_checks
phase2_feature_checks
print_success "Phase 2 validation complete"
