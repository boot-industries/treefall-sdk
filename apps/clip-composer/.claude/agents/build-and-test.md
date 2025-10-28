# Agent: Build and Test

## Purpose

Build Clip Composer application, verify compilation succeeds, and run any available tests. Provides clear feedback on build status and errors.

## Triggers

- After significant code changes (new features, refactoring)
- Before creating pull requests
- When testing new features or bug fixes
- After updating dependencies (SDK, JUCE)
- User explicitly requests: "Build and test" or "Verify the build"

## Process

### 1. Clean Build (Optional)

- If requested or previous build is corrupted, remove build directory
- `rm -rf build`

### 2. Configure CMake

```bash
cd /Users/chrislyons/dev/orpheus-sdk
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
```

### 3. Build Application

```bash
cmake --build build --target orpheus_clip_composer_app
```

**Monitor for:**

- Compilation warnings (report but don't fail)
- Compilation errors (report and fail)
- Linker errors (report and fail)
- Missing dependencies (report and suggest fixes)

### 4. Verify Build Artifacts

```bash
ls -lh build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app
```

**Check:**

- Application bundle exists
- Binary size is reasonable (~90-130MB for Debug)
- Resources are present (icons, fonts)

### 5. Run Tests (If Available)

```bash
ctest --test-dir build --output-on-failure -R clip_composer
```

**Note:** As of v0.1.0-alpha, no automated tests exist. This step is for future use.

### 6. Report Results

- **Success:** "Build completed successfully. Binary size: [SIZE]. No errors."
- **Warnings:** "Build succeeded with [N] warnings. Review recommended."
- **Failure:** "Build failed. Error: [ERROR MESSAGE]. Suggested fix: [FIX]"

## Success Criteria

- [x] CMake configuration completes without errors
- [x] Application builds successfully
- [x] Binary exists at expected path
- [x] No critical warnings or errors
- [x] Tests pass (when available)

## Tools Required

- Bash (cmake, ls commands)
- Read (for reading build logs if needed)
- Grep (for searching build output)

## Error Handling

### CMake Configuration Fails

**Symptom:** `CMake Error: Could not find a package configuration file`
**Action:**

1. Verify Orpheus SDK is built: `ls build/src/`
2. Check JUCE is available: `cmake -L build | grep JUCE`
3. Suggest: "Rebuild Orpheus SDK first: `cmake --build build`"

### Compilation Fails

**Symptom:** `error: no member named 'X' in 'Y'`
**Action:**

1. Read error message fully
2. Identify file and line number
3. Check for missing includes or SDK API changes
4. Suggest: "Review recent SDK changes or check file_path:line_number"

### Linker Fails

**Symptom:** `Undefined symbols for architecture arm64`
**Action:**

1. Identify missing symbols
2. Check if SDK libraries are linked
3. Suggest: "Verify SDK libraries in CMakeLists.txt or rebuild SDK"

### Missing Binary

**Symptom:** Binary doesn't exist after "successful" build
**Action:**

1. Check build logs for silent failures
2. Verify CMake target name is correct
3. Suggest: "Clean build and retry: `rm -rf build && [configure/build]`"

---

## Example Usage

**User:** "Build the application and let me know if there are any issues"

**Agent Response:**

1. Configures CMake with Debug + ORPHEUS_ENABLE_APP_CLIP_COMPOSER
2. Runs build command
3. Monitors output for errors/warnings
4. Reports: "Build completed successfully. Binary size: 127MB. 3 warnings (non-critical). Application ready at build/apps/clip-composer/.../OrpheusClipComposer.app"

---

**Last Updated:** October 22, 2025
**Version:** 1.0
**Compatible with:** v0.1.0-alpha and later
