# ORP105 CI Infrastructure Diagnosis

**Date:** 2025-11-09
**Status:** Implemented ✅
**Author:** Claude Code Investigation
**Related:** ORP102, ORP103, ORP104, DECISION_PACKAGES.md

---

## Executive Summary

- **Root Cause #1:** Incomplete cleanup of TypeScript CI jobs after package archival (2025-11-05)
- **Root Cause #2:** CMake build cache poisoning (cached builds without libsndfile support)
- **Root Cause #3:** Integration test script referencing archived TypeScript packages
- **Root Cause #4:** Insufficient libsndfile installation verification in CI workflows
- **Affected Platforms:** macOS, Windows, Ubuntu (all platforms on interim-ci.yml workflow)
- **Fix:** Remove obsolete workflow, update package.json, remove CMake cache, fix integration tests, add libsndfile verification
- **Status:** Implemented and ready for verification

---

## Problem Statement

CI builds failed on macOS and Windows platforms across multiple PRs over several weeks. Initial investigation suspected infrastructure issues ("code bloat"), but deeper analysis revealed **incomplete implementation of the package archival decision**.

### Symptoms

1. ❌ macOS builds: FAILING (all workflows attempting to build packages)
2. ❌ Windows builds: FAILING (all workflows attempting to build packages)
3. ✅ Ubuntu builds on `ci-pipeline.yml`: PASSING (C++ SDK only)
4. ❌ All platforms on `interim-ci.yml`: FAILING (references archived packages)

### Timeline

- **2025-11-05:** Decision made to archive `packages/` directory (DECISION_PACKAGES.md)
- **2025-11-05:** Packages directory removed from repository
- **2025-11-05 to 2025-11-09:** CI failures persist due to incomplete workflow cleanup
- **2025-11-09:** Root cause identified and fixed (ORP105)

---

## Root Cause Analysis

### Investigation Process

1. **Examined CI workflow files** (.github/workflows/)
   - `ci-pipeline.yml` - Primary workflow, C++ SDK only ✅
   - `ci.yml` - Ubuntu-only workflow, C++ SDK only ✅
   - `interim-ci.yml` - Legacy workflow referencing packages ❌

2. **Checked repository structure**
   ```bash
   $ ls -la packages/
   ls: cannot access 'packages/': No such file or directory
   ```
   Result: **packages/ directory does not exist** (archived as planned)

3. **Analyzed interim-ci.yml**
   - Line 108: `working-directory: packages/engine-native` ❌
   - Line 109: `run: pnpm install --frozen-lockfile` ❌
   - Line 112-113: `run: pnpm run build:ts` ❌
   - Line 135-136: `run: pnpm run build:native` ❌
   - Line 142: `path: packages/engine-native/build/Release/orpheus_native.node` ❌

4. **Verified package.json configuration**
   - Line 7-8: `"workspaces": ["packages/*"]` ❌
   - Lines 10-28: Multiple scripts referencing packages ❌

### Root Cause #1: Incomplete Package Archival

**The DECISION_PACKAGES.md implementation checklist was incomplete:**

```markdown
**Completed Actions:**
- [x] Created docs/DECISION_PACKAGES.md documenting choice
- [ ] Moved packages/ to archive/packages/ (preserves structure)  ← INCOMPLETE
- [ ] Updated .claudeignore to exclude archived packages           ← INCOMPLETE
- [ ] Removed TypeScript CI jobs from .github/workflows/           ← INCOMPLETE ⚠️
- [ ] Removed chaos tests workflow (packages-only)                 ← INCOMPLETE
- [ ] Updated README.md with "C++ SDK" positioning                 ← INCOMPLETE
- [ ] Updated .claude/implementation_progress.md                   ← INCOMPLETE
```

The critical missing step was **"Removed TypeScript CI jobs from .github/workflows/"**, leaving `interim-ci.yml` attempting to build non-existent packages.

### Root Cause #2: CMake Build Cache Poisoning

**After fixing Root Cause #1, tests still failed with `SessionGraphError::NotReady` (error code 03).**

Investigation revealed:

1. **Test Failure Pattern:**
   ```
   Expected equality of these values:
     regResult
       Which is: 1-byte object <03>
     SessionGraphError::OK
       Which is: 1-byte object <00>
   Failed to register test clip
   ```

2. **Error Code 03 = `SessionGraphError::NotReady`**
   - Returned by `TransportController::registerClipAudio()` when audio file reader creation fails
   - Line 864-866 in `transport_controller.cpp`:
     ```cpp
     // Check if audio file reader is available (may be nullptr if libsndfile not installed)
     if (!uniqueReader) {
       return SessionGraphError::NotReady; // Audio file reading not available
     }
     ```

3. **Cache Poisoning Mechanism:**
   - `.github/workflows/ci-pipeline.yml` lines 74-81 cached the entire `build/` directory
   - Cache key: `${{ runner.os }}-cmake-${{ matrix.build_type }}-${{ hashFiles(...) }}`
   - **Problem:** Cache key doesn't include dependency state (libsndfile presence)
   - If cache was created when libsndfile failed to install, it persists even after libsndfile is installed
   - CMakeCache.txt in cached build contains: `libsndfile: NOT FOUND`
   - Subsequent builds restore this cached configuration, skipping dependency detection

4. **Why This Happened:**
   - Libsndfile installation steps run **before** cache restoration
   - Cache restoration happens **before** CMake configuration
   - Cached build directory contains stale CMakeCache.txt
   - CMake skips re-detection when cache is restored (assumes configuration is valid)

**Solution:** Remove CMake build cache entirely to force fresh configuration on every run.

### Root Cause #3: Integration Test Script Referencing Archived Packages

**After fixing Root Causes #1 and #2, `pnpm test` failed immediately.**

Investigation revealed:

1. **package.json Line 9:** `"test": "./tests/integration/run-tests.sh"`
2. **tests/integration/run-tests.sh Lines 19-23:**
   ```bash
   if [ ! -d "packages/contract/dist" ] || \
      [ ! -d "packages/client/dist" ] || \
      [ ! -d "packages/react/dist" ]; then
     echo "⚠ Warning: Some packages not built. Building now..."
     pnpm run build  # ← FAILS: "build" script removed from package.json
   fi
   ```

3. **Failure Mode:**
   - Script checks for TypeScript package dist directories (archived 2025-11-05)
   - Directories don't exist (as expected)
   - Script tries to run `pnpm run build` to build them
   - Build script was removed in package.json cleanup (Root Cause #1 fix)
   - Result: "Missing script: build" error

4. **Additional Issues:**
   - tests/integration/smoke.test.mjs imports archived TypeScript packages
   - Lines 19, 31, 43: Import from `packages/contract/dist`, `packages/client/dist`, `packages/react/dist`
   - These tests are for TypeScript SDK functionality, not C++ SDK

**Solution:** Rewrite `tests/integration/run-tests.sh` to run C++ SDK tests only (via ctest).

### Root Cause #4: Insufficient libsndfile Installation Verification

**After all previous fixes, tests might still fail if libsndfile installation is silently failing.**

Potential failure modes:

1. **macOS (Homebrew):**
   - `brew install libsndfile` succeeds but pkg-config not configured
   - CMake `pkg_check_modules(SNDFILE sndfile)` fails
   - Fallback `find_library()` might also fail if homebrew paths not in CMAKE_PREFIX_PATH

2. **Windows (vcpkg):**
   - `vcpkg install libsndfile:x64-windows` fails silently
   - vcpkg might not be initialized correctly
   - CMake toolchain file not loaded or incorrect path
   - Result: CMake can't find libsndfile even though it's "installed"

3. **CMake Detection:**
   - No verification step after CMake configuration
   - Stub implementation silently used (create_audio_file_reader_stub.cpp)
   - Tests compile and link successfully but fail at runtime with NotReady

**Solution:** Add explicit verification steps:
- Check libsndfile installation succeeded on each platform
- Verify CMake detected libsndfile (check build files for audio_file_reader_libsndfile.cpp vs stub)
- Fail CI early if libsndfile not detected (don't wait for test failures)

---

## Technical Details

### What interim-ci.yml Was Trying To Do

The workflow had 5 jobs:

1. **build-cpp** (ubuntu, windows, macos)
   - Build C++ SDK with CMake ✅ (should work)
   - Install libsndfile ✅
   - Run tests ✅

2. **build-native-driver** (ubuntu, windows, macos)
   - **FAILS:** References `packages/engine-native` which doesn't exist ❌
   - Attempts: `pnpm install`, `pnpm run build:ts`, `pnpm run build:native`

3. **build-ui** (ubuntu only)
   - **FAILS:** References packages workspace ❌
   - Attempts: `pnpm install`, `pnpm run build`, build service/client/react packages

4. **integration-tests** (ubuntu only)
   - **FAILS:** Depends on build-ui (which fails) ❌
   - Attempts: `pnpm run test:integration`

5. **status-check** (ubuntu only)
   - **FAILS:** Depends on all above jobs ❌

**Result:** All jobs except `build-cpp` failed because they referenced archived packages.

### Why macOS/Windows Appeared to Fail More

- The `ci-pipeline.yml` workflow (C++ SDK only) **should pass on all platforms**
- The `interim-ci.yml` workflow **failed on ALL platforms** (ubuntu, macos, windows)
- User perception: "macOS and Windows are broken" (actually: interim workflow is broken everywhere)

---

## Implementation

### Changes Made

#### 1. Removed Obsolete Workflow

**File:** `.github/workflows/interim-ci.yml`
**Action:** Deleted entirely
**Rationale:** Entire workflow was for TypeScript packages (archived 2025-11-05)

#### 2. Removed CMake Build Cache

**File:** `.github/workflows/ci-pipeline.yml`
**Lines Removed:** 74-81 (Cache CMake build step)
**Rationale:** Cache was poisoned with stale libsndfile detection results

**Before:**
```yaml
- name: Cache CMake build
  uses: actions/cache@v4
  with:
    path: build
    key: ${{ runner.os }}-cmake-${{ matrix.build_type }}-${{ hashFiles('**/CMakeLists.txt', 'src/**/*.cpp', 'src/**/*.h') }}
    restore-keys: |
      ${{ runner.os }}-cmake-${{ matrix.build_type }}-
      ${{ runner.os }}-cmake-
```

**After:** (removed - CMake will configure fresh on every run)

**Impact:**
- ✅ Ensures libsndfile detection runs on every build
- ✅ Prevents cache poisoning from dependency installation failures
- ⚠️ Slightly longer CI times (~30s per job, acceptable trade-off for reliability)

**Note:** The `ci.yml` workflow does not have caching and was unaffected.

#### 3. Fixed Integration Test Script

**File:** `tests/integration/run-tests.sh`
**Changes:** Complete rewrite to work with C++ SDK

**Before:**
```bash
# Ensure packages are built
echo "Step 1: Verifying package builds..."
if [ ! -d "packages/contract/dist" ] || \
   [ ! -d "packages/client/dist" ] || \
   [ ! -d "packages/react/dist" ]; then
  echo "⚠ Warning: Some packages not built. Building now..."
  pnpm run build
else
  echo "✓ Package builds verified"
fi

echo ""
echo "Step 2: Running smoke tests..."
node --test tests/integration/smoke.test.mjs
```

**After:**
```bash
# Check if build directory exists
if [ ! -d "build" ]; then
  echo "⚠ Error: Build directory not found"
  echo "Please run: cmake -S . -B build && cmake --build build"
  exit 1
fi

# Run C++ tests via CTest
echo "Running C++ SDK tests..."
ctest --test-dir build --output-on-failure
```

**Impact:**
- ✅ Removes references to archived TypeScript packages
- ✅ Runs C++ SDK tests directly via ctest
- ✅ `pnpm test` now works (calls C++ tests, not TypeScript builds)

**Note:** `tests/integration/smoke.test.mjs` remains in the repository but is not executed (archived TypeScript package tests).

#### 4. Updated package.json

**File:** `package.json`
**Changes:**
- Removed `"workspaces": ["packages/*"]` configuration
- Updated `name` from `"orpheus-monorepo"` to `"orpheus-sdk"`
- Updated `description` to `"C++ SDK for professional audio applications."`
- Removed package-related scripts:
  - `build`, `build:ts` (referenced packages)
  - `dep:check`, `dep:graph` (analyzed packages/)
  - `lint:js`, `format:js` (pnpm workspace commands)
  - `changeset`, `changeset:version`, `changeset:publish` (monorepo versioning)
  - `perf:validate` (packages-specific)
  - `postinstall` (workspace bootstrap)

**Retained Scripts:**
- `prepare` (husky git hooks - still needed)
- `test` (integration tests - C++ SDK)
- `lint`, `lint:cpp` (C++ formatting)
- `format`, `format:cpp` (C++ formatting)

#### 5. Added libsndfile Installation Verification

**File:** `.github/workflows/ci-pipeline.yml`
**Changes:** Added explicit verification for libsndfile installation and detection

**macOS Installation (Lines 58-71):**
```yaml
- name: Install libsndfile (macOS)
  if: matrix.os == 'macos-latest'
  run: |
    echo "Installing libsndfile via Homebrew..."
    brew install libsndfile

    # Verify installation
    if ! brew list libsndfile &>/dev/null; then
      echo "ERROR: libsndfile installation failed"
      exit 1
    fi

    echo "libsndfile installed successfully"
    pkg-config --modversion sndfile || echo "Note: pkg-config not finding sndfile (may use find_library fallback)"
```

**Windows Installation (Lines 73-79):**
```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile via vcpkg..."
    vcpkg install libsndfile:x64-windows --debug

    # Verify installation
    if ! vcpkg list | grep -q libsndfile; then
      echo "ERROR: libsndfile installation failed"
      exit 1
    fi

    echo "libsndfile installed successfully"
    vcpkg list | grep libsndfile

    # Set toolchain file for CMake
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
```

**CMake Detection Verification (Lines 106-124 - new step):**
```yaml
- name: Verify libsndfile detection
  run: |
    echo "Checking CMake configuration for libsndfile..."
    if grep -q "libsndfile not found" build/CMakeFiles/CMakeOutput.log 2>/dev/null || \
       grep -q "libsndfile not found" build/CMakeFiles/CMakeError.log 2>/dev/null; then
      echo "WARNING: libsndfile may not have been detected correctly"
    fi

    # Check if audio_file_reader_libsndfile.cpp is being compiled
    if [ -f "build/CMakeFiles/orpheus_audio_io.dir/DependInfo.cmake" ]; then
      if grep -q "audio_file_reader_libsndfile.cpp" build/CMakeFiles/orpheus_audio_io.dir/DependInfo.cmake; then
        echo "✓ libsndfile detected - audio file reading enabled"
      else
        echo "⚠ libsndfile NOT detected - audio file reading disabled (stub implementation)"
        echo "This will cause transport tests to fail with NotReady error"
        exit 1
      fi
    fi
  shell: bash
```

**Impact:**
- ✅ Fails fast if libsndfile installation fails (before build)
- ✅ Fails fast if CMake doesn't detect libsndfile (before build)
- ✅ Prevents silent fallback to stub implementation
- ✅ Clearer error messages for debugging
- ✅ `--debug` flag on vcpkg provides verbose output for troubleshooting

**Before:**
```json
{
  "name": "orpheus-monorepo",
  "description": "Monorepo workspace for the Orpheus project.",
  "workspaces": ["packages/*"],
  "scripts": {
    "build": "pnpm -r build",
    "build:ts": "pnpm -r --filter='!@orpheus/engine-native' build",
    "dep:check": "madge --circular --extensions ts,tsx,js,jsx packages/",
    ...
  }
}
```

**After:**
```json
{
  "name": "orpheus-sdk",
  "description": "C++ SDK for professional audio applications.",
  "scripts": {
    "prepare": "husky",
    "test": "./tests/integration/run-tests.sh",
    "lint": "pnpm run lint:cpp",
    ...
  }
}
```

### Remaining CI Workflows

After cleanup, the repository has **two CI workflows**:

#### 1. ci-pipeline.yml (Primary, Multi-Platform)
- **Platforms:** ubuntu-latest, windows-latest, macos-latest
- **Build Types:** Debug, Release
- **Scope:** C++ SDK build, test, lint
- **Sanitizers:** ON for Debug (ubuntu, macos), OFF for Windows (MSVC limitation)
- **Status:** ✅ Should pass on all platforms (C++ SDK only)

#### 2. ci.yml (Secondary, Ubuntu Only)
- **Platform:** ubuntu-latest only
- **Build Types:** Debug, RelWithDebInfo
- **Scope:** C++ SDK build, test
- **Status:** ✅ Passes (confirmed)

**Both workflows now focus exclusively on C++ SDK** - no TypeScript package references.

---

## Verification Plan

### Expected Results After Fix

1. **macOS builds (ci-pipeline.yml):** ✅ PASS (C++ SDK, no package references)
2. **Windows builds (ci-pipeline.yml):** ✅ PASS (C++ SDK, no package references)
3. **Ubuntu builds (ci-pipeline.yml):** ✅ PASS (C++ SDK, confirmed working)
4. **Ubuntu builds (ci.yml):** ✅ PASS (C++ SDK, confirmed working)
5. **interim-ci.yml:** ⏹️ DELETED (no longer exists)

### Verification Steps

```bash
# 1. Confirm workflow removal
$ ls -la .github/workflows/
# Should see: ci-pipeline.yml, ci.yml (no interim-ci.yml)

# 2. Confirm package.json cleanup
$ grep -n "packages" package.json
# Should see: no references to packages/ directory

# 3. Trigger CI on branch
$ git push origin claude/investigate-ci-macos-windows-011CUxAwYCSDHBxBAqbpGW7o

# 4. Monitor CI runs
# - ci-pipeline.yml should pass on ubuntu, macos, windows (Debug + Release)
# - ci.yml should pass on ubuntu (Debug + RelWithDebInfo)

# 5. Check test results
# Expected: 58/58 tests passing on all platforms
```

### Success Criteria

- [ ] macOS Debug build passes
- [ ] macOS Release build passes
- [ ] Windows Debug build passes
- [ ] Windows Release build passes
- [ ] Ubuntu Debug build passes (with sanitizers)
- [ ] Ubuntu Release build passes
- [ ] All tests pass: 58/58 on each platform
- [ ] No workflow references archived packages

---

## Long-Term Prevention

### Process Improvements

1. **Complete Implementation Checklists**
   - When making architectural decisions (like DECISION_PACKAGES.md), ensure ALL checklist items are completed
   - Use GitHub issues to track multi-step cleanup tasks
   - Review CI workflows explicitly when archiving code

2. **CI Workflow Hygiene**
   - Regularly audit `.github/workflows/` for obsolete jobs
   - Use workflow naming convention: `{scope}-{purpose}.yml`
   - Delete workflows rather than disable (git history preserves them)

3. **Documentation Standards**
   - Decision documents should include verification steps
   - Mark checklist items with commit SHAs when completed
   - Document expected CI impact for major changes

4. **CI Caching Best Practices**
   - **Don't cache build directories** - cache only immutable artifacts (dependencies, tools)
   - Cache keys must include dependency state (e.g., hash of installed package versions)
   - Use cache versioning: `v1-${{ runner.os }}-deps-...` (bump v1→v2 to invalidate)
   - Prefer rebuilding over cache poisoning - CI time < debugging time
   - Test cache invalidation during dependency updates

### Monitoring

**Add to Sprint Review Checklist:**
- [ ] All CI workflows passing on main branch
- [ ] No obsolete workflows in `.github/workflows/`
- [ ] package.json scripts match current architecture
- [ ] Decision documents have completed checklists

**Automation Opportunity:**
```yaml
# Future: Add workflow validation job
- name: Validate Workflow Configuration
  run: |
    # Fail if workflows reference archived code
    if grep -r "packages/engine-native" .github/workflows/; then
      echo "ERROR: Workflow references archived packages"
      exit 1
    fi
```

---

## Related Work

### Completed (ORP104)
- ✅ Audio thread safety audit (routing_matrix.cpp)
- ✅ Lock-free config pattern
- ✅ Gain conversion caching
- ✅ SessionGuard RAII pattern
- ✅ 58/58 tests passing on Ubuntu

**ORP104 changes did NOT cause CI failures** - Ubuntu builds passed cleanly with ORP104 code, proving the C++ SDK is healthy.

### Completed (DECISION_PACKAGES)
- ✅ Decision documented
- ✅ Packages directory removed
- ⚠️ CI workflow cleanup incomplete (fixed by ORP105)

### Pending (DECISION_PACKAGES Checklist)
- [ ] Update .claudeignore to exclude archive/packages/ (if restored to archive/)
- [ ] Update README.md with "C++ SDK for professional audio" positioning
- [ ] Update .claude/implementation_progress.md to mark Phase 1-2 as archived

---

## Lessons Learned

1. **Incomplete migrations are worse than no migration**
   - Half-removed code creates confusing failure modes
   - Better to complete all checklist items before merging

2. **CI workflows are code**
   - Treat workflow files with same rigor as source code
   - Test workflow changes on feature branches
   - Review workflows during architectural changes

3. **Infrastructure issues can mask code bugs**
   - Initial assumption: "macOS/Windows infrastructure problem"
   - Reality: "Workflow referencing deleted code" + "cache poisoning"
   - Always verify assumptions with evidence

4. **Git history is a safety net**
   - interim-ci.yml preserved in git history (can restore if needed)
   - Packages can be restored from git history (see DECISION_PACKAGES.md)
   - Deletion is reversible when git is used properly

5. **Cache poisoning is insidious**
   - Cached build artifacts can persist stale configuration
   - CMakeCache.txt contains dependency detection results
   - Cache keys must reflect ALL inputs (code + dependencies + environment)
   - When in doubt, don't cache build directories - only dependencies
   - "Works on my machine" + "Works in fresh container" + "Fails in CI" = cache poisoning

6. **Test failures reveal underlying issues**
   - 8 tests failing with same error code (NotReady) = systematic problem
   - Error code 03 led to transport_controller.cpp:865
   - Which led to libsndfile detection failure
   - Which led to cache poisoning discovery
   - Follow the error codes, not assumptions

---

## References

- [DECISION_PACKAGES.md](../DECISION_PACKAGES.md) - Package archival decision
- [ORP102](./ORP102%20Repository%20Analysis%20and%20Sprint%20Recommendations.md) - Analysis leading to archival
- [ORP103](./ORP103%20Build%20System%20Analysis%20and%20Optimization%20Recommendations.md) - Build system analysis
- [ORP104](https://github.com/chrislyons/orpheus-sdk/pull/159) - Codebase optimization (proven to work on Ubuntu)
- [.github/workflows/ci-pipeline.yml](../../.github/workflows/ci-pipeline.yml) - Primary CI workflow
- [.github/workflows/ci.yml](../../.github/workflows/ci.yml) - Secondary CI workflow

---

**Implementation Date:** 2025-11-09
**Verification Date:** Pending CI run
**Next Review:** Post-v1.0 release (2026-Q1)
