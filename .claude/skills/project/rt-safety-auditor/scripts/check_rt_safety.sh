#!/bin/bash
# Real-Time Safety Auditor Script
# Scans C++ code for real-time safety violations
# Version: 1.0
# Usage: ./check_rt_safety.sh <file_or_directory>

set -e

# Colors for output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Counters
VIOLATIONS=0
WARNINGS=0
FILES_SCANNED=0

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REF_DIR="$SCRIPT_DIR/../reference"

# Check if input provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <file_or_directory>"
    echo "Example: $0 src/modules/m1/transport_controller.cpp"
    echo "Example: $0 src/"
    exit 1
fi

INPUT="$1"

# Function to scan a single file
scan_file() {
    local file="$1"
    local filename=$(basename "$file")

    echo "Scanning: $file"
    FILES_SCANNED=$((FILES_SCANNED + 1))

    # Check if file exists
    if [ ! -f "$file" ]; then
        echo "Error: File not found: $file"
        return 1
    fi

    # Pattern 1: Heap allocations
    if grep -n '\bnew\s\+\w\+\[' "$file" | grep -v '//.*new' | grep -v 'placement new'; then
        echo -e "${RED}VIOLATION${NC}: Heap allocation detected (new[])"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bdelete\s\+\[' "$file" | grep -v '//.*delete'; then
        echo -e "${RED}VIOLATION${NC}: Heap deallocation detected (delete[])"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bmalloc\s*(' "$file" | grep -v '//.*malloc'; then
        echo -e "${RED}VIOLATION${NC}: Heap allocation detected (malloc)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bcalloc\s*(' "$file" | grep -v '//.*calloc'; then
        echo -e "${RED}VIOLATION${NC}: Heap allocation detected (calloc)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\brealloc\s*(' "$file" | grep -v '//.*realloc'; then
        echo -e "${RED}VIOLATION${NC}: Heap allocation detected (realloc)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bfree\s*(' "$file" | grep -v '//.*free'; then
        echo -e "${RED}VIOLATION${NC}: Heap deallocation detected (free)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 2: Locks
    if grep -n 'std::mutex' "$file" | grep -v '//.*std::mutex'; then
        echo -e "${RED}VIOLATION${NC}: Mutex detected (std::mutex)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::lock_guard' "$file" | grep -v '//.*std::lock_guard'; then
        echo -e "${RED}VIOLATION${NC}: Lock guard detected (std::lock_guard)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::unique_lock' "$file" | grep -v '//.*std::unique_lock'; then
        echo -e "${RED}VIOLATION${NC}: Unique lock detected (std::unique_lock)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::scoped_lock' "$file" | grep -v '//.*std::scoped_lock'; then
        echo -e "${RED}VIOLATION${NC}: Scoped lock detected (std::scoped_lock)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'pthread_mutex_lock' "$file" | grep -v '//.*pthread_mutex_lock'; then
        echo -e "${RED}VIOLATION${NC}: Pthread mutex lock detected"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 3: Console I/O
    if grep -n 'std::cout' "$file" | grep -v '//.*std::cout'; then
        echo -e "${RED}VIOLATION${NC}: Console output detected (std::cout)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::cerr' "$file" | grep -v '//.*std::cerr'; then
        echo -e "${RED}VIOLATION${NC}: Console output detected (std::cerr)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bprintf\s*(' "$file" | grep -v '//.*printf'; then
        echo -e "${RED}VIOLATION${NC}: Console output detected (printf)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bfprintf\s*(' "$file" | grep -v '//.*fprintf'; then
        echo -e "${RED}VIOLATION${NC}: File output detected (fprintf)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 4: File I/O
    if grep -n '\bfopen\s*(' "$file" | grep -v '//.*fopen'; then
        echo -e "${RED}VIOLATION${NC}: File I/O detected (fopen)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bfread\s*(' "$file" | grep -v '//.*fread'; then
        echo -e "${RED}VIOLATION${NC}: File I/O detected (fread)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n '\bfwrite\s*(' "$file" | grep -v '//.*fwrite'; then
        echo -e "${RED}VIOLATION${NC}: File I/O detected (fwrite)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::ifstream' "$file" | grep -v '//.*std::ifstream'; then
        echo -e "${RED}VIOLATION${NC}: File I/O detected (std::ifstream)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'std::ofstream' "$file" | grep -v '//.*std::ofstream'; then
        echo -e "${RED}VIOLATION${NC}: File I/O detected (std::ofstream)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 5: Threading
    if grep -n 'std::thread' "$file" | grep -v '//.*std::thread'; then
        echo -e "${RED}VIOLATION${NC}: Thread creation detected (std::thread)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'pthread_create' "$file" | grep -v '//.*pthread_create'; then
        echo -e "${RED}VIOLATION${NC}: Thread creation detected (pthread_create)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'sleep_for' "$file" | grep -v '//.*sleep_for'; then
        echo -e "${RED}VIOLATION${NC}: Thread sleep detected (sleep_for)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    if grep -n 'sleep_until' "$file" | grep -v '//.*sleep_until'; then
        echo -e "${RED}VIOLATION${NC}: Thread sleep detected (sleep_until)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 6: Exceptions
    if grep -n '\bthrow\s\+' "$file" | grep -v '//.*throw' | grep -v 'noexcept'; then
        echo -e "${RED}VIOLATION${NC}: Exception throwing detected"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi

    # Pattern 7: Warnings (may allocate)
    if grep -n '\.push_back\s*(' "$file" | grep -v '//.*push_back' | grep -v 'SAFE:'; then
        echo -e "${YELLOW}WARNING${NC}: push_back may allocate"
        WARNINGS=$((WARNINGS + 1))
    fi

    if grep -n '\.push_front\s*(' "$file" | grep -v '//.*push_front'; then
        echo -e "${YELLOW}WARNING${NC}: push_front may allocate"
        WARNINGS=$((WARNINGS + 1))
    fi

    if grep -n '\.insert\s*(' "$file" | grep -v '//.*insert' | grep -v 'SAFE:'; then
        echo -e "${YELLOW}WARNING${NC}: insert may allocate"
        WARNINGS=$((WARNINGS + 1))
    fi

    if grep -n '\.resize\s*(' "$file" | grep -v '//.*resize' | grep -v 'SAFE:'; then
        echo -e "${YELLOW}WARNING${NC}: resize may allocate"
        WARNINGS=$((WARNINGS + 1))
    fi

    # Pattern 8: String operations (may allocate)
    if grep -n 'std::string' "$file" | grep -v '//.*std::string' | grep -v 'using std::string' | grep -v '#include'; then
        echo -e "${YELLOW}WARNING${NC}: std::string operations may allocate"
        WARNINGS=$((WARNINGS + 1))
    fi

    echo ""
}

# Main execution
echo "=== Orpheus Real-Time Safety Auditor ==="
echo "Checking: $INPUT"
echo ""

if [ -f "$INPUT" ]; then
    # Single file
    scan_file "$INPUT"
elif [ -d "$INPUT" ]; then
    # Directory - find all .cpp and .h files
    find "$INPUT" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) | while read -r file; do
        scan_file "$file"
    done
else
    echo "Error: Input is neither a file nor a directory: $INPUT"
    exit 1
fi

# Summary
echo "========================================="
echo "=== Audit Summary ==="
echo "Files scanned: $FILES_SCANNED"
echo -e "${RED}Violations: $VIOLATIONS${NC}"
echo -e "${YELLOW}Warnings: $WARNINGS${NC}"
echo ""

if [ $VIOLATIONS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ Real-Time Safety: VERIFIED${NC}"
    echo "No violations or warnings detected."
    exit 0
elif [ $VIOLATIONS -eq 0 ]; then
    echo -e "${YELLOW}⚠ Real-Time Safety: WARNINGS${NC}"
    echo "No critical violations, but $WARNINGS warnings found."
    echo "Review warnings to ensure they are safe (pre-allocated, bounded)."
    exit 0
else
    echo -e "${RED}✗ Real-Time Safety: VIOLATIONS DETECTED${NC}"
    echo "$VIOLATIONS critical violations must be fixed before use in audio threads."
    exit 1
fi
