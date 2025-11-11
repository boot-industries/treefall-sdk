# Fix Windows libsndfile Installation - SourceForge 403 Error

## Problem

Windows CI builds are failing because `vcpkg install libsndfile:x64-windows` tries to download `mp3lame` from SourceForge, which returns HTTP 403 errors (blocks GitHub Actions IPs).

**Error Log:**

```
error: https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download: failed: status code 403
error: building mp3lame:x64-windows failed with: BUILD_FAILED
```

**Root Cause:** SourceForge firewall blocks GitHub Actions runner IPs. This is a known vcpkg issue: https://github.com/microsoft/vcpkg/issues

## Solution Options

### Option A: Use vcpkg Binary Cache (RECOMMENDED)

GitHub Actions has pre-built vcpkg binaries that bypass SourceForge downloads.

**Change in `.github/workflows/ci-pipeline.yml` (Windows install step):**

```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile via vcpkg (using binary cache)..."

    # Use vcpkg binary caching (GitHub Actions pre-built packages)
    # This bypasses SourceForge downloads
    vcpkg install libsndfile:x64-windows --binarysource="clear;nuget,GitHub,readwrite"

    # Verify installation
    if ! vcpkg list | grep -q libsndfile; then
      echo "ERROR: libsndfile installation failed"
      echo "Installed packages:"
      vcpkg list
      exit 1
    fi

    echo "✓ libsndfile installed successfully"
    vcpkg list | grep libsndfile

    # Set toolchain file for CMake
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
  env:
    VCPKG_BINARY_SOURCES: 'clear;nuget,GitHub,readwrite'
```

**Why This Works:**

- `--binarysource` tells vcpkg to use pre-built binaries from GitHub's cache
- Skips compilation (and SourceForge downloads)
- Much faster than building from source
- Officially supported by GitHub Actions

---

### Option B: Install from Pre-Built Binaries (Alternative)

Download libsndfile directly from official releases, skip vcpkg.

**Change in `.github/workflows/ci-pipeline.yml`:**

```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile from official release..."

    # Download pre-built Windows binaries
    curl -L -o libsndfile-1.2.2-win64.zip https://github.com/libsndfile/libsndfile/releases/download/1.2.2/libsndfile-1.2.2-win64.zip

    # Extract to C:\libsndfile
    powershell -Command "Expand-Archive -Path libsndfile-1.2.2-win64.zip -DestinationPath C:\libsndfile"

    # Set CMake paths
    echo "LIBSNDFILE_ROOT=C:\libsndfile" >> $GITHUB_ENV
    echo "CMAKE_PREFIX_PATH=C:\libsndfile" >> $GITHUB_ENV

    # Verify
    if [ ! -f "C:/libsndfile/bin/sndfile.dll" ]; then
      echo "ERROR: libsndfile installation failed"
      exit 1
    fi

    echo "✓ libsndfile installed successfully"
  shell: bash
```

**Also update CMake configure step:**

```yaml
- name: Configure CMake
  run: |
    cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
      -G "${{ matrix.generator }}" \
      ${{ matrix.os == 'windows-latest' && '-DCMAKE_PREFIX_PATH=C:/libsndfile' || '' }} \
      ... (rest of flags)
```

**Why This Works:**

- Direct download from GitHub releases (no SourceForge)
- Official libsndfile binaries
- No vcpkg complexity

**Downside:**

- Need to manually update version when libsndfile releases new version
- Need to ensure CMake can find the library

---

### Option C: Skip mp3lame Feature (Minimal Fix)

Tell vcpkg to install libsndfile WITHOUT mp3lame support.

**Change in `.github/workflows/ci-pipeline.yml`:**

```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    # Install libsndfile with minimal features (no mp3)
    vcpkg install "libsndfile[core]:x64-windows"

    # Set toolchain file
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
```

**Why This Works:**

- `[core]` feature set excludes optional dependencies like mp3lame
- We don't need MP3 encoding/decoding for SDK tests
- Avoids SourceForge entirely

**Downside:**

- Might be missing some libsndfile features (but we only need WAV/FLAC)

---

## Recommendation

**Use Option A (vcpkg binary cache)** - It's the cleanest solution and officially supported.

If that fails, fall back to Option C (skip mp3lame feature).

---

## macOS Test Failures (Separate Issue)

macOS builds are actually working (56/58 tests passing)! Only 2-3 tests failing:

1. `multi_clip_stress_test` - Known flaky test (pre-existing)
2. `coreaudio_driver_test` (Debug only) - May need investigation
3. `transport_integration_test` (Release only) - May need investigation

**These are NOT infrastructure issues** - they're legitimate test failures to investigate separately.

**Action:** Don't block on macOS - focus on Windows fix first. macOS is 96.5% passing.

---

## Implementation Steps

1. **Update `.github/workflows/ci-pipeline.yml`** (Windows install step)
2. **Remove** the `--debug` flag from vcpkg (added in ORP105 for troubleshooting)
3. **Test** on PR #161 branch
4. **Verify** CI passes on Windows
5. **Document** in ORP105 if successful

---

## Quick Fix (Copy-Paste)

Replace lines 73-90 in `.github/workflows/ci-pipeline.yml`:

```yaml
- name: Install libsndfile (Windows)
  if: matrix.os == 'windows-latest'
  run: |
    echo "Installing libsndfile via vcpkg (using binary cache)..."
    vcpkg install libsndfile:x64-windows --binarysource="clear;nuget,GitHub,readwrite"

    if ! vcpkg list | grep -q libsndfile; then
      echo "ERROR: libsndfile installation failed"
      vcpkg list
      exit 1
    fi

    echo "✓ libsndfile installed successfully"
    vcpkg list | grep libsndfile
    echo "CMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake" >> $GITHUB_ENV
  shell: bash
  env:
    VCPKG_BINARY_SOURCES: 'clear;nuget,GitHub,readwrite'
```

Done!
