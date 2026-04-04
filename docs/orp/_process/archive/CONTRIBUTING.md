# Contributing to Orpheus SDK

**Version:** 1.0
**Last Updated:** October 26, 2025

Thank you for your interest in contributing to the Orpheus SDK! This guide will help you get started with development, understand our standards, and submit high-quality contributions.

---

## Table of Contents

- [Quick Start](#quick-start)
- [Development Setup](#development-setup)
- [Code Standards](#code-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Documentation Requirements](#documentation-requirements)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Branch Naming Conventions](#branch-naming-conventions)
- [Code Review Process](#code-review-process)
- [Getting Help](#getting-help)

---

## Quick Start

**For first-time contributors:**

1. **Fork and clone:**

   ```bash
   git clone https://github.com/YOUR_USERNAME/orpheus-sdk.git
   cd orpheus-sdk
   ```

2. **Bootstrap the development environment:**

   ```bash
   ./scripts/bootstrap-dev.sh
   ```

3. **Verify your setup:**

   ```bash
   # C++ build + tests
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build --output-on-failure

   # JavaScript/TypeScript (if working on drivers/UI)
   pnpm install
   pnpm run lint
   pnpm run build
   ```

4. **Create a feature branch:**

   ```bash
   git checkout -b feat/your-feature-name
   ```

5. **Make your changes, commit, and push:**

   ```bash
   git add .
   git commit -m "feat: add your feature description"
   git push origin feat/your-feature-name
   ```

6. **Open a Pull Request** on GitHub

---

## Development Setup

### Prerequisites

**Required:**

- **C++ Toolchain:**
  - CMake 3.22+
  - C++20-capable compiler (MSVC 2019+, Clang 13+, or GCC 11+)
  - Ninja or Make (optional, for faster builds)

- **JavaScript/TypeScript (for driver/UI work):**
  - Node.js 20+
  - pnpm 8.15.4 (managed via `packageManager` in `package.json`)

**Recommended:**

- **libsndfile** – For audio file I/O (WAV, AIFF, FLAC support)
  - macOS: `brew install libsndfile`
  - Windows: `vcpkg install libsndfile`
  - Linux: `sudo apt-get install libsndfile1-dev`

- **JUCE 8.0.4** – For building OCC application (automatically fetched by CMake)

### C++ Development

**Configure and build:**

```bash
# Debug build with sanitizers (recommended for development)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release build (optimized, no sanitizers)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# With OCC application
cmake -S . -B build -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
cmake --build build
```

**Build options:**

- `ORPHEUS_ENABLE_REALTIME` – Enable real-time audio modules (default: ON)
- `ORPHEUS_ENABLE_APP_CLIP_COMPOSER` – Build OCC application (default: OFF)
- `ORPHEUS_ENABLE_APP_JUCE_HOST` – Build JUCE demo host (default: OFF)
- `ORPHEUS_BUILD_EXAMPLES` – Build example applications (default: OFF)

### JavaScript/TypeScript Development

**Install dependencies:**

```bash
pnpm install
```

**Build packages:**

```bash
# Build all packages
pnpm run build

# Build specific package
pnpm --filter @orpheus/contract run build
pnpm --filter @orpheus/engine-service run build
```

**Run development server (Shmui UI):**

```bash
pnpm --filter www dev
# Visit http://localhost:4000
```

---

## Code Standards

### C++ Style Guide

We follow a modified LLVM style enforced by `.clang-format`.

**Key rules:**

- **Indentation:** 2 spaces (no tabs)
- **Line length:** 100 characters (soft limit)
- **Naming:**
  - Classes/Structs: `PascalCase` (e.g., `SessionGraph`, `TransportController`)
  - Functions/Methods: `camelCase` (e.g., `startClip()`, `processAudio()`)
  - Variables: `snake_case` (e.g., `clip_handle`, `sample_rate`)
  - Constants: `SCREAMING_SNAKE_CASE` (e.g., `MAX_CLIPS`, `DEFAULT_BUFFER_SIZE`)
- **Bracing:** Opening brace on same line for functions/classes
- **Pointers/References:** `int* ptr` (asterisk attached to type)

**Before committing C++ code:**

```bash
# Auto-format all C++ files
clang-format -i src/**/*.cpp include/**/*.h

# Check formatting (CI gate)
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h
```

**Static analysis (recommended):**

```bash
# Configure with compile commands
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Run clang-tidy
clang-tidy -p build src/core/transport/transport_controller.cpp
```

### TypeScript/JavaScript Style Guide

We use ESLint + Prettier for JavaScript/TypeScript.

**Key rules:**

- **Indentation:** 2 spaces
- **Quotes:** Single quotes for strings (except to avoid escaping)
- **Semicolons:** Required
- **Line length:** 100 characters (soft limit)
- **Naming:**
  - Classes/Types: `PascalCase`
  - Functions/Variables: `camelCase`
  - Constants: `SCREAMING_SNAKE_CASE`

**Before committing JS/TS code:**

```bash
# Auto-fix linting issues
pnpm run lint:fix

# Check linting (CI gate)
pnpm run lint
```

### Audio Code Standards (CRITICAL)

**All audio thread code MUST follow broadcast-safe rules:**

#### ✅ Allowed in Audio Thread

- Read atomic values (`std::atomic<T>::load()`)
- CPU-bound calculations (fade curves, gain conversion)
- Lock-free data structures (if pre-allocated)

#### ❌ FORBIDDEN in Audio Thread

- Memory allocation (`new`, `malloc`, `std::vector::push_back()`)
- Locks (`std::mutex::lock()`, `std::condition_variable::wait()`)
- File I/O (`fread()`, `fwrite()`, `fopen()`)
- Network I/O (HTTP, WebSocket, TCP/UDP)
- System calls (blocking operations)
- Debug logging to console/files

**Verification:**

Use the `.claude/skills/rt.safety.auditor.sh` skill to check audio code:

```bash
./.claude/skills/rt.safety.auditor.sh src/core/transport/transport_controller.cpp
```

---

## Testing Requirements

All contributions must include tests and pass existing tests.

### C++ Testing

**Run full test suite:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

**Run specific test:**

```bash
ctest --test-dir build -R clip_gain_test -VV
```

**Test with sanitizers (required for audio code):**

```bash
# AddressSanitizer (memory safety)
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build

# UBSanitizer (undefined behavior)
# Enabled automatically in Debug builds
```

**Writing new tests:**

- Use GoogleTest framework
- Place in `tests/` directory (organized by module)
- Update `tests/CMakeLists.txt` to include new test
- Aim for >85% code coverage

**Example test structure:**

```cpp
#include <gtest/gtest.h>
#include <orpheus/transport_controller.h>

TEST(TransportControllerTest, StartClip_ValidHandle_ReturnsSuccess) {
    // Arrange
    auto transport = orpheus::createTransportController(48000, 512);

    // Act
    auto result = transport->startClip(reader, 0, 48000);

    // Assert
    EXPECT_NE(result, orpheus::INVALID_CLIP_HANDLE);
}
```

### JavaScript/TypeScript Testing

**Run tests:**

```bash
# All packages
pnpm test

# Specific package
pnpm --filter @orpheus/contract test
```

**Writing new tests:**

- Use Vitest framework (or Jest for Node.js packages)
- Place tests adjacent to source files (e.g., `src/foo.test.ts`)
- Aim for >80% coverage

---

## Pull Request Process

### Before Opening a PR

1. **Update your branch:**

   ```bash
   git checkout main
   git pull upstream main
   git checkout feat/your-feature
   git rebase main
   ```

2. **Run pre-commit checks:**

   ```bash
   # C++ formatting
   clang-format --dry-run --Werror src/**/*.cpp include/**/*.h

   # C++ tests
   ctest --test-dir build --output-on-failure

   # JS/TS linting
   pnpm run lint

   # JS/TS tests
   pnpm test
   ```

3. **Update documentation** (if applicable):
   - Update `ARCHITECTURE.md` for architectural changes
   - Update `CHANGELOG.md` for user-facing changes
   - Add/update Doxygen comments for public APIs
   - Update integration guides if APIs changed

### Opening the PR

Use our PR template (auto-populated on GitHub):

**Title format:**

```
feat: add clip loop mode API
fix: resolve heap-use-after-free in SessionGuard
docs: update Transport Integration Guide
```

**Description must include:**

- **Summary:** What does this PR do? (1-2 sentences)
- **Motivation:** Why is this change needed?
- **Changes:** Bullet list of specific modifications
- **Testing:** How was this tested? (manual tests, unit tests)
- **Breaking Changes:** Any API changes that affect existing code?
- **Documentation:** What docs were updated?

**Checklist (must complete):**

- [ ] Code follows style guidelines (clang-format, ESLint)
- [ ] Tests added/updated (unit tests, integration tests)
- [ ] All tests pass locally
- [ ] Documentation updated (API docs, guides, CHANGELOG)
- [ ] No sanitizer violations (AddressSanitizer, UBSanitizer)
- [ ] Commit messages follow conventional commits
- [ ] Branch is up-to-date with `main`

### PR Review Process

**Typical timeline:**

- **Initial review:** 1-2 business days
- **Feedback cycles:** 1-2 days per round
- **Approval & merge:** 1 day after final approval

**What reviewers check:**

1. **Code quality:** Follows style guide, well-structured, readable
2. **Testing:** Adequate test coverage, tests pass
3. **Documentation:** APIs documented, guides updated
4. **Performance:** No regressions, meets performance budgets
5. **Safety:** Audio code is broadcast-safe, no sanitizer violations
6. **API design:** Consistent with existing APIs, future-proof

**Addressing feedback:**

- Make requested changes in new commits (don't force-push during review)
- Reply to comments when changes are made
- Mark conversations as resolved when addressed
- Re-request review after addressing feedback

---

## Documentation Requirements

### API Documentation (Doxygen)

All public APIs must have complete Doxygen comments.

**Required elements:**

- `@brief` – One-sentence summary
- `@param` – Each parameter with description, type, range, constraints
- `@return` – Return value semantics, error codes
- `@note` – Thread safety, performance notes, platform-specific behavior
- `@code` – Example usage (inline code blocks)
- `@see` – Related functions

**Example:**

```cpp
/// @brief Set per-clip gain adjustment in decibels
///
/// This method updates the gain applied to a clip during audio processing.
/// The gain change is applied atomically and takes effect in the next audio
/// callback. Negative values attenuate, positive values amplify.
///
/// @param clip_handle Handle to the clip (obtained from startClip)
/// @param gain_db Gain in decibels (range: -96.0 to +12.0 dB)
///                Values outside this range are clamped.
///
/// @return SessionGraphError::Success on success
/// @return SessionGraphError::ClipNotFound if clip_handle is invalid
///
/// @note Thread-safe: Can be called from UI thread while audio thread is running
/// @note Gain is applied in linear domain: gain_linear = pow(10, gain_db / 20)
///
/// @code
/// // Example: Set clip to -6dB (half amplitude)
/// auto result = transport->updateClipGain(clip_handle, -6.0f);
/// if (result == SessionGraphError::Success) {
///     std::cout << "Gain updated successfully" << std::endl;
/// }
/// @endcode
///
/// @see getClipGain(), updateClipFades()
SessionGraphError updateClipGain(ClipHandle clip_handle, float gain_db);
```

### Integration Guides

When adding new features that affect SDK usage, update integration guides:

- `docs/QUICK_START.md` – If setup process changes
- `docs/integration/TRANSPORT_INTEGRATION.md` – For transport/playback features
- `docs/platform/CROSS_PLATFORM.md` – For platform-specific behavior

### Changelog

Update `CHANGELOG.md` (or app-specific changelog) for user-facing changes:

**Format:**

```markdown
## [Unreleased]

### Added

- New `updateClipGain()` API for per-clip volume control

### Changed

- `startClip()` now returns `ClipHandle` instead of `uint32_t`

### Fixed

- Resolved heap-use-after-free in SessionGuard move assignment

### Deprecated

- `setGlobalGain()` deprecated in favor of per-clip gain

### Removed

- Removed legacy `renderToFile()` method (use `renderClick()` instead)
```

---

## Commit Message Guidelines

We follow **Conventional Commits** (enforced by commitlint).

### Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

### Types

- `feat` – New feature for the user
- `fix` – Bug fix for the user
- `docs` – Documentation only changes
- `style` – Code style changes (formatting, no logic change)
- `refactor` – Code refactoring (no functional change)
- `perf` – Performance improvements
- `test` – Adding or updating tests
- `build` – Build system or dependency changes
- `ci` – CI configuration changes
- `chore` – Other changes (e.g., release prep)

### Scopes (optional)

- `core` – Core SDK (`src/core/`)
- `transport` – Transport controller
- `drivers` – Audio drivers
- `contract` – Contract/schema system
- `occ` – Orpheus Clip Composer
- `ci` – CI/CD pipeline
- `docs` – Documentation

### Examples

**Good commit messages:**

```
feat(transport): add loop mode API for sample-accurate restart

Implements setClipLoopMode() to enable seamless looping at trim IN point.
Audio thread checks loopEnabled flag and seeks to trim IN when reaching
trim OUT boundary.

Closes #42
```

```
fix(core): resolve heap-use-after-free in SessionGuard

SessionGuard lacked move semantics, causing double-free during move
assignment. Added proper move constructor/assignment with ownership
transfer and deleted copy operations.

Discovered by AddressSanitizer during ORP077 Task 1.3.
```

```
docs: update ARCHITECTURE.md with driver layer

Documents Service/Native/WASM driver architecture from ORP068 Phases 1-2.
Includes threading model, contract system, and future architecture sections.
```

**Bad commit messages:**

```
Update code          # Too vague, no type, no description
fix bug              # Which bug? What was the fix?
WIP                  # Work-in-progress commits should be squashed before PR
```

### Pre-commit Hook

We use Husky to enforce commit message format:

```bash
# Husky automatically installed by pnpm install
# Validates commit messages on commit-msg hook
```

---

## Branch Naming Conventions

**Format:** `<type>/<short-description>`

**Types:**

- `feat/` – New feature branches
- `fix/` – Bug fix branches
- `docs/` – Documentation branches
- `refactor/` – Refactoring branches
- `test/` – Test addition branches
- `chore/` – Maintenance branches

**Examples:**

```
feat/clip-loop-mode
fix/session-guard-double-free
docs/architecture-update
refactor/transport-cleanup
test/add-determinism-tests
```

**Branch lifecycle:**

1. Create from `main`: `git checkout -b feat/your-feature`
2. Work and commit regularly
3. Push to your fork: `git push origin feat/your-feature`
4. Open PR against `upstream/main`
5. After merge, delete branch: `git branch -d feat/your-feature`

---

## Code Review Process

### For Contributors

**During review:**

- Be responsive to feedback (aim for <2 day response time)
- Ask questions if feedback is unclear
- Explain your design choices when challenged
- Be open to alternative approaches

**Addressing feedback:**

- Make changes in new commits (preserve review history)
- Reply to each comment when addressed
- Mark conversations as resolved
- Re-request review after updates

**If stuck:**

- Tag maintainers in comments
- Ask in GitHub Discussions
- Join community chat (if available)

### For Reviewers

**What to check:**

1. **Correctness:** Does the code do what it claims?
2. **Style:** Follows clang-format/ESLint?
3. **Tests:** Adequate coverage, tests pass?
4. **Documentation:** APIs documented, guides updated?
5. **Performance:** No obvious regressions?
6. **Safety:** Audio code is broadcast-safe?
7. **Design:** Consistent with existing patterns?

**Review etiquette:**

- Be constructive and specific
- Explain _why_ when requesting changes
- Acknowledge good work
- Distinguish between "required" and "optional" feedback

**Approval criteria:**

- ✅ All tests pass
- ✅ Code follows style guidelines
- ✅ Documentation complete
- ✅ No sanitizer violations
- ✅ At least one maintainer approval

---

## Getting Help

### Resources

- **Documentation:**
  - [Quick Start Guide](QUICK_START.md)
  - [Architecture Overview](../ARCHITECTURE.md)
  - [Transport Integration Guide](integration/TRANSPORT_INTEGRATION.md)

- **Implementation Plans:**
  - [ORP068 - SDK Integration Plan](integration/ORP068%20Implementation%20Plan%20v2.0%20-%20Orpheus%20Driver%20Architecture%20%2B%20Contract%20System.md)
  - [ORP077 - SDK Core Quality Sprint](ORP/ORP077.md)

- **Examples:**
  - [Simple Clip Player](../examples/simple_player/)
  - [Multi-Clip Trigger](../examples/multi_clip_trigger/)
  - [Offline Renderer](../examples/offline_renderer/)

### Communication Channels

- **GitHub Issues:** Bug reports, feature requests
- **GitHub Discussions:** General questions, design discussions
- **Pull Requests:** Code contributions, reviews

### Reporting Issues

**Good issue reports include:**

1. **Title:** Clear, descriptive summary
2. **Description:** What is the problem?
3. **Steps to reproduce:** Minimal code example
4. **Expected behavior:** What should happen?
5. **Actual behavior:** What actually happens?
6. **Environment:** OS, compiler, SDK version
7. **Logs/Screenshots:** Error messages, stack traces

**Example:**

```markdown
## Bug: Heap-use-after-free in minhost after successful render

**Description:**
minhost crashes with SIGABRT after successfully rendering a click track.

**Steps to Reproduce:**

1. Build SDK in Debug mode with ASAN
2. Run: `./build/adapters/minhost/orpheus_minhost --session tools/fixtures/solo_click.json --render out.wav`
3. Observe crash after WAV file is created

**Expected:** Clean exit with code 0
**Actual:** Crash with exit code 134 (SIGABRT)

**Environment:**

- OS: macOS 14.6
- Compiler: Apple Clang 15.0
- SDK Version: commit d8910fce

**Logs:**
```

AddressSanitizer: heap-use-after-free on address 0x000102e04e20
READ of size 8 at 0x000102e04e20 thread T0
#0 0x102d70abc in SessionGuard::~SessionGuard() minhost/main.cpp:290

```

```

---

## License

By contributing to Orpheus SDK, you agree that your contributions will be licensed under the MIT License.

---

**Questions?** Open a [GitHub Discussion](https://github.com/orpheus-sdk/orpheus-sdk/discussions) or comment on an issue.

**Ready to contribute?** Check out our [Good First Issues](https://github.com/orpheus-sdk/orpheus-sdk/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22) to get started!
