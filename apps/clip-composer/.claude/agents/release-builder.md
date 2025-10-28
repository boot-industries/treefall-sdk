# Agent: Release Builder

## Purpose

Build, package, and prepare Clip Composer releases for distribution. Creates DMG packages (macOS), validates binaries, generates release notes, and publishes to GitHub.

## Triggers

- Ready to create a new release (alpha, beta, stable)
- Need to test release builds
- Creating distribution packages for beta testers
- User explicitly requests: "Create release build" or "Package for distribution"

## Process

### 1. Pre-Release Validation

Before building release:

#### Version Consistency Check

- CMakeLists.txt project version
- Release tag (e.g., v0.1.0-alpha)
- Documentation references (README.md, implementation_progress.md)

#### Documentation Check

- README.md updated with current release status
- implementation_progress.md reflects completed work
- CLAUDE.md updated with latest conventions
- Release notes drafted (what's new, known issues, roadmap)

#### Code Quality Check

- No uncommitted changes (`git status` clean or expected)
- All critical bugs resolved
- Known limitations documented

### 2. Build Configuration

#### macOS Release Build

```bash
cd /Users/chrislyons/dev/orpheus-sdk
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
```

**Note:** As of v0.1.0-alpha, Release builds have linker issues. Use Debug build:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES="arm64" \
  -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
```

#### Universal Binary (Future)

```bash
cmake -S . -B build-universal \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
```

**Blocker:** libsndfile only available for arm64 on Apple Silicon Macs.

### 3. Build Application

```bash
cmake --build build --target orpheus_clip_composer_app
```

**Verify:**

- Build completes without errors
- No critical warnings
- Binary size reasonable (~90MB Debug, ~30MB Release expected)

### 4. Verify Build Artifacts

```bash
APP_PATH="build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app"
ls -lh "$APP_PATH"
```

**Checks:**

- Application bundle exists
- Binary is executable: `file "$APP_PATH/Contents/MacOS/OrpheusClipComposer"`
- Architecture correct: `lipo -info "$APP_PATH/Contents/MacOS/OrpheusClipComposer"`
- Resources present: Fonts, icons, etc.

### 5. Launch and Smoke Test

```bash
open "$APP_PATH"
```

**Manual validation:**

- Application launches without crashes
- UI renders correctly
- Can load a session
- Can play a clip
- Audio outputs correctly
- Edit dialog opens and functions

**Wait for user confirmation:** "Does the application work correctly? (yes/no)"

### 6. Create DMG Package

```bash
VERSION="v0.1.0-alpha"
ARCH="arm64"  # or "universal" for multi-arch

# Stage application
mkdir -p /tmp/occ-staging
cp -R "$APP_PATH" /tmp/occ-staging/

# Create DMG
hdiutil create \
  -volname "Orpheus Clip Composer" \
  -srcfolder /tmp/occ-staging \
  -ov \
  -format UDZO \
  "OrpheusClipComposer-${VERSION}-${ARCH}.dmg"

# Verify DMG
hdiutil imageinfo "OrpheusClipComposer-${VERSION}-${ARCH}.dmg"

# Clean up staging
rm -rf /tmp/occ-staging
```

**Verify:**

- DMG created successfully
- File size reasonable (~30-40MB)
- DMG format is UDZO (compressed)

### 7. Generate Release Notes

**Template:**

```markdown
# Orpheus Clip Composer [VERSION]

**Release Date:** [DATE]
**Platform:** macOS 12+ ([ARCH] only)
**Build Type:** [Debug/Release]
**Download:** OrpheusClipComposer-[VERSION]-[ARCH].dmg ([SIZE])

---

## What's New

### Features

- [Feature 1]
- [Feature 2]

### Improvements

- [Improvement 1]
- [Improvement 2]

### Bug Fixes

- [Fix 1]
- [Fix 2]

---

## Known Limitations

- [Limitation 1]
- [Limitation 2]

---

## Installation

1. Download the DMG file
2. Open the DMG
3. Drag OrpheusClipComposer.app to Applications folder
4. Launch from Applications
5. If macOS blocks the app: System Preferences → Security & Privacy → "Open Anyway"

---

## Getting Started

[Brief usage instructions or link to documentation]

---

## Roadmap

**Next Release (v0.2.0):**

- [Feature 1]
- [Feature 2]

**Future (v1.0):**

- [Long-term feature 1]
- [Long-term feature 2]

---

## Feedback

- GitHub Issues: https://github.com/chrislyons/orpheus-sdk/issues
- Tag: `clip-composer`
```

### 8. Tag and Publish Release

#### Create Git Tag

```bash
VERSION="v0.1.0-alpha"
git tag -a "$VERSION" -m "Clip Composer $VERSION release"
git push origin "$VERSION"
```

#### Publish to GitHub

```bash
gh release create "$VERSION" \
  --title "Orpheus Clip Composer $VERSION" \
  --notes-file release-notes.md \
  --prerelease \
  "OrpheusClipComposer-${VERSION}-arm64.dmg"
```

**Note:** Use `--prerelease` for alpha/beta, omit for stable releases.

### 9. Post-Release Tasks

#### Update Documentation

- Mark release as complete in implementation_progress.md
- Update README.md with download link
- Update CLAUDE.md with current status
- Tag session report if applicable

#### Announce Release

- Notify beta testers (if applicable)
- Post to relevant channels (Slack, Discord, etc.)
- Update project website/homepage (if applicable)

#### Archive Build Artifacts

- Keep DMG for historical reference
- Archive build logs
- Document any issues encountered during release

## Success Criteria

- [x] Pre-release validation complete
- [x] Application builds successfully
- [x] Binary verified (architecture, executable, resources)
- [x] Smoke test passed (user confirmed)
- [x] DMG created and verified
- [x] Release notes generated
- [x] Git tag created and pushed
- [x] GitHub release published
- [x] Documentation updated
- [x] No regressions from previous release

## Tools Required

- Bash (cmake, git, hdiutil, gh commands)
- Read (for reading release notes template)
- Write (for generating release notes)
- Edit (for updating documentation)

## Error Handling

### Build Fails

**Symptom:** Compilation or linker errors
**Action:**

1. Do NOT proceed with release
2. Document error: File, line, error message
3. Suggest: "Fix build errors before creating release. See build-and-test agent."

### Smoke Test Fails

**Symptom:** Application crashes or doesn't function correctly
**Action:**

1. Do NOT proceed with release
2. Document issue: Steps to reproduce, expected vs actual
3. Suggest: "Fix critical bugs before releasing. Defer to next version if non-blocking."

### DMG Creation Fails

**Symptom:** `hdiutil create` fails
**Action:**

1. Check disk space: `df -h`
2. Verify staging directory: `ls -lh /tmp/occ-staging`
3. Retry with verbose output: `hdiutil create -verbose ...`

### GitHub Release Upload Fails

**Symptom:** `gh release upload` times out or fails
**Action:**

1. Check network connection
2. Verify DMG file exists and is readable
3. Retry: `gh release upload [VERSION] [DMG_FILE]`
4. If persistent: Upload manually via GitHub web interface

### Universal Binary Blocked

**Symptom:** Linker error for x86_64 architecture
**Action:**

1. Document dependency limitation (e.g., libsndfile arm64-only)
2. Build separate binaries for Intel and ARM
3. Update release notes: "Intel build coming soon" or "ARM only for now"

---

## Example Usage

**User:** "I'm ready to release v0.2.0-beta. Package it up and publish to GitHub."

**Agent Response:**

1. Validates: Documentation updated? ✅ Version consistent? ✅
2. Builds: Release mode, arm64, no errors
3. Verifies: Binary 35MB, launches correctly
4. Asks: "Smoke test passed?" → User: "Yes"
5. Creates DMG: 32MB compressed
6. Generates release notes from template
7. Tags: v0.2.0-beta
8. Publishes: GitHub release with DMG attached
9. Updates docs: README.md, implementation_progress.md
10. Reports: "Release v0.2.0-beta published successfully. Download: [URL]"

---

**Last Updated:** October 22, 2025
**Version:** 1.0
**Latest Release:** v0.1.0-alpha (October 22, 2025)
**Platform:** macOS 12+ (arm64)
