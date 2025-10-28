#!/usr/bin/env bash
# Skill: Project Validation
# Purpose: Comprehensive health check for Clip Composer project
# Usage: bash .claude/skills/validate.sh
# Dependencies: cmake, clang-format (optional), git, jq (optional)

set -euo pipefail

# Configuration
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
readonly SDK_ROOT="$(cd "$PROJECT_ROOT/../.." && pwd)"

# Colors for output
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly NC='\033[0m' # No Color

# Counters
PASS_COUNT=0
WARN_COUNT=0
FAIL_COUNT=0

# Helper functions
pass() {
    echo -e "${GREEN}✓${NC} $1"
    ((PASS_COUNT++))
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
    ((WARN_COUNT++))
}

fail() {
    echo -e "${RED}✗${NC} $1"
    ((FAIL_COUNT++))
}

section() {
    echo ""
    echo "=== $1 ==="
}

# Validation checks

check_build_system() {
    section "Build System"

    # Check CMakeLists.txt exists
    if [[ -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        pass "CMakeLists.txt exists"
    else
        fail "CMakeLists.txt missing"
        return
    fi

    # Check if build directory exists
    if [[ -d "$SDK_ROOT/build" ]]; then
        pass "Build directory exists"
    else
        warn "Build directory not found (run cmake first)"
    fi

    # Check if binary exists
    local binary_path="$SDK_ROOT/build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app"
    if [[ -d "$binary_path" ]]; then
        pass "Application binary exists"

        # Check binary size
        local size=$(du -sh "$binary_path" | cut -f1)
        echo "  Binary size: $size"
    else
        warn "Application binary not found (run cmake --build build)"
    fi
}

check_source_files() {
    section "Source Files"

    # Check key source files exist
    local key_files=(
        "Source/Main.cpp"
        "Source/MainComponent.h"
        "Source/MainComponent.cpp"
        "Source/Audio/AudioEngine.h"
        "Source/Audio/AudioEngine.cpp"
        "Source/Session/SessionManager.h"
        "Source/Session/SessionManager.cpp"
        "Source/UI/ClipGrid.h"
        "Source/UI/ClipGrid.cpp"
        "Source/UI/ClipButton.h"
        "Source/UI/ClipButton.cpp"
        "Source/UI/WaveformDisplay.h"
        "Source/UI/WaveformDisplay.cpp"
        "Source/UI/ClipEditDialog.h"
        "Source/UI/ClipEditDialog.cpp"
    )

    local missing=0
    for file in "${key_files[@]}"; do
        if [[ -f "$PROJECT_ROOT/$file" ]]; then
            ((PASS_COUNT++))
        else
            fail "Missing: $file"
            ((missing++))
        fi
    done

    if [[ $missing -eq 0 ]]; then
        pass "All key source files present (${#key_files[@]} files)"
    fi
}

check_documentation() {
    section "Documentation"

    # Check key docs exist
    local docs=(
        "README.md"
        "CLAUDE.md"
        ".claude/README.md"
        ".claude/implementation_progress.md"
    )

    for doc in "${docs[@]}"; do
        if [[ -f "$PROJECT_ROOT/$doc" ]]; then
            pass "$doc exists"
        else
            fail "$doc missing"
        fi
    done

    # Check design docs
    if [[ -d "$PROJECT_ROOT/docs/OCC" ]]; then
        local doc_count=$(find "$PROJECT_ROOT/docs/OCC" -name "*.md" | wc -l | tr -d ' ')
        if [[ $doc_count -gt 0 ]]; then
            pass "Design documentation: $doc_count files in docs/OCC/"
        else
            warn "No design documents found in docs/OCC/"
        fi
    else
        warn "docs/OCC directory not found"
    fi
}

check_code_formatting() {
    section "Code Formatting"

    if ! command -v clang-format &> /dev/null; then
        warn "clang-format not installed (skipping format check)"
        return
    fi

    # Check if .clang-format exists in SDK root
    if [[ ! -f "$SDK_ROOT/.clang-format" ]]; then
        warn ".clang-format not found in SDK root"
        return
    fi

    # Check a few key files for formatting
    local files_to_check=(
        "Source/MainComponent.cpp"
        "Source/UI/ClipButton.cpp"
    )

    local format_issues=0
    for file in "${files_to_check[@]}"; do
        if [[ -f "$PROJECT_ROOT/$file" ]]; then
            if ! clang-format --dry-run -Werror "$PROJECT_ROOT/$file" &> /dev/null; then
                ((format_issues++))
            fi
        fi
    done

    if [[ $format_issues -eq 0 ]]; then
        pass "Code formatting looks good (sampled ${#files_to_check[@]} files)"
    else
        warn "$format_issues files need formatting (run clang-format)"
    fi
}

check_git_status() {
    section "Git Status"

    cd "$SDK_ROOT" || return

    # Check current branch
    local branch=$(git rev-parse --abbrev-ref HEAD)
    echo "  Current branch: $branch"

    # Check for uncommitted changes
    if [[ -z $(git status --porcelain) ]]; then
        pass "Working directory clean"
    else
        warn "Uncommitted changes present"
        git status --short | head -5
    fi

    # Check recent commits
    echo "  Recent commits:"
    git log --oneline -3 | sed 's/^/    /'
}

check_agents_and_skills() {
    section "Agents & Skills"

    # Check agents directory
    if [[ -d "$PROJECT_ROOT/.claude/agents" ]]; then
        local agent_count=$(find "$PROJECT_ROOT/.claude/agents" -name "*.md" | wc -l | tr -d ' ')
        if [[ $agent_count -gt 0 ]]; then
            pass "Agents: $agent_count definitions found"
        else
            warn "No agent definitions found"
        fi
    else
        warn "Agents directory not found"
    fi

    # Check skills directory
    if [[ -d "$PROJECT_ROOT/.claude/skills" ]]; then
        local skill_count=$(find "$PROJECT_ROOT/.claude/skills" -name "*.sh" | wc -l | tr -d ' ')
        if [[ $skill_count -gt 0 ]]; then
            pass "Skills: $skill_count scripts found"
        else
            warn "No skill scripts found"
        fi
    else
        warn "Skills directory not found"
    fi
}

generate_report() {
    section "Validation Report"

    echo ""
    echo "Results:"
    echo "  ✓ Passed:  $PASS_COUNT"
    echo "  ⚠ Warnings: $WARN_COUNT"
    echo "  ✗ Failed:  $FAIL_COUNT"
    echo ""

    if [[ $FAIL_COUNT -eq 0 ]]; then
        if [[ $WARN_COUNT -eq 0 ]]; then
            echo -e "${GREEN}All checks passed!${NC}"
            return 0
        else
            echo -e "${YELLOW}Passed with warnings.${NC}"
            return 0
        fi
    else
        echo -e "${RED}Validation failed. Fix errors before proceeding.${NC}"
        return 1
    fi
}

# Main execution
main() {
    echo "Orpheus Clip Composer - Project Validation"
    echo "==========================================="

    check_build_system
    check_source_files
    check_documentation
    check_code_formatting
    check_git_status
    check_agents_and_skills

    generate_report
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
