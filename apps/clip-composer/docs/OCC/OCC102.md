# OCC102 - OCC Track: v0.2.0 Release & v0.2.1 Planning

**Created:** 2025-10-31
**Status:** Active
**Priority:** Critical
**Estimated Duration:** 3-5 days
**Owner:** OCC Development Agent

---

## Purpose

Complete final QA and release for Orpheus Clip Composer v0.2.0-alpha, then plan v0.2.1 enhancements based on beta tester feedback. This track focuses on user-facing application development while SDK improvements happen in parallel.

---

## Context

**Background:**

- v0.2.0 sprint complete (OCC093) - 6 critical UX fixes shipped
- PR #121 merged to main (Oct 29, 2025)
- Beta testers awaiting v0.2.0 release
- v0.1.0-alpha released Oct 22, 2025

**v0.2.0 Sprint Deliverables (OCC093):**

1. ✅ Stop Others fade distortion fix (gain smoothing)
2. ✅ 75fps button state tracking (visual sync)
3. ✅ Edit Dialog time counter spacing fix
4. ✅ `[` `]` keyboard shortcuts restart playback
5. ✅ Transport spam fix (single command per action)
6. ✅ Trim point edit laws (playhead constraint enforcement)

**Related Documents:**

- OCC093 - v0.2.0 Sprint Completion Report
- OCC026 - MVP Definition (6-month plan)
- OCC099 - Testing Strategy
- OCC100 - Performance Requirements
- OCC101 - Troubleshooting Guide

---

## Task List

### Priority 1: v0.2.0 Release (Critical Path) ⚡

#### Task 1.1: Final QA - Manual Testing Checklist

**Status:** 🔴 Not Started
**Estimated Time:** 3 hours

**Test Environment Setup:**

- [ ] Clean install from latest main branch
- [ ] Delete all preferences/caches (factory reset)
- [ ] Load reference session (960 clips, mixed metadata)
- [ ] Test on primary hardware (macOS, ASIO/CoreAudio)

**Core Functionality:**

- [ ] Load session with 960 clips (verify <2s load time)
- [ ] Trigger 16 simultaneous clips (verify no dropouts)
- [ ] Verify CPU usage <30% during 16-clip playback
- [ ] Verify memory stable (no leaks over 5 minutes)
- [ ] Verify latency <16ms round-trip (512 samples @ 48kHz)

**v0.2.0 Fix Verification (from OCC093):**

- [ ] **Fix #1:** Stop Others → smooth fade, no zigzag distortion
- [ ] **Fix #2:** Clip buttons update during playback (75fps visual sync)
- [ ] **Fix #3:** Edit Dialog time counter → no text collision with waveform
- [ ] **Fix #4:** `[` `]` keys restart playback from new IN point
- [ ] **Fix #5:** Click-to-jog → single command (no transport spam)
- [ ] **Fix #6:** Trim point editing → playhead respects boundaries (all methods)

**Edit Dialog Workflows:**

- [ ] Cmd+Click waveform (set IN) → playhead jumps to IN, restarts
- [ ] Cmd+Shift+Click waveform (set OUT) → if OUT <= playhead, jump to IN and restart
- [ ] Keyboard shortcuts `[` `]` `;` `'` → edit laws enforced
- [ ] Time editor inputs → edit laws enforced
- [ ] Shift+click nudge → 10x acceleration, no crashes
- [ ] Loop mode toggle → clip loops infinitely at trim OUT point

**Multi-Tab Isolation (Critical):**

- [ ] Load clip on Tab 1, button 1
- [ ] Load clip on Tab 2, button 1
- [ ] Trigger Tab 1 button 1 → only Tab 1 clip plays (no Tab 2)
- [ ] Verify transport isolation across all 8 tabs

**Regression Testing (v0.1.0 Features):**

- [ ] Session save/load → metadata preserved (trim, fade, gain, loop, color)
- [ ] Routing controls → 4 Clip Groups functional
- [ ] Keyboard shortcuts → all keys working (space, arrow keys, modifier combos)
- [ ] Waveform display → rendering correctly, no visual glitches

**Acceptance Criteria:**

- All test cases pass (0 critical bugs, <3 minor bugs)
- No regressions from v0.1.0
- No crashes during 30-minute test session
- QA checklist saved to `apps/clip-composer/docs/OCC/qa_v020_results.md`

**Reference:**

- OCC093 Section "Testing Results" for known working state
- OCC099 Section "Manual Testing Checklist" for full test matrix

---

#### Task 1.2: Fix Any Critical Bugs Discovered in QA

**Status:** 🟡 Pending Task 1.1
**Estimated Time:** Variable (0-4 hours)

**Requirements:**

- [ ] If QA finds critical bugs → fix immediately
- [ ] If QA finds minor bugs → defer to v0.2.1 backlog
- [ ] Re-run QA after fixes (regression check)
- [ ] Document all bugs in GitHub Issues

**Acceptance Criteria:**

- Zero critical bugs blocking release
- Minor bugs triaged to v0.2.1 milestone
- All fixes committed to `main` branch

---

#### Task 1.3: Update Release Notes

**Status:** 🔴 Not Started
**Files to Create:** `apps/clip-composer/CHANGELOG.md`
**Estimated Time:** 1 hour

**Requirements:**

- [ ] Create `CHANGELOG.md` following Keep a Changelog format
- [ ] List all 6 v0.2.0 fixes with user-facing descriptions
- [ ] Add "Breaking Changes" section (if any)
- [ ] Add "Known Issues" section (deferred bugs)
- [ ] Add "Upgrade Notes" section (how to migrate from v0.1.0)

**Content Structure:**

```markdown
# Changelog

## [0.2.0-alpha] - 2025-10-31

### Fixed

- Stop Others fade-out now smooth (no zigzag distortion)
- Clip buttons update in real-time during playback (75fps)
- Edit Dialog time counter spacing improved
- `[` `]` keyboard shortcuts now restart playback from new IN point
- Click-to-jog uses single command (gap-free seeking)
- Trim point editing enforces playhead boundaries (all input methods)

### Known Issues

- [Issue #XX] Audio device selection UI pending (manual config in preferences)
- [Issue #XX] Latch acceleration needs tuning (Shift+click sensitivity)

## [0.1.0-alpha] - 2025-10-22

### Added

- Initial 960-clip grid (10×12 buttons × 8 tabs)
- Edit Dialog with trim/fade/gain/loop controls
- Session save/load with JSON format
- 4 Clip Groups with routing controls
  ...
```

**Acceptance Criteria:**

- Changelog covers all user-facing changes
- Clear upgrade path from v0.1.0 → v0.2.0
- Known issues documented with GitHub issue links

---

#### Task 1.4: Tag and Publish v0.2.0-alpha Release

**Status:** 🟡 Pending Task 1.1-1.3
**Estimated Time:** 30 minutes

**Requirements:**

- [ ] Ensure all QA passes and bugs fixed
- [ ] Update version number in `apps/clip-composer/CMakeLists.txt`
- [ ] Update version in `apps/clip-composer/Source/Version.h` (if exists)
- [ ] Commit version bump to `main`
- [ ] Tag commit with `v0.2.0-alpha`
- [ ] Push tag to remote

**Commands:**

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer

# Update version in CMakeLists.txt (manual edit)
# JUCE_VERSION "0.2.0"

git add CMakeLists.txt CHANGELOG.md
git commit -m "chore(occ): bump version to v0.2.0-alpha"
git tag v0.2.0-alpha
git push origin main --tags
```

**Acceptance Criteria:**

- Version tag `v0.2.0-alpha` exists on `main` branch
- GitHub release created with CHANGELOG excerpt
- Tag matches final QA-approved commit (no further changes)

---

#### Task 1.5: Create GitHub Release

**Status:** 🟡 Pending Task 1.4
**Estimated Time:** 30 minutes

**Requirements:**

- [ ] Draft GitHub release for `v0.2.0-alpha`
- [ ] Attach CHANGELOG excerpt
- [ ] Attach build instructions (or link to README)
- [ ] Mark as "pre-release" (alpha software)
- [ ] Publish release

**Commands:**

```bash
gh release create v0.2.0-alpha \
  --title "Orpheus Clip Composer v0.2.0-alpha" \
  --notes-file apps/clip-composer/CHANGELOG.md \
  --prerelease
```

**Acceptance Criteria:**

- GitHub release visible at `https://github.com/<user>/orpheus-sdk/releases/tag/v0.2.0-alpha`
- Release notes match CHANGELOG
- Beta testers can download and install

---

#### Task 1.6: Deploy to Beta Testers

**Status:** 🟡 Pending Task 1.5
**Estimated Time:** 1 hour

**Requirements:**

- [ ] Build release binaries (macOS .app, Windows .exe if applicable)
- [ ] Code sign macOS app bundle (if applicable)
- [ ] Create installer package (DMG for macOS)
- [ ] Send download link to beta testers (email/Slack)
- [ ] Provide quick start guide (session setup, basic usage)
- [ ] Create feedback form (Google Form or GitHub Discussions)

**Acceptance Criteria:**

- At least 3 beta testers receive v0.2.0 build
- Quick start guide provided (< 1 page)
- Feedback mechanism in place (form or GitHub issue template)

**Build Commands:**

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# macOS: Create DMG (example)
hdiutil create -volname "OCC v0.2.0" -srcfolder build-release/OrpheusClipComposer.app -ov -format UDZO occ-v0.2.0-macos.dmg
```

---

### Priority 2: v0.2.1 Planning (Next Sprint)

#### Task 2.1: Gather Beta Tester Feedback

**Status:** 🟡 Pending Task 1.6 (starts after v0.2.0 deployed)
**Estimated Time:** 3-5 days (waiting period)

**Requirements:**

- [ ] Monitor feedback form responses
- [ ] Check GitHub Issues for bug reports
- [ ] Join beta tester Slack/Discord for real-time feedback
- [ ] Categorize feedback: bugs, feature requests, UX improvements
- [ ] Prioritize top 3 issues for v0.2.1

**Acceptance Criteria:**

- Feedback from at least 3 beta testers
- Top issues identified and documented
- v0.2.1 backlog created in GitHub Issues (milestone: v0.2.1)

---

#### Task 2.2: Create v0.2.1 Sprint Plan

**Status:** 🟡 Pending Task 2.1
**Files to Create:** `apps/clip-composer/docs/OCC/OCC103.md` (v0.2.1 Sprint Plan)
**Estimated Time:** 2 hours

**Requirements:**

- [ ] Review deferred issues from OCC093 (audio device UI, latch tuning, modal styling)
- [ ] Incorporate beta tester feedback
- [ ] Define 3-5 issues for v0.2.1 sprint
- [ ] Estimate effort (time, complexity)
- [ ] Create sprint timeline (target: 1-2 weeks)

**Candidate Issues (from OCC093 "Next Steps"):**

1. Audio device selection UI (Issue #1 from OCC093)
2. Latch acceleration tuning (Issue #5 feedback)
3. Modal dialog styling (Issue #6 from OCC093)
4. [New] Issues from beta tester feedback

**Acceptance Criteria:**

- OCC103 document created with sprint plan
- GitHub milestone `v0.2.1` created with issues
- Sprint ready to start after v0.2.0 feedback period

---

#### Task 2.3: Update MVP Roadmap

**Status:** 🟡 Pending Task 2.2
**Files to Update:** `apps/clip-composer/docs/OCC/OCC026.md` (MVP Definition)
**Estimated Time:** 1 hour

**Requirements:**

- [ ] Mark v0.2.0 as complete in OCC026 roadmap
- [ ] Update timeline (Month 2 → v0.2.0 complete)
- [ ] Add v0.2.1 to roadmap
- [ ] Adjust Month 4-6 milestones based on current velocity

**Acceptance Criteria:**

- OCC026 reflects current progress (v0.2.0 complete)
- Roadmap shows realistic path to MVP (Month 6 target)
- Stakeholders can see updated timeline

---

### Priority 3: Technical Debt & Maintenance

#### Task 3.1: Code Review - v0.2.0 Sprint Changes

**Status:** 🔴 Not Started
**Estimated Time:** 2 hours

**Requirements:**

- [ ] Review all commits from OCC093 sprint (7763b7a0 → a1870737)
- [ ] Check for TODO comments → create GitHub issues
- [ ] Check for hardcoded values → extract to constants
- [ ] Check for duplicated code → refactor to shared functions
- [ ] Check for missing error handling → add defensive checks
- [ ] Run clang-tidy on modified files

**Files to Review:**

- `Source/AudioEngine.h`, `Source/AudioEngine.cpp`
- `Source/UI/ClipEditDialog.h`, `Source/UI/ClipEditDialog.cpp`
- `Source/UI/ClipGrid.h`, `Source/UI/ClipGrid.cpp`
- `Source/MainComponent.cpp`

**Acceptance Criteria:**

- All TODO comments converted to GitHub issues
- No clang-tidy warnings on modified files
- Code follows JUCE best practices

**Commands:**

```bash
clang-tidy Source/AudioEngine.cpp -- -I/path/to/JUCE/modules -std=c++20
```

---

#### Task 3.2: Update OCC Documentation Index

**Status:** 🔴 Not Started
**Files to Update:** `apps/clip-composer/docs/OCC/INDEX.md`
**Estimated Time:** 30 minutes

**Requirements:**

- [ ] Add OCC102 (this document) to index
- [ ] Add OCC103 (v0.2.1 Sprint Plan) placeholder
- [ ] Update status of OCC093 (complete)
- [ ] Archive old/superseded docs to `docs/OCC/archive/`
- [ ] Verify all links in INDEX.md are valid

**Acceptance Criteria:**

- INDEX.md reflects current documentation state
- All active docs listed and linked
- No broken links

---

#### Task 3.3: Clean Up Build Artifacts

**Status:** 🔴 Not Started
**Estimated Time:** 15 minutes

**Requirements:**

- [ ] Delete old build directories (`build/`, `build-release/`)
- [ ] Clean CMake cache (fresh build)
- [ ] Remove any test session files from repo root
- [ ] Ensure `.gitignore` excludes build artifacts

**Commands:**

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
rm -rf build build-release
git clean -fdx  # CAUTION: deletes untracked files
```

**Acceptance Criteria:**

- Clean working directory (git status clean)
- Fresh build succeeds after cleanup
- No accidental commits of build artifacts

---

## Success Criteria (Track Complete)

**v0.2.0 Release:**

- [ ] QA passed (all 6 fixes verified, no critical bugs)
- [ ] GitHub release published (`v0.2.0-alpha` tag)
- [ ] Beta testers deployed (3+ users)
- [ ] CHANGELOG.md created and accurate

**v0.2.1 Planning:**

- [ ] Beta feedback collected (3+ responses)
- [ ] OCC103 Sprint Plan created
- [ ] MVP roadmap updated (OCC026)

**Technical Health:**

- [ ] Code review complete (no new tech debt)
- [ ] Documentation index current
- [ ] Build artifacts cleaned

---

## Dependencies

**External:**

- Beta testers available for v0.2.0 testing
- macOS code signing certificate (if deploying signed builds)

**Internal:**

- SDK Track (ORP099) - No blocking dependencies, but v0.2.1 may integrate SDK v1.0 features

**Coordination:**

- If beta testers report SDK bugs, coordinate with SDK Track agent
- If SDK Track completes gain/loop tests first, integrate SDK v1.0 into OCC v0.2.1

---

## Next Steps After Completion

1. **v0.2.1 Sprint** - Execute OCC103 plan (3-5 issues based on feedback)
2. **MVP Progress** - Continue toward Month 4 milestone (16-clip demo with routing)
3. **SDK v1.0 Integration** - Integrate SDK v1.0 when ready (parallel track coordination)

---

## Notes for Agent

**Working Directory:** `/Users/chrislyons/dev/orpheus-sdk/apps/clip-composer`

**Build Commands:**

```bash
# IMPORTANT: Use relaunch script (don't use `open` command)
./apps/clip-composer/launch.sh  # From repo root
# OR
./launch.sh  # From apps/clip-composer/
```

**Testing:**

```bash
# Load reference session
./launch.sh /path/to/test_session_960clips.json
```

**Documentation:**

- OCC docs in `apps/clip-composer/docs/OCC/`
- Session format reference in `OCC097.md`
- SDK integration patterns in `OCC096.md`
- Troubleshooting in `OCC101.md`

**When Blocked:**

- Check OCC093 for known working state
- Check OCC101 for troubleshooting common issues
- Ask user for clarification (don't assume missing requirements)

**Multi-Instance Coordination:**

- If SDK Track needs OCC testing → pause OCC work and assist
- If OCC finds SDK bugs → create GitHub issue and notify SDK Track agent

---

## References

[1] OCC093 v0.2.0 Sprint Completion Report: `apps/clip-composer/docs/OCC/OCC093 v020 Sprint - Completion Report.md`
[2] OCC026 MVP Definition: `apps/clip-composer/docs/OCC/OCC026.md`
[3] OCC099 Testing Strategy: `apps/clip-composer/docs/OCC/OCC099.md`
[4] OCC100 Performance Requirements: `apps/clip-composer/docs/OCC/OCC100.md`
[5] OCC101 Troubleshooting Guide: `apps/clip-composer/docs/OCC/OCC101.md`

---

**Created:** 2025-10-31
**Last Updated:** 2025-10-31
**Status:** Active - Ready for OCC development agent
