# ORP116: Bloat Remediation Sprint

**Status:** Draft
**Created:** 2025-11-18
**Author:** Claude Code Audit
**Priority:** High

## Executive Summary

**Total Potential Size Reduction:** ~62+ MB
**Risk Level:** Most removals are **Safe** with low impact
**Quick Wins:** 7 high-impact, low-risk items identified

---

## 1. Binary Files in Git (CRITICAL)

### Issue #1: Release Artifacts Tracked in Git

- **Location:** `docs/releases/OrpheusClipComposer-v0.1.0-arm64.dmg`
- **Type:** Binary bloat
- **Size Impact:** 37 MB
- **Risk Level:** Safe to remove
- **Action:**
  ```bash
  git rm docs/releases/OrpheusClipComposer-v0.1.0-arm64.dmg
  echo "*.dmg" >> .gitignore
  ```
- **Rationale:** Release binaries should be hosted on GitHub Releases, not in the repository. This bloats git history permanently.

### Issue #2: Competitor Research PDFs

- **Location:** `apps/clip-composer/docs/occ/peers/SpotOn Manual*.pdf` (14 files)
- **Type:** Binary bloat
- **Size Impact:** ~21 MB
- **Risk Level:** Safe (move to external storage)
- **Action:**

  ```bash
  # Remove from git
  git rm apps/clip-composer/docs/occ/peers/*.pdf

  # Move to external reference if needed
  # Add note in peers/README.md with link to cloud storage
  ```

- **Rationale:** Competitor research materials don't need version control. Store externally (Dropbox, Google Drive) and link in documentation.

---

## 2. Archived TypeScript Packages

### Issue #3: Obsolete TypeScript Packages

- **Location:** `archive/packages/` (react, engine-wasm, engine-native, client, contract, engine-service)
- **Type:** Dead code
- **Size Impact:** Several MB (includes node_modules, build artifacts, dist directories)
- **Risk Level:** Needs Review (verify truly obsolete)
- **Action:**

  ```bash
  # If confirmed obsolete:
  git rm -r archive/packages

  # If needed for reference, move to separate git branch
  git checkout -b archive/typescript-packages
  git mv archive/packages .
  git commit -m "chore: preserve TypeScript packages in archive branch"
  git checkout main
  git rm -r archive/
  ```

- **Rationale:** Per commit `acd00b3` ("refactor: archive TypeScript packages, focus on C++ SDK"), these were intentionally archived when pivoting to C++ focus. Safe to remove if truly obsolete.

---

## 3. Dependency Bloat

### Issue #4: Unused TypeScript/ESLint Dependencies

- **Location:** `package.json` devDependencies
- **Type:** Dependency bloat
- **Size Impact:** ~100 MB (node_modules when installed)
- **Risk Level:** Safe (TypeScript ecosystem no longer needed)
- **Unused Dependencies:**
  - `@typescript-eslint/eslint-plugin` (7.15.0)
  - `@typescript-eslint/parser` (7.15.0)
  - `eslint` (8.57.0)
  - `eslint-config-prettier` (9.1.0)
  - `eslint-import-resolver-typescript` (3.6.1)
  - `eslint-plugin-import` (2.29.1)
  - `madge` (8.0.0) - never used in scripts/CI

- **Action:**

  ```bash
  pnpm remove @typescript-eslint/eslint-plugin @typescript-eslint/parser \
    eslint eslint-config-prettier eslint-import-resolver-typescript \
    eslint-plugin-import madge

  # Update .lintstagedrc.json - remove TS/JS linting rules
  # Keep only: "*.{cpp,h,hpp}": ["clang-format -i"]

  # Remove .eslintrc.cjs (no longer needed)
  rm .eslintrc.cjs
  ```

### Issue #5: Unnecessary pnpm Overrides

- **Location:** `package.json` pnpm.overrides section
- **Type:** Dependency bloat
- **Size Impact:** Minor (but adds complexity)
- **Risk Level:** Safe
- **Obsolete Overrides:**
  - `wait-on`, `camera-controls`, `@vercel/oidc`, `@orama/orama` - related to archived TS packages
  - Most others are security patches for transitive dependencies that no longer exist

- **Action:**
  ```json
  // Keep only if still needed for Husky/commitlint:
  "pnpm": {
    "overrides": {
      "cross-spawn": "7.0.6"  // Common security fix
    }
  }
  ```

---

## 4. Asset Bloat (Font Files)

### Issue #6: Excessive Font Weights

- **Location:** `apps/clip-composer/Resources/HKGrotesk_3003/`
- **Type:** Asset bloat
- **Size Impact:** 4.3 MB → reduce to ~400 KB (save ~3.9 MB)
- **Risk Level:** Safe
- **Current State:** 72 font files (9 weights × 2 styles × 4 formats)
- **Actual Usage:** Only "plain" and "bold" styles used in code (HKGroteskLookAndFeel.h:28-44)
- **Action:**

  ```bash
  # Keep only Regular and Bold in TTF format (desktop app doesn't need web fonts)
  cd apps/clip-composer/Resources/HKGrotesk_3003/

  # Remove web fonts (WOFF/WOFF2) - 1.5 MB saved
  rm -rf WEB/

  # Remove OTF duplicates - 1.3 MB saved
  rm -rf OTF/

  # Keep only Regular and Bold TTF
  cd TTF/
  mkdir ../keep
  mv HKGrotesk-Regular.ttf HKGrotesk-Bold.ttf ../keep/
  cd ..
  rm -rf TTF/
  mv keep TTF/

  # Total saved: ~3.9 MB
  ```

### Issue #7: Duplicate Icon Sets

- **Location:** `apps/clip-composer/Resources/icons/transport/`
- **Type:** Asset duplication (confirmed unused)
- **Size Impact:** ~5 KB (minimal, but indicates confusion)
- **Risk Level:** Safe
- **Current State:** 3 icon sets (lucide, phosphor, tabler) with same 5 icons each
- **Actual Usage:** **CONFIRMED UNUSED** - No SVG references found in any C++ source files
- **Action:**

  ```bash
  # Determine which set is actually used (check at runtime or ask team)
  # Then remove the other two:

  # Assuming tabler is used (newest commit mentioned tabler):
  cd apps/clip-composer/Resources/icons/transport/
  rm -rf lucide/ phosphor/

  # If none are used, remove all:
  # rm -rf apps/clip-composer/Resources/icons/transport/
  ```

---

## 5. Dead Code

### Issue #8: Backup Files Tracked in Git

- **Location:**
  - `CLAUDE.bak`
  - `docs/archive/CLAUDE.bak`
  - `docs/repo-commands.html.bak`
- **Type:** Dead code
- **Size Impact:** <10 KB (but violates best practices)
- **Risk Level:** Safe
- **Action:**
  ```bash
  git rm CLAUDE.bak docs/archive/CLAUDE.bak docs/repo-commands.html.bak
  echo "*.bak" >> .gitignore
  ```

### Issue #9: .eslintrc.cjs for Archived Packages

- **Location:** `.eslintrc.cjs`
- **Type:** Dead code (config for non-existent packages)
- **Size Impact:** <1 KB
- **Risk Level:** Safe (if Issue #4 resolved)
- **Action:**
  ```bash
  # After removing TypeScript dependencies
  git rm .eslintrc.cjs
  ```

---

## 6. Build & Infrastructure

### Issue #10: pnpm Lock File Size

- **Location:** `pnpm-lock.yaml`
- **Type:** Dependency overhead
- **Size Impact:** 225 KB, 6768 lines
- **Risk Level:** Will be resolved by Issue #4
- **Action:** After removing TS dependencies, regenerate:
  ```bash
  rm pnpm-lock.yaml
  pnpm install
  # New lockfile should be <50 KB
  ```

---

## Summary Metrics

| Category      | Issues | Size Impact         | Risk Level      |
| ------------- | ------ | ------------------- | --------------- |
| Binary files  | 2      | 58 MB               | Safe            |
| Archived code | 1      | Several MB          | Needs Review    |
| Dependencies  | 2      | ~100 MB (installed) | Safe            |
| Assets        | 2      | 3.9 MB              | Safe            |
| Dead code     | 2      | <10 KB              | Safe            |
| **TOTAL**     | **9**  | **~62+ MB**         | **Mostly Safe** |

---

## Quick Wins (Immediate Action)

These 7 items can be removed immediately with high confidence:

1. **Remove DMG file** (37 MB) - Issue #1
2. **Remove SpotOn PDFs** (21 MB) - Issue #2
3. **Remove backup files** (<10 KB) - Issue #8
4. **Remove unused font weights** (3.9 MB) - Issue #6
5. **Remove unused TypeScript tooling** (~100 MB node_modules) - Issue #4
6. **Remove duplicate icon sets** (~3 KB) - Issue #7
7. **Simplify pnpm overrides** - Issue #5

**Total immediate savings:** ~62 MB in repo + ~100 MB in installed dependencies

---

## Areas Requiring Deeper Review

### 1. Archived TypeScript Packages (Issue #3)

- **Question:** Are these needed for historical reference or future revival?
- **Recommendation:** If truly obsolete, remove completely. If might be revived, move to separate git branch.

### 2. Icon Sets (Issue #7) - RESOLVED

- **Finding:** All three icon sets (lucide, phosphor, tabler) are **confirmed unused** - no SVG references in source code
- **Recommendation:** Remove entire `apps/clip-composer/Resources/icons/transport/` directory

---

## Additional Findings (Low Priority)

1. **Comment Density:** 2,241 commented lines in C++ code - likely includes Doxygen documentation (acceptable)
2. **TODO Markers:** 18 TODO/FIXME markers (reasonable for active development)
3. **No Build Artifacts:** Build directories properly excluded (good hygiene)
4. **No Empty Files:** No zero-byte source files (clean codebase)

---

## Recommended Action Plan

### Phase 1: Immediate (This Week)

```bash
# 1. Remove binary artifacts
git rm docs/releases/OrpheusClipComposer-v0.1.0-arm64.dmg
git rm apps/clip-composer/docs/occ/peers/*.pdf

# 2. Remove backup files
git rm CLAUDE.bak docs/archive/CLAUDE.bak docs/repo-commands.html.bak

# 3. Update .gitignore
echo "*.dmg" >> .gitignore
echo "*.bak" >> .gitignore

# 4. Commit
git commit -m "chore: remove binary artifacts and backup files (58 MB)"
```

### Phase 2: Dependency Cleanup (Next Week)

```bash
# 1. Remove TypeScript tooling
pnpm remove @typescript-eslint/eslint-plugin @typescript-eslint/parser \
  eslint eslint-config-prettier eslint-import-resolver-typescript \
  eslint-plugin-import madge

# 2. Update configs
rm .eslintrc.cjs
# Update .lintstagedrc.json (remove TS/JS rules)

# 3. Simplify pnpm overrides in package.json

# 4. Commit
git commit -m "chore: remove obsolete TypeScript tooling"
```

### Phase 3: Asset Optimization (Following Week)

```bash
# 1. Trim font files
# (See Issue #6 detailed instructions)

# 2. Remove duplicate icon sets
# (After confirming which set is used)

# 3. Commit
git commit -m "chore: optimize font and icon assets (3.9 MB)"
```

### Phase 4: Review Archived Packages (Future)

- Team discussion: Remove archive/packages entirely or preserve in branch?

---

## Conclusion

The Orpheus SDK codebase is generally well-maintained with good separation of concerns. The primary bloat comes from:

1. **Binary artifacts in git** (58 MB) - CRITICAL to remove
2. **Unnecessary TypeScript tooling** (~100 MB installed) - Safe to remove after TypeScript package archival
3. **Over-provisioned font assets** (3.9 MB) - Safe to trim to 2 weights

**Total actionable savings:** ~62 MB in repository + ~100 MB in installed dependencies

The C++ core (456 KB src/ + 126 KB include/) is lean and appropriate for a professional audio SDK. Focus cleanup efforts on the items above for maximum impact.

---

## Appendix: Audit Methodology

This audit was conducted by analyzing:

- Git-tracked files and their sizes
- package.json dependencies and usage
- CMakeLists.txt build configuration
- Source code references to assets and dependencies
- .gitignore patterns vs actual tracked files

Tools used: git, find, du, grep, wc
