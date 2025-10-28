#!/bin/bash
# Orpheus SDK Skills Validation Script
# Validates skill installation and Orpheus-specific constraints
# Version: 1.0

set -e

# Colors
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Counters
ERRORS=0
WARNINGS=0
CHECKS=0

SKILLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== Orpheus SDK Skills Validation ==="
echo "Location: $SKILLS_DIR"
echo ""

# Function to check file exists
check_file() {
    local file="$1"
    local description="$2"
    CHECKS=$((CHECKS + 1))

    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $description: $file"
    else
        echo -e "${RED}✗${NC} $description: $file (MISSING)"
        ERRORS=$((ERRORS + 1))
    fi
}

# Function to validate YAML frontmatter
validate_skill() {
    local skill_file="$1"
    CHECKS=$((CHECKS + 1))

    if ! grep -q "^---$" "$skill_file"; then
        echo -e "${RED}✗${NC} $skill_file missing YAML frontmatter"
        ERRORS=$((ERRORS + 1))
        return 1
    fi

    if ! grep -q "^name:" "$skill_file"; then
        echo -e "${RED}✗${NC} $skill_file missing 'name' field"
        ERRORS=$((ERRORS + 1))
        return 1
    fi

    if ! grep -q "^description:" "$skill_file"; then
        echo -e "${RED}✗${NC} $skill_file missing 'description' field"
        ERRORS=$((ERRORS + 1))
        return 1
    fi

    echo -e "${GREEN}✓${NC} $skill_file: valid frontmatter"
    return 0
}

# Function to validate Orpheus-specific real-time mentions
validate_rt_safety() {
    local skill_file="$1"
    CHECKS=$((CHECKS + 1))

    if [[ $skill_file == *"rt-safety-auditor"* ]]; then
        if ! grep -qi "real-time\|lock-free\|audio thread" "$skill_file"; then
            echo -e "${RED}✗${NC} rt-safety-auditor must reference real-time constraints"
            ERRORS=$((ERRORS + 1))
            return 1
        fi
        echo -e "${GREEN}✓${NC} rt-safety-auditor: mentions real-time constraints"
    fi
    return 0
}

echo "--- Checking Directory Structure ---"
check_file "$SKILLS_DIR/manifest.json" "Manifest file"
check_file "$SKILLS_DIR/validate.sh" "Validation script"

echo ""
echo "--- Checking Project Skills ---"

# rt.safety.auditor
check_file "$SKILLS_DIR/project/rt-safety-auditor/SKILL.md" "rt.safety.auditor SKILL.md"
check_file "$SKILLS_DIR/project/rt-safety-auditor/reference/rt_constraints.md" "RT constraints reference"
check_file "$SKILLS_DIR/project/rt-safety-auditor/reference/banned_functions.md" "Banned functions reference"
check_file "$SKILLS_DIR/project/rt-safety-auditor/reference/allowed_patterns.md" "Allowed patterns reference"
check_file "$SKILLS_DIR/project/rt-safety-auditor/scripts/check_rt_safety.sh" "RT safety check script"

# test.result.analyzer
check_file "$SKILLS_DIR/project/test-result-analyzer/SKILL.md" "test.result.analyzer SKILL.md"
check_file "$SKILLS_DIR/project/test-result-analyzer/reference/test_formats.md" "Test formats reference"
check_file "$SKILLS_DIR/project/test-result-analyzer/reference/sanitizer_formats.md" "Sanitizer formats reference"

# orpheus.doc.gen
check_file "$SKILLS_DIR/project/orpheus-doc-gen/SKILL.md" "orpheus.doc.gen SKILL.md"
check_file "$SKILLS_DIR/project/orpheus-doc-gen/templates/doxygen_function.template" "Doxygen function template"
check_file "$SKILLS_DIR/project/orpheus-doc-gen/templates/doxygen_class.template" "Doxygen class template"

echo ""
echo "--- Checking Shared Skills ---"
check_file "$SKILLS_DIR/shared/test-analyzer/SKILL.md" "test.analyzer SKILL.md"
check_file "$SKILLS_DIR/shared/test-analyzer/config.json" "test.analyzer config"
check_file "$SKILLS_DIR/shared/ci-troubleshooter/SKILL.md" "ci.troubleshooter SKILL.md"
check_file "$SKILLS_DIR/shared/ci-troubleshooter/config.json" "ci.troubleshooter config"

echo ""
echo "--- Validating SKILL.md Files ---"

# Validate all SKILL.md files
while IFS= read -r -d '' skill_file; do
    validate_skill "$skill_file"
    validate_rt_safety "$skill_file"
done < <(find "$SKILLS_DIR" -name "SKILL.md" -print0)

echo ""
echo "--- Validating manifest.json ---"
CHECKS=$((CHECKS + 1))

if [ -f "$SKILLS_DIR/manifest.json" ]; then
    # Check JSON syntax
    if python3 -c "import json; json.load(open('$SKILLS_DIR/manifest.json'))" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} manifest.json: valid JSON"
    else
        echo -e "${RED}✗${NC} manifest.json: invalid JSON syntax"
        ERRORS=$((ERRORS + 1))
    fi

    # Check for required fields
    if grep -q '"version"' "$SKILLS_DIR/manifest.json" && \
       grep -q '"repository"' "$SKILLS_DIR/manifest.json" && \
       grep -q '"skills"' "$SKILLS_DIR/manifest.json"; then
        echo -e "${GREEN}✓${NC} manifest.json: required fields present"
    else
        echo -e "${RED}✗${NC} manifest.json: missing required fields"
        ERRORS=$((ERRORS + 1))
    fi

    # Check for Orpheus project constraints
    if grep -q '"real_time_safety": true' "$SKILLS_DIR/manifest.json"; then
        echo -e "${GREEN}✓${NC} manifest.json: real-time safety constraint present"
    else
        echo -e "${YELLOW}⚠${NC} manifest.json: real-time safety constraint missing"
        WARNINGS=$((WARNINGS + 1))
    fi
else
    echo -e "${RED}✗${NC} manifest.json not found"
    ERRORS=$((ERRORS + 1))
fi

echo ""
echo "--- Orpheus-Specific Checks ---"

# Check if RT safety script is executable
CHECKS=$((CHECKS + 1))
if [ -x "$SKILLS_DIR/project/rt-safety-auditor/scripts/check_rt_safety.sh" ]; then
    echo -e "${GREEN}✓${NC} RT safety script is executable"
else
    echo -e "${YELLOW}⚠${NC} RT safety script not executable (run: chmod +x check_rt_safety.sh)"
    WARNINGS=$((WARNINGS + 1))
fi

# Run RT safety auditor on examples
echo ""
echo "--- Testing RT Safety Auditor ---"
CHECKS=$((CHECKS + 1))

if [ -f "$SKILLS_DIR/project/rt-safety-auditor/scripts/check_rt_safety.sh" ]; then
    EXAMPLES_DIR="$SKILLS_DIR/project/rt-safety-auditor/examples"

    if [ -d "$EXAMPLES_DIR" ]; then
        # Test on violation example (should fail)
        if [ -f "$EXAMPLES_DIR/violation_heap_alloc.cpp" ]; then
            if bash "$SKILLS_DIR/project/rt-safety-auditor/scripts/check_rt_safety.sh" \
                "$EXAMPLES_DIR/violation_heap_alloc.cpp" > /dev/null 2>&1; then
                echo -e "${RED}✗${NC} RT safety auditor did not detect violations in violation_heap_alloc.cpp"
                ERRORS=$((ERRORS + 1))
            else
                echo -e "${GREEN}✓${NC} RT safety auditor correctly detected violations"
            fi
        fi

        # Test on safe example (should pass)
        if [ -f "$EXAMPLES_DIR/safe_audio_callback.cpp" ]; then
            if bash "$SKILLS_DIR/project/rt-safety-auditor/scripts/check_rt_safety.sh" \
                "$EXAMPLES_DIR/safe_audio_callback.cpp" > /dev/null 2>&1; then
                echo -e "${GREEN}✓${NC} RT safety auditor correctly passed safe code"
            else
                echo -e "${YELLOW}⚠${NC} RT safety auditor flagged safe code (possible false positive)"
                WARNINGS=$((WARNINGS + 1))
            fi
        fi
    fi
fi

# Summary
echo ""
echo "========================================="
echo "=== Validation Summary ==="
echo "Total Checks: $CHECKS"
echo -e "${RED}Errors: $ERRORS${NC}"
echo -e "${YELLOW}Warnings: $WARNINGS${NC}"
echo ""

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All Orpheus skills validated successfully!${NC}"
    echo "Skills are ready for use."
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ Validation passed with $WARNINGS warnings${NC}"
    echo "Review warnings above. Skills are functional but may need attention."
    exit 0
else
    echo -e "${RED}✗ Validation failed with $ERRORS errors${NC}"
    echo "Fix errors above before using skills."
    exit 1
fi
