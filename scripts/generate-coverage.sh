#!/bin/bash
# Generate code coverage reports for Orpheus SDK tests
# Part of ORP077 Task 3.1: Improve Test Reporting

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Orpheus SDK Code Coverage Report Generator ===${NC}"
echo ""

# Check dependencies
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov not found${NC}"
    echo "Install with: brew install lcov (macOS) or apt-get install lcov (Linux)"
    exit 1
fi

if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}Error: genhtml not found${NC}"
    echo "Install with: brew install lcov (macOS) or apt-get install lcov (Linux)"
    exit 1
fi

# Configuration
BUILD_DIR="${BUILD_DIR:-build-coverage}"
COVERAGE_DIR="coverage-report"

echo -e "${YELLOW}Step 1: Configure build with coverage flags${NC}"
cmake -S . -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
    -DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage"

echo ""
echo -e "${YELLOW}Step 2: Build test targets${NC}"
cmake --build "${BUILD_DIR}" --target clip_gain_test clip_loop_test clip_metadata_test

echo ""
echo -e "${YELLOW}Step 3: Run tests${NC}"
ctest --test-dir "${BUILD_DIR}" -R "clip_(gain|loop|metadata)" --output-on-failure

echo ""
echo -e "${YELLOW}Step 4: Capture coverage data${NC}"

# Initialize coverage baseline
lcov --directory "${BUILD_DIR}" --zerocounters

# Re-run tests to generate coverage data
ctest --test-dir "${BUILD_DIR}" -R "clip_(gain|loop|metadata)" --output-on-failure > /dev/null 2>&1

# Capture coverage info
lcov --directory "${BUILD_DIR}" \
     --capture \
     --output-file coverage.info \
     --rc branch_coverage=1 \
     --ignore-errors inconsistent,deprecated,format,unsupported

# Filter out system headers and test code
lcov --remove coverage.info \
     '/usr/*' \
     '/Applications/*' \
     '*/tests/*' \
     '*/external/*' \
     '*/build-coverage/_deps/*' \
     --output-file coverage_filtered.info \
     --rc branch_coverage=1 \
     --ignore-errors inconsistent,deprecated,format,unsupported

echo ""
echo -e "${YELLOW}Step 5: Generate HTML report${NC}"
genhtml coverage_filtered.info \
        --output-directory "${COVERAGE_DIR}" \
        --title "Orpheus SDK Code Coverage" \
        --legend \
        --show-details \
        --rc branch_coverage=1 \
        --ignore-errors inconsistent,deprecated,format,unsupported

echo ""
echo -e "${GREEN}✓ Coverage report generated!${NC}"
echo ""
echo "Report location: ${COVERAGE_DIR}/index.html"
echo ""
echo -e "${YELLOW}Coverage Summary:${NC}"
lcov --summary coverage_filtered.info \
     --rc branch_coverage=1 \
     --ignore-errors inconsistent,deprecated,format,unsupported

echo ""
echo -e "${GREEN}To view the report:${NC}"
echo "  open ${COVERAGE_DIR}/index.html"
echo ""
