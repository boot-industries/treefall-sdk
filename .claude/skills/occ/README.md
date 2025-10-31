# Clip Composer - Skills Library

**Purpose:** Reusable code patterns, utilities, and validation scripts for common Clip Composer development tasks

---

## Available Skills

### 1. `validate.sh`

**Purpose:** Comprehensive validation script for Clip Composer project health
**Usage:** `bash .claude/skills/validate.sh`
**Checks:**

- Build status (can the project compile?)
- Code formatting (clang-format compliance)
- Documentation sync (versions match across files)
- Session schema validation (test fixtures)
- Git status (uncommitted changes, branch info)

### 2. `build-utils.sh`

**Purpose:** Helper functions for building and packaging
**Usage:** `source .claude/skills/build-utils.sh`
**Functions:**

- `occ_build_debug()` - Build Debug configuration
- `occ_build_release()` - Build Release configuration (when fixed)
- `occ_create_dmg()` - Package application as DMG
- `occ_verify_binary()` - Check binary architecture and size

### 3. `session-utils.sh`

**Purpose:** Session file manipulation and validation
**Usage:** `source .claude/skills/session-utils.sh`
**Functions:**

- `validate_session_json()` - Validate session file schema
- `upgrade_session_format()` - Upgrade old sessions to current format
- `create_test_session()` - Generate test fixture sessions

### 4. `waveform-benchmark.sh`

**Purpose:** Benchmark waveform rendering performance
**Usage:** `bash .claude/skills/waveform-benchmark.sh [audio_file]`
**Measures:**

- Generation time for various file sizes
- Memory usage during rendering
- Paint latency simulation

---

## How to Use Skills

Skills are executable scripts or sourced utilities that provide reusable functionality:

### Direct Execution (Scripts)

```bash
bash .claude/skills/validate.sh
```

### Sourced Utilities (Functions)

```bash
source .claude/skills/build-utils.sh
occ_build_debug
occ_create_dmg "v0.2.0-beta"
```

### From Claude Code

Skills can be invoked by Claude during development:

- "Run the validation script"
- "Validate the session file using session-utils"
- "Benchmark waveform rendering"

---

## Creating New Skills

When creating a new skill:

1. **Clear purpose:** Single responsibility, reusable across sessions
2. **Well-documented:** Header comments explaining usage
3. **Error handling:** Graceful failures with helpful messages
4. **Idempotent:** Can be run multiple times safely
5. **Shell best practices:** Quoting, error checking, shellcheck clean

**Template:**

```bash
#!/usr/bin/env bash
# Skill: [Name]
# Purpose: [1-2 sentence description]
# Usage: [command or source instructions]
# Dependencies: [list any required tools]

set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Configuration
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Main function
main() {
    # Implementation
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

---

## Skill vs Agent

**When to create a Skill:**

- Reusable validation or utility function
- Shell script that performs specific task
- Benchmarking or testing tool
- Build or packaging automation

**When to create an Agent:**

- Multi-step workflow requiring AI reasoning
- Tasks that need to read/edit code
- Complex debugging or optimization
- Context-aware decision making

**Example:**

- **Skill:** `validate.sh` - Runs checks and reports results
- **Agent:** `session-validator.md` - Analyzes results and suggests fixes

---

**Last Updated:** October 22, 2025
**Skill Count:** 4
**Next Review:** After v0.2.0 planning or when new common workflows emerge
