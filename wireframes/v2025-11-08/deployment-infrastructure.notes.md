# Deployment Infrastructure Notes

## Overview

The Orpheus SDK uses a comprehensive CI/CD pipeline built on GitHub Actions to ensure code quality, cross-platform compatibility, and reliable releases. This document describes the build system, testing infrastructure, and deployment process.

## Development Environment

### Local Development Setup

**Prerequisites:**
- **C++ Compiler:** clang 15+ (macOS), GCC 11+ (Linux), MSVC 2022 (Windows)
- **CMake:** 3.20 or newer
- **Node.js:** 18+ with pnpm package manager
- **Git:** 2.30+

**One-command setup:**
```bash
./scripts/bootstrap-dev.sh
```

This script:
1. Checks for required tools
2. Installs dependencies (libsndfile, JUCE, etc.)
3. Configures CMake with recommended settings
4. Builds the SDK and tests
5. Runs initial validation

**Manual setup:**
```bash
# 1. Install dependencies (macOS)
brew install cmake libsndfile pkg-config

# 2. Install Node.js dependencies
pnpm install

# 3. Configure CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 4. Build SDK
cmake --build build

# 5. Run tests
ctest --test-dir build --output-on-failure
```

**Build types:**
- **Debug** - Sanitizers enabled, assertions enabled, debug symbols
- **Release** - Optimizations enabled, no assertions, minimal symbols
- **RelWithDebInfo** - Optimizations + debug symbols (for profiling)

### Development Workflow

**1. Create feature branch:**
```bash
git checkout -b feature/my-new-feature
```

**2. Make changes and test locally:**
```bash
# Incremental build
cmake --build build --target my_file.cpp.o

# Run specific test
ctest --test-dir build -R my_test -VV

# Run all tests
ctest --test-dir build --output-on-failure
```

**3. Format code:**
```bash
# Format C++ files
clang-format -i src/core/my_file.cpp

# Format TypeScript files
pnpm run format
```

**4. Run linters:**
```bash
# C++ linting
clang-tidy src/core/my_file.cpp

# TypeScript linting
pnpm run lint
```

**5. Commit and push:**
```bash
git add .
git commit -m "feat: add new feature"
git push origin feature/my-new-feature
```

**6. Create pull request:**
- GitHub automatically runs CI pipeline
- Address any failures before requesting review
- Once approved and tests pass, merge to main

## Version Control (GitHub)

### Repository Structure

**Main branch:**
- Always stable, deployable
- Protected (requires PR + CI pass)
- No direct commits

**Feature branches:**
- Naming: `feature/description`, `fix/bug-name`, `docs/update-name`
- Merged via PR after review
- Deleted after merge

**Claude branches:**
- Naming: `claude/*` (AI-assisted development)
- Automatically created by Claude Code sessions
- Follow same PR process as feature branches

### Branch Protection Rules

**`main` branch requirements:**
1. ✅ CI pipeline must pass (all jobs)
2. ✅ At least 1 approving review (for core changes)
3. ✅ Status checks must succeed
4. ✅ Branch must be up to date with main
5. ✅ No force pushes

**Override:** Repository admins can force-merge (emergency hotfixes only)

### Release Process

**1. Prepare release:**
```bash
# Update version in relevant files
# - CMakeLists.txt (project VERSION)
# - package.json (version field)
# - CHANGELOG.md (new section)

# Commit version bump
git commit -am "chore: bump version to 1.0.0"
git push
```

**2. Create release tag:**
```bash
git tag -a v1.0.0 -m "Release v1.0.0"
git push origin v1.0.0
```

**3. GitHub Actions automatically:**
- Builds release artifacts for all platforms
- Publishes npm packages to registry
- Generates documentation and publishes to GitHub Pages
- Creates GitHub Release with assets

**4. Post-release:**
- Announce on Discord/Slack/Twitter
- Update documentation links
- Monitor for issues

**Versioning scheme (Semantic Versioning):**
- `v1.0.0` - Major release (breaking changes)
- `v1.1.0` - Minor release (new features, backward compatible)
- `v1.1.1` - Patch release (bug fixes)
- `v1.0.0-alpha.1` - Pre-release (unstable)
- `v1.0.0-beta.1` - Beta release (feature complete, testing)

## CI/CD Pipeline

### Unified Pipeline (`.github/workflows/ci-pipeline.yml`)

**Triggers:**
- Push to any branch
- Pull request to `main`
- Manual trigger via workflow_dispatch

**Target duration:** <25 minutes for full pipeline

**Jobs:**

#### 1. C++ Build & Test (Matrix)

**Platforms:**
- `ubuntu-latest` (GCC 11)
- `windows-latest` (MSVC 2022)
- `macos-latest` (Clang 15)

**Build types:**
- `Debug` (with sanitizers)
- `Release` (optimizations)

**Total combinations:** 3 OS × 2 types = 6 parallel jobs

**Steps:**
1. Check out code
2. Install dependencies (libsndfile, etc.)
3. Configure CMake with sanitizers (Debug only)
4. Build SDK and tests
5. Run GoogleTest suite
6. Upload test results as artifacts

**Sanitizers (Debug builds only):**
- AddressSanitizer - Memory errors, leaks
- UBSanitizer - Undefined behavior

**Success criteria:**
- All tests pass (0 failures)
- No sanitizer violations
- Build completes in <10 minutes

#### 2. Lint Job

**Tools:**
- `clang-format` - C++ code style (enforced)
- `clang-tidy` - C++ static analysis (warnings only)
- `eslint` - TypeScript linting
- `prettier` - JSON/YAML formatting

**Steps:**
1. Check out code
2. Run clang-format with `--dry-run --Werror` (fails on violations)
3. Run eslint on TypeScript packages
4. Run prettier on JSON/YAML configs

**Success criteria:**
- All files pass formatting checks
- No ESLint errors (warnings OK)

**Fix violations locally:**
```bash
# C++ formatting
clang-format -i src/**/*.cpp include/**/*.h

# TypeScript linting/formatting
pnpm run format
pnpm run lint --fix
```

#### 3. Native Driver Job

**Purpose:** Build and test N-API bindings

**Platform:** `ubuntu-latest` (Linux)

**Steps:**
1. Build C++ SDK (Release mode)
2. Build `@orpheus/engine-native` addon
3. Run Node.js tests (Jest)
4. Verify bindings export correct symbols

**Success criteria:**
- Native addon builds without errors
- All Node.js tests pass
- No memory leaks detected

#### 4. TypeScript Job

**Purpose:** Test JavaScript packages

**Packages tested:**
- `@orpheus/client`
- `@orpheus/contract`
- `@orpheus/engine-service`
- `@orpheus/react`

**Steps:**
1. Install dependencies (`pnpm install`)
2. Build TypeScript packages (`pnpm run build`)
3. Run Jest tests
4. Check type definitions (`tsc --noEmit`)

**Success criteria:**
- All tests pass
- No TypeScript compilation errors
- Coverage >80% (goal)

#### 5. Integration Job

**Purpose:** End-to-end testing across components

**Scenarios:**
1. **Service Driver Handshake**
   - Start service driver
   - Connect client
   - Verify version negotiation
   - Execute sample commands
   - Verify events received

2. **Native Driver Commands**
   - Load session via native driver
   - Execute all contract commands
   - Verify responses match schema

3. **Event Frequency Validation**
   - Start transport with TransportTick events
   - Verify frequency ≤30 Hz
   - Check RenderProgress ≤10 Hz

4. **Multi-Clip Stress Test**
   - Load session with 16 clips
   - Start all simultaneously
   - Verify CPU <30%
   - Check for audio glitches (buffer underruns)

**Success criteria:**
- All scenarios pass
- No errors or warnings
- Performance within budgets

#### 6. Dependencies Job

**Purpose:** Audit dependencies for vulnerabilities

**Checks:**
1. **npm audit** - JavaScript package vulnerabilities
2. **Outdated packages** - Check for updates
3. **License compliance** - Verify compatible licenses
4. **CMake dependency graph** - Check for circular dependencies

**Success criteria:**
- No critical vulnerabilities
- All licenses compatible with MIT
- Warnings reported but don't fail build

**Fix vulnerabilities:**
```bash
# Update vulnerable packages
pnpm audit fix

# Or manually update package.json and re-run
pnpm install
```

#### 7. Performance Job

**Purpose:** Enforce performance budgets

**Metrics:**
- **CPU usage** - <20% with 8 simultaneous clips
- **Memory usage** - <100MB baseline, <500MB with 48 clips loaded
- **Latency** - <10ms round-trip (256 sample buffer @ 48kHz)
- **Build time** - <10 minutes (Release build)

**Steps:**
1. Build SDK in Release mode
2. Run performance benchmarks (`tools/perf/`)
3. Compare against baseline (stored in `budgets.json`)
4. Report regressions (warn, don't fail)

**Budgets configuration (`budgets.json`):**
```json
{
  "cpu": {
    "baseline": 5,
    "max_8_clips": 20,
    "max_16_clips": 30
  },
  "memory": {
    "baseline_mb": 100,
    "per_clip_mb": 8
  },
  "latency": {
    "max_ms": 10
  }
}
```

**Success criteria:**
- All metrics within budgets
- Regressions logged (CI artifacts)
- Warnings displayed in PR comments

### Specialized Workflows

#### Chaos Tests (`.github/workflows/chaos-tests.yml`)

**Trigger:** Nightly (3:00 AM UTC)

**Duration:** 2 hours

**Purpose:** Test failure recovery and long-term stability

**Tests:**
1. **Rapid Start/Stop**
   - Start/stop clips 100 times per second
   - Verify no memory leaks
   - Check for race conditions

2. **24-Hour Stability**
   - Run transport for 24 hours (simulated)
   - Check for memory growth
   - Verify timing accuracy (no drift)

3. **Resource Exhaustion**
   - Load 1000 clips (stress test)
   - Exhaust memory (check graceful degradation)
   - Recover and continue

4. **Recovery from Failures**
   - Simulate file read errors
   - Kill audio driver mid-stream
   - Verify clean recovery

**Success criteria:**
- No crashes
- Memory usage stable (no leaks)
- Graceful error handling

**Failure handling:**
- Notify team on Discord/Slack
- Create GitHub issue with logs
- Mark as blocking for next release

#### Security Audit (`.github/workflows/security-audit.yml`)

**Trigger:** Weekly (Monday 9:00 AM UTC)

**Duration:** ~15 minutes

**Tools:**
1. **CodeQL** - Static analysis for security issues
2. **npm audit** - JavaScript vulnerability scanning
3. **Dependency scanning** - Check for known CVEs
4. **Secret detection** - Scan for accidentally committed secrets

**Checks:**
- SQL injection (N/A - no database)
- Command injection (check CLI argument parsing)
- Path traversal (check file loading)
- Buffer overflows (C++ code analysis)
- Use-after-free (C++ code analysis)

**Success criteria:**
- No high/critical vulnerabilities
- All secrets removed (if detected)

**Failure handling:**
- Create security advisory (if public vulnerability)
- Patch immediately (emergency release)
- Notify users if already released

#### Documentation Publish (`.github/workflows/docs-publish.yml`)

**Trigger:** Release tag (`v*.*.*`)

**Duration:** ~5 minutes

**Steps:**
1. **Generate Doxygen documentation**
   - C++ API reference
   - Internal architecture docs
   - Call graphs

2. **Build user guides**
   - Convert markdown to HTML
   - Generate PDF versions
   - Create searchable index

3. **Publish to GitHub Pages**
   - Deploy to `gh-pages` branch
   - Update versioned docs (v1.0, v1.1, etc.)
   - Set up redirects (latest → current version)

4. **Update README badges**
   - Documentation link
   - Version badge
   - Build status

**Output:**
- Documentation live at: `https://yourusername.github.io/orpheus-sdk/`
- Versioned docs: `/v1.0/`, `/v1.1/`, `/latest/`

## Build Artifacts

### C++ Libraries

**Static libraries:**
- `liborpheus_core.a` - Session graph, JSON serialization
- `liborpheus_transport.a` - Transport controller, clip playback
- `liborpheus_audio_io.a` - File reader, audio drivers
- `liborpheus_routing.a` - Routing matrix, gain smoothing (future)

**Shared libraries:**
- `liborpheus.dylib` (macOS)
- `liborpheus.dll` (Windows)
- `liborpheus.so` (Linux)

**Usage:**
```cmake
# Option 1: Link individual libraries
target_link_libraries(your_app
    orpheus_core
    orpheus_transport
    orpheus_audio_io
)

# Option 2: Link combined library
target_link_libraries(your_app orpheus)
```

### Executables

**CLI tools:**
- `orpheus_minhost` - Command-line interface
- `inspect_session` - Session validator
- `json_roundtrip` - Conformance tool

**Applications:**
- `OrpheusClipComposer.app` (macOS)
- `OrpheusClipComposer.exe` (Windows)
- `OrpheusClipComposer` (Linux AppImage)

**Test binaries:**
- `abi_smoke_test`
- `session_roundtrip_test`
- `transport_tests`
- `routing_tests`
- `audio_io_tests`

### npm Packages

**Published packages:**
- `@orpheus/client` - Unified client API
- `@orpheus/contract` - JSON schemas and validation
- `@orpheus/engine-native` - N-API bindings
- `@orpheus/engine-service` - HTTP + WebSocket server
- `@orpheus/engine-wasm` - WebAssembly module
- `@orpheus/react` - React components (UI toolkit)

**Publishing:**
```bash
# Automated (CI on release tag)
# Manual:
pnpm publish --access public
```

**Installation:**
```bash
npm install @orpheus/client
# or
pnpm add @orpheus/client
```

## Distribution Channels

### GitHub Releases

**URL:** `https://github.com/yourusername/orpheus-sdk/releases`

**Assets per release:**
- Source code (zip/tar.gz) - Automatic
- Compiled binaries:
  - `orpheus-sdk-v1.0.0-macos.zip`
  - `orpheus-sdk-v1.0.0-windows.zip`
  - `orpheus-sdk-v1.0.0-linux.zip`
- Applications:
  - `OrpheusClipComposer-v1.0.0.dmg` (macOS)
  - `OrpheusClipComposer-v1.0.0-setup.exe` (Windows)
  - `OrpheusClipComposer-v1.0.0.AppImage` (Linux)
- Documentation:
  - `OrpheusSDK-Docs-v1.0.0.pdf`
- CHANGELOG.md

**Release notes format:**
```markdown
## v1.0.0 (2025-11-08)

### 🎉 New Features
- Real-time clip playback with fade IN/OUT
- Loop mode with sample-accurate restart
- Audio device configuration dialog

### 🐛 Bug Fixes
- Fixed memory leak in transport controller
- Corrected fade curve calculation for exponential type

### 📚 Documentation
- Added integration guide for new developers
- Updated API reference with examples

### ⚠️ Breaking Changes
- Renamed `startPlayback()` to `startClip()`
- Changed JSON schema version to 1.0

### 🔧 Internal Changes
- Refactored session loading for faster startup
- Optimized audio callback (20% CPU reduction)
```

### npm Registry

**Organization:** `@orpheus`

**Public packages:** All packages are public (open source)

**Installation:**
```bash
npm install @orpheus/client
```

**Version management:**
- Semantic versioning (semver)
- Pre-release tags: `-alpha`, `-beta`, `-rc`
- Dist-tags: `latest`, `next`, `beta`

**Deprecation policy:**
- Mark old versions as deprecated
- Provide migration guide
- Support for 6 months after deprecation

### Application Stores (Future)

**macOS:**
- **Mac App Store** - Sandboxed, notarized
- **Direct download** - Notarized DMG from GitHub Releases

**Windows:**
- **Microsoft Store** - UWP app
- **Direct download** - Signed EXE installer

**Linux:**
- **Snap Store** - Universal package
- **AppImage** - Portable executable
- **Flatpak** - Sandboxed app

**Requirements:**
- Code signing certificates
- App store developer accounts
- Review process (1-2 weeks)

### GitHub Pages

**URL:** `https://docs.orpheus-sdk.dev` (custom domain, future)

**Content:**
- API documentation (Doxygen-generated)
- User guides (Markdown-based)
- Tutorial videos (embedded YouTube)
- Release notes (all versions)

**Structure:**
```
/
├── index.html          (landing page)
├── api/
│   ├── v1.0/          (versioned API docs)
│   ├── v1.1/
│   └── latest/        (symlink to newest)
├── guides/
│   ├── getting-started.html
│   ├── integration.html
│   └── tutorials.html
└── releases/
    ├── v1.0.0.html
    └── v1.1.0.html
```

**Update process:**
1. Documentation built by CI on release
2. Pushed to `gh-pages` branch
3. GitHub automatically publishes to pages URL
4. Custom domain (if configured) updates within minutes

## Deployment Targets

### Desktop Applications

**Platforms:**
- macOS 10.15 (Catalina) or newer
- Windows 10 or newer
- Linux (Ubuntu 20.04 LTS or newer)

**Distribution formats:**
- **macOS:** DMG (disk image), notarized
- **Windows:** EXE installer (Inno Setup or NSIS)
- **Linux:** AppImage (portable), Snap, Flatpak

**Installation:**
1. Download from GitHub Releases
2. Open installer (DMG/EXE/AppImage)
3. Follow prompts (macOS: drag to Applications, Windows: run installer, Linux: make executable and run)
4. Launch application

**Auto-update (future):**
- Check for updates on launch
- Download and install in background
- Prompt user to restart

### Web Applications

**Deployment options:**

**Option 1: Static hosting (for WASM driver)**
- Platforms: Vercel, Netlify, GitHub Pages
- Cost: Free for low traffic
- Limitations: No server-side processing

```bash
# Deploy to Vercel
vercel deploy

# Deploy to Netlify
netlify deploy --prod
```

**Option 2: Node.js server (for Service driver)**
- Platforms: Heroku, Fly.io, Railway, AWS
- Cost: $5-50/month (depending on usage)
- Features: Full SDK access, WebSocket support

```bash
# Deploy to Fly.io
fly launch
fly deploy
```

**Configuration requirements:**
- HTTPS enabled (for WebSocket security)
- CORS configured (if client on different domain)
- Environment variables:
  - `ORPHEUS_SERVICE_PORT` (default: 8080)
  - `ORPHEUS_LOG_LEVEL` (default: INFO)
  - `NODE_ENV` (production/development)

**Example nginx config:**
```nginx
server {
    listen 443 ssl;
    server_name api.orpheus-app.com;

    location / {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

### Embedded Systems (Future)

**Target platforms:**
- Raspberry Pi 4/5 (ARM64)
- BeagleBone Black (ARM)
- Dedicated audio hardware (custom)

**Requirements:**
- Cross-compilation toolchain
- Minimal dependencies (no GUI frameworks)
- Low-latency audio drivers (ALSA, Jack)

**Build process:**
```bash
# Cross-compile for ARM64
cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=cmake/arm64-toolchain.cmake \
    -DORPHEUS_ENABLE_REALTIME=ON \
    -DORPHEUS_ENABLE_GUI=OFF

cmake --build build
```

**Deployment:**
- Copy binaries to embedded device
- Configure systemd service (auto-start)
- Set up remote control (WebSocket)

### Cloud Services (Future)

**Use cases:**
- Server-side rendering (batch processing)
- Batch processing (convert multiple sessions)
- API services (REST API for remote control)

**Platforms:**
- **AWS Lambda** - Serverless functions (offline rendering)
- **Google Cloud Run** - Container-based (service driver)
- **Azure Functions** - Serverless (batch processing)

**Example Lambda deployment:**
```typescript
// lambda/handler.ts
import { NativeEngine } from '@orpheus/engine-native';

export const handler = async (event: any) => {
    const engine = new NativeEngine();
    await engine.loadSession(event.sessionPath);
    await engine.renderClick({
        outputPath: '/tmp/click.wav',
        bars: event.bars,
        bpm: event.bpm
    });
    // Upload to S3
    return { statusCode: 200, body: 'Rendered' };
};
```

**Considerations:**
- Cold start latency (5-10 seconds for Lambda)
- Timeout limits (15 minutes for Lambda)
- Memory limits (up to 10GB for Lambda)
- No persistent storage (use S3/GCS for output)

## Monitoring & Observability

### CI/CD Monitoring

**GitHub Actions dashboard:**
- View workflow runs
- Check job status
- Download artifacts
- Re-run failed jobs

**Notifications:**
- Email (on failure)
- Slack/Discord webhooks (optional)
- GitHub mobile app (push notifications)

### Application Monitoring (Future)

**Metrics:**
- Crash reports (Sentry)
- Usage analytics (Amplitude)
- Performance monitoring (New Relic)

**Privacy:**
- All metrics opt-in
- No personal data collected
- Anonymized session data only

## Troubleshooting CI Issues

### Common CI Failures

**1. Sanitizer violations**

**Symptom:**
```
AddressSanitizer: heap-use-after-free
```

**Fix:**
- Run locally with sanitizers: `cmake -B build -DCMAKE_BUILD_TYPE=Debug`
- Use `rt.safety.auditor` skill for static analysis
- Fix memory management issues

**2. Formatting violations**

**Symptom:**
```
clang-format: files not formatted correctly
```

**Fix:**
```bash
clang-format -i src/**/*.cpp include/**/*.h
git commit -am "style: format code"
git push
```

**3. Test failures on specific platform**

**Symptom:**
```
Test failed on windows-latest but passed on ubuntu-latest
```

**Fix:**
- Check for platform-specific code paths
- Run tests locally on that platform (if available)
- Add platform guards: `#ifdef _WIN32`

**4. Build timeout (>10 minutes)**

**Symptom:**
```
Job cancelled due to timeout
```

**Fix:**
- Optimize build (reduce template instantiations)
- Use ccache (compiler cache)
- Split into smaller targets

### Re-running Failed Jobs

**GitHub UI:**
1. Go to Actions tab
2. Click on failed workflow run
3. Click "Re-run failed jobs"

**GitHub CLI:**
```bash
gh run rerun <run-id> --failed
```

## Related Documentation

**Build system:**
- `CMakeLists.txt` - Main CMake configuration
- `cmake/` - CMake modules and toolchain files

**CI/CD:**
- `.github/workflows/` - GitHub Actions workflows
- `scripts/` - Helper scripts for CI

**Developer guides:**
- `CLAUDE.md` - Development conventions
- `docs/CONTRIBUTING.md` - Contribution guidelines
- `ARCHITECTURE.md` - System architecture

## Related Diagrams

- See `architecture-overview.mermaid.md` for system layers
- See `component-map.mermaid.md` for component relationships
- See `entry-points.mermaid.md` for SDK access methods
