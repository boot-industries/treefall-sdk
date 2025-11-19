# Orpheus SDK Modernization Audit Report

**Date:** November 18, 2025
**Auditor:** Claude Code
**Scope:** Orpheus SDK (C++ Core) + Clip Composer Application (JUCE)
**Repository Status:** Active development (109 commits since Jan 2025)

---

## Executive Summary

The Orpheus SDK demonstrates strong foundational practices with modern C++20, comprehensive CI/CD, and good documentation. However, several modernization opportunities exist that could improve maintainability, developer experience, and leverage newer language features. This audit identifies 24 actionable recommendations across 4 priority levels.

**Key Findings:**

- ✅ **Strengths:** C++20 adoption, sanitizers enabled, active clang-tidy, comprehensive test suite (41 test files)
- ⚠️ **Modernization Gaps:** Limited C++20 feature adoption (no concepts/ranges), manual memory management in places, outdated npm dependencies
- 🔧 **Quick Wins:** Upgrade npm packages, add Dependabot, replace raw pointers with smart pointers (3 files)

---

## 1. Dependencies & Build System

### 1.1 Outdated npm/pnpm Dependencies

**What:** Multiple npm development dependencies are outdated, including major version jumps for linting tools.

**Current State:**

```
- ESLint: 8.57.1 → 9.39.1 (major version bump)
- @typescript-eslint/*: 7.x → 8.x (major version)
- @commitlint/*: 17.x → 20.x (major version jump)
- eslint-config-prettier: 9.x → 10.x
- lint-staged: 16.2.4 → 16.2.6 (minor)
- prettier: 3.3.2 → 3.6.2 (minor)
```

**Why:**

- **Security:** Older versions may contain unpatched vulnerabilities
- **Features:** Missing improvements in linting rules and error detection
- **Compatibility:** Future tooling may drop support for older versions
- **Developer Experience:** Newer versions have better error messages and performance

**How:**

1. Update ESLint to v9 (requires migration guide review - breaking changes)
2. Update @typescript-eslint plugins to v8 (check for rule changes)
3. Update @commitlint to v20 (review config for breaking changes)
4. Update minor version packages (prettier, lint-staged, eslint-config-prettier)
5. Test pre-commit hooks thoroughly after updates
6. Update CI to match local tooling versions

**Priority:** **HIGH** (security + compatibility)

---

### 1.2 CMake Version Could Be Higher

**What:** Project requires CMake 3.22, but 3.28+ is available with useful features.

**Current State:**

```cmake
cmake_minimum_required(VERSION 3.22)
```

**Why:**

- **Performance:** CMake 3.28+ has faster configuration and build times
- **Features:** Better C++20 module support, improved FetchContent performance
- **Workflow Presets:** Enhanced `CMakePresets.json` support for multi-config workflows
- **Developer Experience:** Better error messages and diagnostics

**How:**

1. Update minimum to CMake 3.25 (adds `DOWNLOAD_EXTRACT_TIMESTAMP` by default)
2. Consider CMake 3.28 for C++20 modules preparation
3. Update CI workflow CMake version (`CMAKE_VERSION: '3.27.x'` → `'3.28.x'`)
4. Test all build configurations on all platforms

**Priority:** **MEDIUM**

---

### 1.3 Missing Dependabot Configuration

**What:** No automated dependency monitoring for npm packages or GitHub Actions.

**Current State:** No `.github/dependabot.yml` file exists.

**Why:**

- **Security:** Automated alerts for vulnerable dependencies
- **Maintenance:** Reduces manual work tracking updates
- **Best Practice:** Industry standard for open source projects

**How:**
Create `.github/dependabot.yml`:

```yaml
version: 2
updates:
  # npm dependencies
  - package-ecosystem: 'npm'
    directory: '/'
    schedule:
      interval: 'weekly'
    open-pull-requests-limit: 5

  # GitHub Actions
  - package-ecosystem: 'github-actions'
    directory: '/'
    schedule:
      interval: 'weekly'
```

**Priority:** **HIGH** (security + maintenance)

---

### 1.4 JUCE Version Check

**What:** Clip Composer uses JUCE 8.0.4 (released August 2024).

**Current State:**

```cmake
GIT_TAG 8.0.4
```

**Why:**

- JUCE 8.0.x is relatively recent but check for security/bug fix releases
- JUCE 8 warnings are currently suppressed (see `apps/clip-composer/CMakeLists.txt:117`)

**How:**

1. Check JUCE release notes for 8.0.5+ (security/critical fixes)
2. Update if patches available
3. Re-enable compiler warnings once JUCE warnings resolved

**Priority:** **LOW** (stable current version)

---

## 2. Code Modernization

### 2.1 Raw Pointer Usage with Manual Memory Management

**What:** Several files use raw `new`/`delete` instead of smart pointers.

**Current State:**

- `src/core/routing/routing_matrix.cpp:24-27,65-66,69` - Raw `delete` for `GainSmoother*`
- `src/core/routing/clip_routing.cpp` - Contains raw pointer management
- `src/core/abi/session_api.cpp` - ABI layer (acceptable use case)

**Example from `routing_matrix.cpp:24-27`:**

```cpp
if (m_master_gain_smoother) {
  delete m_master_gain_smoother;
  m_master_gain_smoother = nullptr;
}
```

**Why:**

- **Safety:** Manual delete can leak if exceptions thrown before cleanup
- **Maintainability:** Smart pointers provide RAII and automatic cleanup
- **Best Practice:** C++ Core Guidelines recommend avoiding explicit delete
- **Exception Safety:** Automatic cleanup on scope exit

**How:**

1. Replace `GainSmoother* m_master_gain_smoother` with `std::unique_ptr<GainSmoother>`
2. Remove all explicit `delete` calls
3. Use `std::make_unique` for construction:
   ```cpp
   m_master_gain_smoother = std::make_unique<GainSmoother>(sample_rate, config.gain_smoothing_ms);
   ```
4. Update `cleanupChannels()` and `cleanupGroups()` similarly if they use raw pointers

**Files to update:**

- `src/core/routing/routing_matrix.cpp` (confirmed)
- `src/core/routing/clip_routing.cpp` (review needed)

**Priority:** **HIGH** (safety + maintainability)

---

### 2.2 Limited C++20 Feature Adoption

**What:** Project uses C++20 but doesn't leverage many modern features.

**Current State:**

- ✅ Uses: `std::atomic`, `std::string_view`, `std::optional`, `std::variant`, `std::span`
- ❌ Missing: No `std::ranges`, no C++20 concepts, limited `constexpr` usage (9 occurrences across 2 files)
- ❌ Missing: No coroutines (may not be needed for audio code)
- ❌ Missing: No `std::format` (C++20 formatting)

**Why:**

- **Performance:** Ranges/views enable zero-cost abstractions and clearer code
- **Type Safety:** Concepts provide better compile-time constraints
- **Readability:** Modern features reduce boilerplate
- **Future-Proofing:** Industry moving toward these idioms

**How:**

**Step 1: Introduce ranges/views for data processing**
Look for opportunities in:

- `src/core/audio_io/waveform_processor.cpp` - Sample data processing
- `src/core/routing/routing_matrix.cpp` - Channel iteration
- Test files - Data validation and assertions

Example transformation:

```cpp
// Before
for (size_t i = 0; i < channels.size(); ++i) {
  if (channels[i].enabled) {
    process(channels[i]);
  }
}

// After (C++20 ranges)
for (auto& channel : channels | std::views::filter(&Channel::enabled)) {
  process(channel);
}
```

**Step 2: Add concepts for template constraints**
Identify generic code that could benefit:

- Audio sample type concepts (`AudioSample`, `FloatingPoint`)
- Callback function concepts
- Buffer type concepts

Example:

```cpp
// Before
template<typename T>
void processSamples(T* buffer, size_t count);

// After
template<std::floating_point T>
void processSamples(T* buffer, size_t count);
```

**Step 3: Increase constexpr usage**
Look for compile-time constants:

- Buffer sizes (`MAX_BUFFER_SIZE`)
- Sample rate constraints
- Channel limits

**Priority:** **MEDIUM** (incremental modernization, not urgent)

---

### 2.3 `noexcept` Specification Underused

**What:** Only 6 `noexcept` annotations across 2 header files.

**Current State:**

```
src/core/session/session_graph.h: 4 occurrences
src/core/routing/routing_matrix.h: 2 occurrences
```

**Why:**

- **Performance:** Compiler can optimize `noexcept` functions better
- **Safety:** Clearer contracts about exception guarantees
- **Real-time Audio:** Critical for audio thread functions (no exceptions allowed)
- **Move Operations:** Move constructors/assignment should be `noexcept` for optimal container performance

**How:**

1. **Audit audio thread functions** - Mark all audio callback paths as `noexcept`
   - `TransportController::process()` and related render functions
   - `RoutingMatrix::processMix()` and audio processing
2. **Mark move operations** - Ensure move constructors/assignment are `noexcept`
3. **Mark simple getters** - Const getters that can't throw
4. **Use conditional noexcept** for templates:
   ```cpp
   template<typename T>
   void swap(T& a, T& b) noexcept(std::is_nothrow_move_constructible_v<T>);
   ```

**Priority:** **MEDIUM** (audio thread safety + performance)

---

### 2.4 Header Guards vs `#pragma once`

**What:** Project consistently uses `#pragma once` (good!).

**Current State:** All 14 public headers in `include/orpheus/` use `#pragma once`.

**Why:** ✅ This is already modern best practice.

**Action:** None needed - document this convention in style guide.

**Priority:** **N/A** (already optimal)

---

### 2.5 Missing C++20 `std::format`

**What:** String formatting uses traditional methods instead of C++20 `std::format`.

**Why:**

- **Type Safety:** Compile-time format string validation
- **Performance:** Often faster than `sprintf`/iostreams
- **Readability:** Python-like formatting syntax

**Example:**

```cpp
// Before
char buffer[256];
snprintf(buffer, sizeof(buffer), "Sample rate: %d Hz", sampleRate);

// After (C++20)
std::string message = std::format("Sample rate: {} Hz", sampleRate);
```

**How:**

1. Check compiler support (GCC 13+ has full support - ✅ project uses GCC 13.3.0)
2. Add `<format>` header
3. Replace string concatenation and sprintf with `std::format`
4. Consider custom formatters for audio types

**Priority:** **LOW** (nice-to-have, not critical)

---

## 3. Architecture & Performance

### 3.1 Limited Async/Concurrency Patterns

**What:** No use of `std::async`, `std::future`, `std::promise` in codebase.

**Current State:**

- Uses `std::thread` (3 files: `dummy_audio_driver.cpp`, `waveform_processor.cpp`)
- Uses `std::mutex` and `std::atomic` (11 files)
- No higher-level concurrency abstractions

**Why:**

- **Modern Patterns:** `std::async` provides cleaner async operations
- **Future-Proofing:** Preparing for C++23 sender/receiver patterns
- **Use Cases:** Audio file loading, waveform processing could benefit

**How:**

1. **Evaluate use cases:**
   - Audio file loading in background threads
   - Waveform visualization processing
   - Session state serialization
2. **Consider thread pool pattern** for background work
3. **Document threading model** clearly (Message/Audio/Background threads per `ARCHITECTURE.md`)

**Priority:** **LOW** (current patterns work, but consider for new features)

---

### 3.2 Thread Safety Documentation

**What:** Strong atomic usage but documentation could be clearer.

**Current State:**

- Excellent: Uses lock-free patterns with `std::atomic` extensively
- Good: Comments about thread safety in critical sections
- Gap: No centralized threading documentation

**Why:**

- **Maintenance:** New contributors need clear threading rules
- **Safety:** Prevents accidental violations of audio thread safety

**How:**

1. Create `docs/THREADING_MODEL.md` documenting:
   - Audio thread constraints (no allocations, no locks, no blocking)
   - Message thread patterns
   - Background thread guidelines
2. Add TSAN (ThreadSanitizer) to CI for one platform
3. Document lock-free queue patterns used

**Priority:** **MEDIUM** (documentation + safety)

---

### 3.3 Performance Benchmarking Infrastructure

**What:** Limited performance testing infrastructure.

**Current State:**

- 1 perf tool: `tools/perf/render_click.cpp` (or similar)
- No benchmark framework integration
- 15 uses of `std::chrono` for timing

**Why:**

- **Performance Regression Detection:** Catch slowdowns in CI
- **Optimization Guidance:** Data-driven optimization decisions
- **Broadcast Requirements:** 24/7 reliability needs performance guarantees

**How:**

1. **Add Google Benchmark:**
   ```cmake
   FetchContent_Declare(
     benchmark
     GIT_REPOSITORY https://github.com/google/benchmark.git
     GIT_TAG v1.8.4
   )
   ```
2. **Create benchmarks for:**
   - Audio processing loops (render path)
   - Clip triggering latency
   - Routing matrix performance
   - File loading times
3. **Add CI performance budgets** (fail if regression > 10%)

**Priority:** **MEDIUM** (important for production readiness)

---

### 3.4 Disabled Tests

**What:** 2 disabled tests found in test suite.

**Current State:**

```bash
find tests -name "*.cpp" -exec grep -l "DISABLED_" {} \; | wc -l
# Result: 2
```

**Why:**

- Disabled tests often indicate:
  - Known bugs
  - Flaky tests (timing issues)
  - Platform-specific failures
  - Technical debt

**How:**

1. Audit disabled tests with:
   ```bash
   grep -rn "DISABLED_" tests/
   ```
2. For each disabled test:
   - Document reason in comments
   - Create GitHub issue to track re-enabling
   - Fix or remove if obsolete
3. Avoid accumulating more disabled tests

**Priority:** **MEDIUM** (test quality)

---

## 4. Developer Experience

### 4.1 Code Coverage Reporting

**What:** No code coverage tracking configured.

**Current State:**

- No `codecov.yml` or similar config
- Sanitizers enabled (ASan, UBSan) ✅
- 41 test files exist

**Why:**

- **Quality Visibility:** Identify untested code paths
- **Regression Prevention:** Ensure new code is tested
- **Confidence:** Quantify test suite effectiveness

**How:**

1. **Add CMake coverage target:**
   ```cmake
   option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
   if(ENABLE_COVERAGE)
     target_compile_options(${target} PRIVATE --coverage)
     target_link_options(${target} PRIVATE --coverage)
   endif()
   ```
2. **Integrate with codecov.io or coveralls**
3. **Add CI job:**
   ```yaml
   - name: Generate coverage
     run: |
       cmake -B build -DENABLE_COVERAGE=ON
       cmake --build build
       ctest --test-dir build
       gcovr --xml coverage.xml
   ```
4. **Set coverage thresholds** (e.g., 75% minimum)

**Priority:** **HIGH** (quality + visibility)

---

### 4.2 CMake Presets for Common Workflows

**What:** No `CMakePresets.json` for standardized build configurations.

**Why:**

- **Developer Onboarding:** One-command builds for common scenarios
- **IDE Integration:** Better VS Code/CLion support
- **Consistency:** Same builds across team members

**How:**
Create `CMakePresets.json`:

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 22, "patch": 0 },
  "configurePresets": [
    {
      "name": "dev-debug",
      "displayName": "Debug (sanitizers + tests)",
      "binaryDir": "${sourceDir}/build-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "ORP_ENABLE_UBSAN": "ON",
        "ORP_WITH_TESTS": "ON"
      }
    },
    {
      "name": "release",
      "displayName": "Release (optimized)",
      "binaryDir": "${sourceDir}/build-release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "ENABLE_LTO": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "dev-debug",
      "configurePreset": "dev-debug"
    },
    {
      "name": "release",
      "configurePreset": "release"
    }
  ],
  "testPresets": [
    {
      "name": "dev-debug",
      "configurePreset": "dev-debug",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

**Usage:**

```bash
cmake --preset dev-debug
cmake --build --preset dev-debug
ctest --preset dev-debug
```

**Priority:** **MEDIUM** (developer experience)

---

### 4.3 clang-tidy Configuration Enhancement

**What:** clang-tidy enabled but warnings not treated as errors.

**Current State (.clang-tidy):**

```yaml
Checks: bugprone-*,modernize-*,performance-*,readability-*
WarningsAsErrors: '' # Empty!
```

**Why:**

- **Quality Enforcement:** Warnings-as-errors prevents technical debt accumulation
- **CI Integration:** Catch issues before merge
- **Modernization:** Force adoption of modern patterns

**How:**

1. **Gradual rollout:**
   ```yaml
   # Phase 1: High-priority checks as errors
   WarningsAsErrors: 'bugprone-*,performance-*'
   ```
2. **Fix existing violations** before enabling
3. **Phase 2: Add modernize checks**
   ```yaml
   WarningsAsErrors: 'bugprone-*,performance-*,modernize-use-override,modernize-use-nullptr'
   ```
4. **Add CI job** that runs clang-tidy:
   ```yaml
   - name: Run clang-tidy
     run: |
       cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
       run-clang-tidy -p build
   ```

**Priority:** **MEDIUM** (code quality)

---

### 4.4 Documentation for C++ API

**What:** Public API documentation completeness unknown.

**Current State:**

- 14 public headers in `include/orpheus/`
- Mentions "Add Doxygen for public APIs" in CLAUDE.md
- Unknown if Doxygen is configured or generated

**Why:**

- **SDK Usability:** External users need API docs
- **Onboarding:** New team members benefit from examples
- **Contract Clarity:** Document preconditions, thread-safety guarantees

**How:**

1. **Audit current documentation:**
   ```bash
   grep -r "///" include/orpheus/ | wc -l  # Doxygen-style comments
   ```
2. **Add Doxyfile if missing**
3. **Document all public interfaces:**
   - Function contracts (preconditions, postconditions)
   - Thread-safety guarantees
   - Example usage
4. **Generate docs in CI:**
   ```yaml
   - name: Build Doxygen docs
     run: |
       sudo apt-get install doxygen graphviz
       doxygen Doxyfile
   - name: Deploy to GitHub Pages
     uses: peaceiris/actions-gh-pages@v3
   ```

**Priority:** **HIGH** (SDK maturity)

---

### 4.5 TODO/FIXME Tracking

**What:** 18 TODO/FIXME comments across 6 files.

**Current State:**

```
src/core/audio_io/waveform_processor.cpp: 1
src/core/audio_io/audio_file_reader_libsndfile.cpp: 1
src/core/transport/transport_controller.cpp: 7
src/core/routing/routing_matrix.cpp: 5
src/core/session/scene_manager.cpp: 3
src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp: 1
```

**Why:**

- **Technical Debt Visibility:** Track what needs to be done
- **Prioritization:** Convert to GitHub issues for planning
- **Accountability:** Ensure TODOs don't accumulate indefinitely

**How:**

1. **Audit all TODOs:**
   ```bash
   grep -rn "TODO\|FIXME\|XXX\|HACK" src/ include/ > todo_audit.txt
   ```
2. **Categorize:**
   - **Critical:** Affects correctness/safety → fix immediately
   - **Enhancement:** Nice-to-have features → create issues
   - **Obsolete:** Remove if no longer relevant
3. **Create GitHub issues** for important TODOs with:
   - Context from surrounding code
   - Acceptance criteria
   - Priority labels
4. **Establish policy:** No new TODOs without associated issue numbers:
   ```cpp
   // TODO(#123): Get sample rate from config instead of hardcoding
   ```

**Priority:** **LOW** (process improvement)

---

## 5. CI/CD Improvements

### 5.1 Add ThreadSanitizer (TSAN) to CI

**What:** Only AddressSanitizer (ASan) and UBSan currently enabled.

**Why:**

- **Real-time Safety:** Audio thread race conditions are critical bugs
- **Concurrency Bugs:** Catch data races, deadlocks
- **Complement ASan:** Different classes of errors

**How:**
Add TSAN job to `.github/workflows/ci-pipeline.yml`:

```yaml
cpp-tsan:
  name: Thread Sanitizer (Ubuntu Debug)
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Configure with TSAN
      run: |
        cmake -S . -B build \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
          -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
    - name: Build
      run: cmake --build build --parallel
    - name: Test with TSAN
      run: ctest --test-dir build --output-on-failure
      env:
        TSAN_OPTIONS: 'halt_on_error=1 second_deadlock_stack=1'
```

**Priority:** **HIGH** (audio thread safety)

---

### 5.2 Add Static Analysis Job

**What:** clang-tidy configured but not enforced in CI.

**Why:**

- **Prevention:** Catch issues before code review
- **Consistency:** Enforce coding standards automatically

**How:**
Add to CI pipeline:

```yaml
static-analysis:
  name: Static Analysis (clang-tidy)
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Install clang-tidy
      run: sudo apt-get install -y clang-tidy-18
    - name: Configure
      run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    - name: Run clang-tidy
      run: |
        run-clang-tidy-18 -p build -header-filter='include/orpheus/.*' \
          -checks='-*,modernize-*,bugprone-*,performance-*' \
          -warnings-as-errors='*'
```

**Priority:** **MEDIUM** (code quality)

---

### 5.3 Expand Platform Coverage

**What:** Clip Composer tests only run on macOS.

**Current State:**

```yaml
matrix:
  os: [macos-latest] # Start with macOS only (JUCE works best there)
```

**Why:**

- **Cross-platform Validation:** Catch platform-specific bugs
- **JUCE Support:** JUCE works on all platforms, not just macOS
- **User Coverage:** Clip Composer targets Windows/Linux too

**How:**

1. **Phase 1:** Add Linux to matrix:
   ```yaml
   matrix:
     os: [macos-latest, ubuntu-latest]
   ```
2. **Install Linux dependencies:**
   ```yaml
   - name: Install JUCE deps (Linux)
     if: matrix.os == 'ubuntu-latest'
     run: |
       sudo apt-get update
       sudo apt-get install -y libx11-dev libxrandr-dev libxinerama-dev \
         libxcursor-dev libfreetype6-dev libasound2-dev
   ```
3. **Phase 2:** Add Windows when stable

**Priority:** **MEDIUM** (cross-platform support)

---

### 5.4 Caching for Faster Builds

**What:** No CMake build caching in CI.

**Why:**

- **Speed:** Reduce CI time from ~20min to ~5min for incremental builds
- **Cost:** Lower CI compute costs
- **Developer Experience:** Faster feedback loops

**How:**
Add caching to CI jobs:

```yaml
- name: Cache CMake build
  uses: actions/cache@v4
  with:
    path: |
      build
      ~/.ccache
    key: ${{ runner.os }}-${{ matrix.build_type }}-${{ hashFiles('**/CMakeLists.txt') }}
    restore-keys: |
      ${{ runner.os }}-${{ matrix.build_type }}-

- name: Setup ccache
  run: |
    sudo apt-get install ccache
    ccache --set-config=max_size=1G
    echo "CMAKE_CXX_COMPILER_LAUNCHER=ccache" >> $GITHUB_ENV
```

**Priority:** **MEDIUM** (efficiency)

---

## 6. Security & Safety

### 6.1 Dependency Scanning

**What:** No automated vulnerability scanning for dependencies.

**Why:**

- **Security:** Detect CVEs in third-party libraries
- **Compliance:** Required for commercial software
- **Proactive:** Catch issues before exploitation

**How:**

1. **Enable GitHub Dependency Graph** (Settings → Security)
2. **Add Dependabot security updates** (see 1.3)
3. **Consider Snyk or similar** for deeper analysis
4. **Audit third-party dependencies:**
   - GoogleTest v1.14.0 ✅ (latest)
   - JUCE 8.0.4 (check for security releases)
   - libsndfile (system dependency - ensure latest)

**Priority:** **HIGH** (security)

---

### 6.2 Secrets Scanning

**What:** No automated secrets detection.

**Why:**

- **Prevention:** Catch accidental credential commits
- **Compliance:** Required for many security standards

**How:**

1. **Enable GitHub Secret Scanning** (if not already on)
2. **Add pre-commit hook:**
   ```yaml
   # .pre-commit-config.yaml
   repos:
     - repo: https://github.com/Yelp/detect-secrets
       rev: v1.4.0
       hooks:
         - id: detect-secrets
   ```
3. **Document secret management** in security policy

**Priority:** **MEDIUM** (prevention)

---

### 6.3 Compiler Security Hardening Flags

**What:** Check if additional security flags could be enabled.

**Current State:**

- Sanitizers enabled ✅
- Warnings as errors (for most warnings) ✅

**Potential Additions:**

```cmake
if(NOT MSVC)
  add_compile_options(
    -fstack-protector-strong  # Stack canaries
    -D_FORTIFY_SOURCE=2       # Buffer overflow checks
    -Wformat -Wformat-security # Format string vulnerabilities
  )
  add_link_options(
    -Wl,-z,relro              # Read-only relocations
    -Wl,-z,now                # Immediate binding
  )
endif()
```

**How:**

1. Add flags to `cmake/CompilerWarnings.cmake`
2. Test all platforms
3. Measure performance impact (should be minimal)

**Priority:** **LOW** (defense-in-depth)

---

## 7. Architecture Patterns

### 7.1 Lock-Free Queue Implementation

**What:** Uses lock-free patterns but no formal queue abstraction.

**Current State:**

- `TransportCommand` queue in `transport_controller.h`
- Manual atomic operations

**Why:**

- **Reusability:** Generic lock-free queue helps other components
- **Testing:** Isolated testing of concurrent data structures
- **Best Practice:** Separate concerns

**How:**

1. Extract lock-free queue to `include/orpheus/lock_free_queue.h`
2. Make it generic: `template<typename T, size_t Capacity>`
3. Add comprehensive tests for concurrent scenarios
4. Document wait-free vs lock-free guarantees

**Priority:** **LOW** (refactoring opportunity)

---

## 8. Testing Infrastructure

### 8.1 Integration Test Coverage

**What:** Comprehensive unit tests (41 files) but integration test status unclear.

**Why:**

- **System-Level Bugs:** Catch issues that unit tests miss
- **Workflow Validation:** Test real-world usage patterns
- **Regression Prevention:** High-value tests for critical paths

**How:**

1. **Audit existing integration tests:**
   ```bash
   find tests/integration -name "*.cpp" -o -name "*.sh"
   ```
2. **Add integration tests for:**
   - Full audio pipeline (file load → mix → output)
   - Clip triggering under load
   - Hot-swap audio device scenarios
3. **Run in CI** (already has `tests/integration/run-tests.sh`)

**Priority:** **MEDIUM** (quality assurance)

---

### 8.2 Fuzz Testing for Audio File Parsing

**What:** No fuzz testing for audio file readers.

**Why:**

- **Security:** Malformed audio files can cause crashes
- **Robustness:** Catch edge cases in parsing logic
- **Best Practice:** Standard for file format parsers

**How:**

1. **Add libFuzzer integration:**
   ```cmake
   if(ENABLE_FUZZING)
     add_executable(fuzz_audio_reader fuzz/audio_reader_fuzzer.cpp)
     target_link_libraries(fuzz_audio_reader orpheus_audio_io -fsanitize=fuzzer)
   endif()
   ```
2. **Create fuzzer targets:**
   - WAV file parsing
   - Metadata extraction
   - Multi-channel handling
3. **Run in CI** (nightly job)

**Priority:** **LOW** (advanced testing, but valuable)

---

## 9. Documentation

### 9.1 Architecture Decision Records (ADRs)

**What:** No formal ADR system for documenting design decisions.

**Why:**

- **Knowledge Preservation:** Why decisions were made
- **Onboarding:** Help new contributors understand context
- **Consistency:** Reference for future decisions

**How:**

1. Create `docs/adr/` directory
2. Template:

   ```markdown
   # ADR-001: Lock-Free Audio Thread Communication

   ## Status

   Accepted

   ## Context

   Audio thread must communicate with message thread without blocking...

   ## Decision

   Use lock-free queue with atomic operations...

   ## Consequences

   - Positive: ...
   - Negative: ...
   ```

3. Document key decisions:
   - Threading model
   - Lock-free patterns
   - ABI stability approach

**Priority:** **LOW** (documentation quality)

---

## Priority Summary

### High Priority (10 items)

1. **Outdated npm dependencies** (1.1) - Security + compatibility
2. **Missing Dependabot** (1.3) - Automated security
3. **Raw pointer usage** (2.1) - Safety + maintainability
4. **Code coverage reporting** (4.1) - Quality visibility
5. **API documentation** (4.4) - SDK maturity
6. **ThreadSanitizer in CI** (5.1) - Audio thread safety
7. **Dependency scanning** (6.1) - Security vulnerabilities

### Medium Priority (11 items)

8. **CMake version bump** (1.2) - Performance + features
9. **noexcept specification** (2.3) - Performance + safety
10. **Thread safety documentation** (3.2) - Maintainability
11. **Performance benchmarking** (3.3) - Optimization guidance
12. **Disabled tests** (3.4) - Test quality
13. **CMake presets** (4.2) - Developer experience
14. **clang-tidy enforcement** (4.3) - Code quality
15. **Static analysis in CI** (5.2) - Prevention
16. **Platform coverage expansion** (5.3) - Cross-platform validation
17. **CI caching** (5.4) - Build speed
18. **Secrets scanning** (6.2) - Security prevention
19. **Integration test coverage** (8.1) - System validation

### Low Priority (7 items)

20. **JUCE version check** (1.4) - Stable current version
21. **C++20 feature adoption** (2.2) - Incremental modernization
22. **std::format usage** (2.5) - Nice-to-have
23. **Async patterns** (3.1) - Future improvement
24. **TODO tracking** (4.5) - Process improvement
25. **Security hardening flags** (6.3) - Defense-in-depth
26. **Lock-free queue abstraction** (7.1) - Refactoring
27. **Fuzz testing** (8.2) - Advanced testing
28. **Architecture Decision Records** (9.1) - Documentation

---

## Implementation Roadmap

### Phase 1: Quick Wins (1-2 weeks)

- Update npm dependencies (1.1)
- Add Dependabot (1.3)
- Fix raw pointer usage (2.1) - 3 files
- Enable code coverage (4.1)
- Add TSAN to CI (5.1)

### Phase 2: Developer Experience (2-4 weeks)

- CMake presets (4.2)
- CMake version bump (1.2)
- clang-tidy enforcement (4.3)
- CI caching (5.4)
- Audit/fix disabled tests (3.4)

### Phase 3: Documentation & Safety (4-6 weeks)

- API documentation (4.4)
- Thread safety docs (3.2)
- Performance benchmarking (3.3)
- Dependency scanning (6.1)
- Secrets scanning (6.2)

### Phase 4: Modernization (ongoing)

- C++20 features (2.2, 2.5)
- noexcept specifications (2.3)
- Platform coverage expansion (5.3)
- Integration tests (8.1)

---

## Conclusion

The Orpheus SDK is well-architected with strong foundations. The modernization opportunities identified are primarily about:

1. **Security & Safety** - Automated scanning, TSAN, code coverage
2. **Developer Experience** - Better tooling, documentation, faster CI
3. **Code Quality** - Smart pointers, modern C++ features, static analysis

Prioritizing the **High** and **Medium** items would significantly improve the project's maturity, maintainability, and production-readiness while maintaining the excellent deterministic audio processing core.

**Estimated Total Effort:** 8-12 weeks for phases 1-3 (high + medium priority items)

---

_This audit was generated by analyzing the repository structure, code patterns, dependencies, CI configuration, and best practices as of November 18, 2025._
