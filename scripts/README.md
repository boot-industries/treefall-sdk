# Orpheus SDK Scripts

This directory contains automation scripts for development, validation, and deployment of the Orpheus SDK.

---

## Table of Contents

- [Development Scripts](#development-scripts)
- [Validation Scripts](#validation-scripts)
- [Phase Validation Scripts](#phase-validation-scripts)
- [Build Scripts](#build-scripts)
- [Usage Examples](#usage-examples)

---

## Development Scripts

### `bootstrap-dev.sh`

**Purpose:** One-command setup for new contributors

**What it does:**

- Installs JavaScript dependencies (`pnpm install`)
- Configures CMake build (`cmake -S . -B build`)
- Builds C++ core (`cmake --build build`)
- Runs tests to verify setup (`ctest --test-dir build`)

**Usage:**

```bash
./scripts/bootstrap-dev.sh
```

**When to use:** First time setting up the repository, after pulling major changes

---

## Validation Scripts

### `validate-sdk.sh` ⭐ **NEW**

**Purpose:** Comprehensive SDK validation before submitting PRs

**What it does:**

1. Configures CMake build (Debug with sanitizers)
2. Builds all targets
3. Runs all tests with `ctest`
4. Checks C++ formatting (`clang-format`)
5. Runs JavaScript linting (`pnpm run lint`)
6. Runs JavaScript tests (`pnpm test`)
7. Prints color-coded summary

**Usage:**

```bash
./scripts/validate-sdk.sh
```

**Exit codes:**

- `0` - All checks passed ✓
- `1` - One or more checks failed ✗

**When to use:** Before committing, before opening PRs, after making changes

**Requirements:**

- CMake 3.22+
- C++20 compiler
- pnpm (optional, skips JS checks if missing)
- clang-format (optional, skips formatting check if missing)

---

### `check-determinism.sh` ⭐ **NEW**

**Purpose:** Validate bit-identical audio rendering across platforms

**What it does:**

1. Renders click track using `orpheus_minhost`
2. Computes SHA-256 checksum of rendered audio
3. Prints checksum for cross-platform comparison

**Usage:**

```bash
./scripts/check-determinism.sh
```

**Cross-platform validation:**

```bash
# On macOS
./scripts/check-determinism.sh
# SHA-256: 1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905

# On Linux
./scripts/check-determinism.sh
# SHA-256: 1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905

# On Windows (Git Bash)
./scripts/check-determinism.sh
# SHA-256: 1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905
```

**Why determinism matters:** Professional audio requires bit-identical output for:

- Reproducible mixes
- Collaborative workflows
- Quality assurance
- Broadcast compliance

**When to use:** Before releases, after audio processing changes, cross-platform testing

---

### `generate-coverage.sh` ⭐ **NEW**

**Purpose:** Generate code coverage reports with lcov

**What it does:**

1. Builds SDK with coverage instrumentation (`--coverage` flags)
2. Runs test suite (clip gain, loop, metadata tests)
3. Captures coverage data with lcov
4. Filters out system headers and test code
5. Generates HTML report with line/function/branch coverage

**Usage:**

```bash
./scripts/generate-coverage.sh
```

**Output:**

- `coverage-report/index.html` - Interactive HTML coverage report
- `coverage_filtered.info` - lcov data file

**To view the report:**

```bash
open coverage-report/index.html
```

**Current coverage (as of ORP077 Task 3.1):**

```
Overall coverage rate:
  lines.......: 12.0% (425 of 3551 lines)
  functions...: 18.3% (93 of 508 functions)
  branches....: 6.8% (709 of 10426 branches)

Transport Controller coverage:
  lines.......: 29.2% (173 of 592 lines)
  functions...: 50.0% (16 of 32 functions)
  branches....: 15.9% (293 of 1837 branches)
```

**When to use:** After adding tests, before releases, to measure test quality

**Requirements:**

- lcov (`brew install lcov` on macOS)
- Coverage-enabled compiler (GCC or Clang)

**Note:** This script creates a separate build directory (`build-coverage`) with coverage instrumentation enabled. This is different from the regular `build/` directory.

---

### `lint-cpp.sh`

**Purpose:** Check C++ code formatting compliance

**What it does:**

- Runs `clang-format --dry-run --Werror` on all C++ files
- Reports formatting violations without modifying files

**Usage:**

```bash
./scripts/lint-cpp.sh
```

**To auto-fix violations:**

```bash
pnpm run format:cpp
```

**When to use:** Before committing C++ changes (also runs in pre-commit hook)

---

## Phase Validation Scripts

These scripts validate ORP068 phase completion gates.

### `validate-phase0.sh`

**Purpose:** Validate Phase 0 (Repository Setup) completion

**Checks:**

- Bootstrap script works
- CMake builds succeed
- Tests pass
- Documentation exists

**Usage:**

```bash
./scripts/validate-phase0.sh
```

---

### `validate-phase1.sh`

**Purpose:** Validate Phase 1 (Driver Architecture) completion

**Checks:**

- Contract package builds
- Service Driver builds
- Native Driver builds
- Client Broker builds
- React integration builds
- All tests pass

**Usage:**

```bash
./scripts/validate-phase1.sh
```

---

### `validate-phase2.sh`

**Purpose:** Validate Phase 2 (Contract Expansion + UI) completion

**Checks:**

- Contract v1.0.0-beta builds
- WASM infrastructure ready
- UI components build
- Frequency validator tests pass

**Usage:**

```bash
./scripts/validate-phase2.sh
```

---

### `validate-phase3.sh`

**Purpose:** Validate Phase 3 (CI/CD Infrastructure) completion

**Checks:**

- CI pipeline configuration valid
- Performance budgets enforced
- Dependency check works
- Security audit tools installed

**Usage:**

```bash
./scripts/validate-phase3.sh
```

---

### `validate-phase4.sh`

**Purpose:** Validate Phase 4 (Documentation + Release) completion

**Checks:**

- API documentation generated
- Integration guides complete
- Examples build and run
- Validation scripts work

**Usage:**

```bash
./scripts/validate-phase4.sh
```

---

## Build Scripts

### `relaunch-occ.sh`

**Purpose:** Rebuild and launch Orpheus Clip Composer (OCC) macOS app

**What it does:**

1. Builds OCC Debug binary
2. Waits for app to finish building
3. Launches OCC.app

**Usage:**

```bash
./scripts/relaunch-occ.sh
```

**When to use:** During OCC development, after making changes to OCC source

**Platform:** macOS only

---

### `changesets-status.sh`

**Purpose:** Check status of changesets for monorepo versioning

**What it does:**

- Displays pending changesets
- Shows which packages have unreleased changes

**Usage:**

```bash
./scripts/changesets-status.sh
```

**When to use:** Before creating releases, during version planning

---

## Usage Examples

### Daily Development Workflow

**Morning setup (after `git pull`):**

```bash
# Verify everything still works
./scripts/validate-sdk.sh
```

**Before committing C++ changes:**

```bash
# Check formatting
./scripts/lint-cpp.sh

# Fix if needed
pnpm run format:cpp

# Full validation
./scripts/validate-sdk.sh
```

**Before opening a PR:**

```bash
# Comprehensive validation
./scripts/validate-sdk.sh

# If working on audio processing
./scripts/check-determinism.sh
```

### Cross-Platform Testing

**Testing determinism across platforms:**

```bash
# Developer on macOS
./scripts/check-determinism.sh > macos-checksum.txt

# CI on Linux
./scripts/check-determinism.sh > linux-checksum.txt

# CI on Windows
./scripts/check-determinism.sh > windows-checksum.txt

# Compare checksums (should all match)
diff macos-checksum.txt linux-checksum.txt
diff linux-checksum.txt windows-checksum.txt
```

### Release Preparation

**Before tagging a release:**

```bash
# Run all phase validations
./scripts/validate-phase0.sh
./scripts/validate-phase1.sh
./scripts/validate-phase2.sh
./scripts/validate-phase3.sh
./scripts/validate-phase4.sh

# Verify SDK validation
./scripts/validate-sdk.sh

# Check determinism
./scripts/check-determinism.sh

# If all pass, ready to tag
git tag v1.0.0
```

---

## Troubleshooting

### `validate-sdk.sh` fails on C++ formatting

**Error:**

```
✗ C++ formatting check failed
Run 'pnpm run format:cpp' to fix formatting issues
```

**Solution:**

```bash
pnpm run format:cpp
git add src/ include/
git commit -m "style: apply clang-format"
```

---

### `validate-sdk.sh` fails on JavaScript linting

**Error:**

```
✗ JavaScript linting failed
Run 'pnpm run lint:fix' to attempt auto-fix
```

**Solution:**

```bash
pnpm run lint:fix
git add packages/
git commit -m "style: fix eslint violations"
```

---

### `check-determinism.sh` fails with "orpheus_minhost not found"

**Error:**

```
Error: orpheus_minhost binary not found
```

**Solution:**

```bash
# Build minhost
cmake -S . -B build
cmake --build build --target orpheus_minhost

# Try again
./scripts/check-determinism.sh
```

---

### Scripts have permission errors

**Error:**

```
bash: ./scripts/validate-sdk.sh: Permission denied
```

**Solution:**

```bash
# Make all scripts executable
chmod +x scripts/*.sh

# Try again
./scripts/validate-sdk.sh
```

---

## Adding New Scripts

When creating new automation scripts:

1. **Name scripts clearly** - Use verb-noun format (`validate-sdk.sh`, `check-determinism.sh`)
2. **Add shebang** - Start with `#!/bin/bash`
3. **Set fail-fast** - Use `set -e` for early termination
4. **Check dependencies** - Verify required tools exist before using them
5. **Provide clear output** - Use colors, section headers, and summary
6. **Document exit codes** - 0 = success, 1 = failure
7. **Make executable** - `chmod +x scripts/new-script.sh`
8. **Update this README** - Add documentation with usage examples

**Template:**

```bash
#!/bin/bash
set -e

# Script: new-script.sh
# Purpose: Brief description
# Usage: ./scripts/new-script.sh

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "Starting validation..."

# Your script logic here

echo -e "${GREEN}✓${NC} Script completed successfully"
exit 0
```

---

## Related Documentation

- [Contributing Guide](../docs/CONTRIBUTING.md) - PR submission process
- [Architecture Overview](../ARCHITECTURE.md) - SDK design principles
- [Quick Start Guide](../docs/QUICK_START.md) - New developer onboarding
- [ORP077 - SDK Core Quality Sprint](../docs/ORP/ORP077.md) - Implementation plan for validation scripts

---

**Last Updated:** October 26, 2025
**Maintained By:** SDK Core Team
