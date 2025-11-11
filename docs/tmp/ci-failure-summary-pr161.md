# CI Failure Summary for PR #161 - Latest Run

**Run ID:** 19212258843 (Orpheus SDK CI Pipeline)
**Date:** 2025-11-09 17:54-17:57 UTC

---

## 🔴 Windows Failures (Both Debug and Release)

**Status:** FAILED - Still hitting SourceForge 403 error

**Root Cause:** The binary cache fix (`--binarysource=clear`) is NOT bypassing SourceForge. It's still trying to build mp3lame from source.

### Error Log:

```
Installing libsndfile via vcpkg (using binary cache pull only)...
vcpkg install libsndfile:x64-windows --binarysource=clear

Downloading lame-3.100.tar.gz, trying https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download
error: https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download: failed: status code 403
error: building mp3lame:x64-windows failed with: BUILD_FAILED
```

**Problem:** `--binarysource=clear` tells vcpkg to SKIP binary cache and build from source! That's the opposite of what we want.

**Correct Fix:**

```yaml
# WRONG (current):
vcpkg install libsndfile:x64-windows --binarysource=clear

# RIGHT (should be):
vcpkg install "libsndfile[core]:x64-windows"
# OR
vcpkg install libsndfile:x64-windows --binarysource="clear;files,C:/vcpkg/binary-cache,readwrite"
```

**Option 1 (Minimal - RECOMMENDED):**
Install libsndfile WITHOUT mp3 support (we don't need it for SDK tests):

```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile (core features only, no mp3)..."
    vcpkg install "libsndfile[core]:x64-windows"

    if ! vcpkg list | grep -q libsndfile; then
      echo "ERROR: libsndfile installation failed"
      vcpkg list
      exit 1
    fi

    echo "✓ libsndfile installed successfully"
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
```

**Why this works:** `[core]` feature excludes optional dependencies like mp3lame, avoiding SourceForge entirely.

---

## ⚠️ macOS Failures (NOT Infrastructure Issues)

### macOS Release (1 test failing):

**Failed Test:** `multi_clip_stress_test`

**Sub-tests:**

1. ✅ `RapidStartStop` - PASSED
2. ❌ `SixteenSimultaneousClips` - **FAILED**
   - Expected: callback_accuracy > 70%
   - Actual: 26.97%
   - Audio callbacks: 1517 (expected: 5625)
3. ❌ `CPUUsageMeasurement` - **FAILED**
   - Expected: callback_accuracy > 80%
   - Actual: 38.4%
   - Callbacks in 2 seconds: 72 (expected: 187.5)
4. ✅ `MemoryUsageTracking` - PASSED

**Error Messages:**

```
/Users/runner/work/orpheus-sdk/orpheus-sdk/tests/transport/multi_clip_stress_test.cpp:304: Failure
Expected: (callback_accuracy) > (70.0), actual: 26.968888888888888 vs 70
Callback accuracy should be >70%

/Users/runner/work/orpheus-sdk/orpheus-sdk/tests/transport/multi_clip_stress_test.cpp:429: Failure
Expected: (callback_accuracy) > (80.0), actual: 38.4 vs 80
```

**Analysis:**

- This is a **timing/performance issue**, not infrastructure
- The dummy audio driver is calling callbacks too infrequently
- Only getting ~27-38% of expected callbacks
- This is a REAL TEST FAILURE (not a CI problem)

**56/58 tests passing** - Only `multi_clip_stress_test` failing

---

### macOS Debug (2 tests failing):

**Failed Tests:**

1. `multi_clip_stress_test` - Same issue as Release (callback accuracy)
2. `coreaudio_driver_test` - **FAILED: CallbackIsInvoked test**

**CoreAudio Driver Test Failure:**

```
[ RUN      ] CoreAudioDriverTest.CallbackIsInvoked
/Users/runner/work/orpheus-sdk/orpheus-sdk/tests/audio_io/coreaudio_driver_test.cpp:238: Failure
Expected: (m_callback->getCallCount()) > (5), actual: 1 vs 5

[  FAILED  ] CoreAudioDriverTest.CallbackIsInvoked (681 ms)
```

**Analysis:**

- Same callback timing issue as `multi_clip_stress_test`
- Expected: >5 callbacks in 681ms
- Actual: Only 1 callback
- This is a REAL TEST FAILURE (timing/callback frequency issue)

**56/58 tests passing** - Only these 2 timing-related tests failing

---

## Summary for Claude Code Web

### Windows (BLOCKING):

**Issue:** Binary cache fix is WRONG. `--binarysource=clear` means "skip cache, build from source" which is what's causing the SourceForge error.

**Action Required:**

1. Change Windows install step to use `libsndfile[core]:x64-windows` (minimal features, no mp3)
2. This avoids mp3lame dependency entirely
3. We don't need MP3 encoding/decoding for SDK tests (only WAV/FLAC)

**File:** `.github/workflows/ci-pipeline.yml` lines ~73-90

---

### macOS (NOT BLOCKING - Real Test Issues):

**Issue:** The tests are RUNNING correctly (libsndfile working, no infrastructure problems), but `multi_clip_stress_test` has a legitimate timing bug.

**Analysis:**

- Only 27-38% of expected audio callbacks are firing
- This suggests the dummy audio driver isn't calling back frequently enough
- OR the test expectations are too strict for CI environment
- NOT an infrastructure issue - this is actual test logic that needs fixing

**Action Required:**

1. ✅ Accept macOS as "mostly working" (56/58 = 96.5%)
2. ⚠️ File separate issue for `multi_clip_stress_test` callback accuracy
3. ⚠️ Investigate `coreaudio_driver_test` failure (Debug only)

**These are CODE ISSUES, not CI infrastructure issues.**

---

## Next Steps for Claude Code Web

**Priority 1: Fix Windows**

```yaml
# Replace the Windows libsndfile install step in .github/workflows/ci-pipeline.yml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile (core features only, no mp3)..."
    vcpkg install "libsndfile[core]:x64-windows"

    if ! vcpkg list | grep -q libsndfile; then
      echo "ERROR: libsndfile installation failed"
      vcpkg list
      exit 1
    fi

    echo "✓ libsndfile installed successfully"
    vcpkg list | grep libsndfile
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
```

**Priority 2: Accept macOS Limitations**

- Don't try to fix `multi_clip_stress_test` in this PR (it's a separate test bug)
- Document that 56/58 tests passing is acceptable
- CI will show "mostly green" which is fine

**Expected Result After Fix:**

- ✅ Windows: All tests passing (libsndfile installed correctly)
- ⚠️ macOS: 56/58 passing (known test timing issues, separate from infrastructure)
- ✅ Ubuntu: All tests passing (already working)

---

## Why Previous Fix Didn't Work

**Problem:** Misunderstood `--binarysource` flag

```yaml
# CURRENT (WRONG):
vcpkg install libsndfile:x64-windows --binarysource=clear
#                                    ^^^^^^^^^^^^^^^^^^^
#                                    This means "don't use binary cache"
#                                    Forces building from source (SourceForge)

# CORRECT:
vcpkg install "libsndfile[core]:x64-windows"
#             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#             Install only core features (no mp3lame)
```

The `[core]` feature set is the minimal libsndfile without optional codecs. We don't need MP3 support for SDK tests.
