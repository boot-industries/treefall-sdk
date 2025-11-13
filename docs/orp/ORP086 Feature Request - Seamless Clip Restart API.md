# SDK Feature Request: Seamless Clip Restart API

**Status:** Proposed
**Priority:** High (Blocking OCC v0.3.0-alpha completion)
**Requested By:** Orpheus Clip Composer Team
**Date:** 2025-10-27
**Affects:** M2 Real-Time Infrastructure (ITransportController)

---

## Problem Statement

**Current Behavior:**

The SDK's `startCueBuss(ClipHandle handle)` method is idempotent:

- If clip is NOT playing → starts playback from IN point ✅
- If clip IS playing → returns `true` but does NOT restart (no-op) ❌

**Impact on OCC:**

Orpheus Clip Composer's Edit Dialog has < > trim buttons (SpotOn-style workflow) that adjust the IN point while audio is playing. The expected behavior is:

1. User clicks `IN >` button to nudge IN point forward by 1/75 second
2. **Playback immediately restarts from new IN point** (seamless, no gap)
3. User hears the new IN point instantly for precise trim editing

**Current Workaround (Unacceptable):**

```cpp
void PreviewPlayer::play() {
  // WORKAROUND: Stop then start to force restart
  if (m_isPlaying) {
    m_audioEngine->stopCueBuss(m_cueBussHandle);  // ❌ Creates audible gap
  }
  m_audioEngine->startCueBuss(m_cueBussHandle);
}
```

**Result:** Audible stop/start gap on every < > button click, creating a "stuttering" effect that breaks the professional editing workflow.

**Logs showing the issue** (from `/tmp/occ.log`, lines 74-282):

```
ClipEditDialog: > button clicked - New IN: 1280
PreviewPlayer: Stopped for synchronous restart          ← Audible gap
AudioEngine: Stopped Cue Buss 10001
AudioEngine: Started Cue Buss 10001
PreviewPlayer: Restarted playback from IN point
```

---

## Proposed Solution

Add a new API method to `ITransportController` (or `AudioEngine` for Cue Buss):

```cpp
/**
 * Restart clip/Cue Buss playback from current IN point (seamless, no gap)
 *
 * Unlike startCueBuss(), this method ALWAYS restarts playback even if already playing.
 * The restart is sample-accurate and gap-free, using audio thread-level position reset.
 *
 * Use Cases:
 * - OCC Edit Dialog < > trim buttons (restart after IN point change)
 * - Professional soundboard "re-fire" behavior (restart clip from top)
 * - Live performance "panic restart" (recover from drift/sync issues)
 *
 * @param handle Cue Buss handle
 * @return true if restart succeeded, false if handle invalid or clip not loaded
 *
 * Thread Safety: Lock-free, safe to call from message thread
 * Real-Time Safety: Restart happens in audio thread (no allocations, no blocking)
 * Sample Accuracy: Position reset is sample-accurate (±0 samples)
 */
bool restartCueBuss(orpheus::ClipHandle handle);
```

**Alternative Naming Options:**

- `restartCueBuss()` (recommended - clear intent)
- `rewindCueBuss()` (implies restart from IN point)
- `retrigerCueBuss()` (professional soundboard terminology)

---

## Implementation Guidance (SDK Team)

### Recommended Approach

**In `AudioEngine::restartCueBuss()`:**

```cpp
bool AudioEngine::restartCueBuss(orpheus::ClipHandle handle) {
  // 1. Validate handle
  auto* cueBuss = findCueBuss(handle);
  if (!cueBuss || !cueBuss->isLoaded) {
    return false;
  }

  // 2. Post command to audio thread (lock-free queue)
  Command cmd;
  cmd.type = CommandType::RESTART_CUE_BUSS;
  cmd.handle = handle;
  m_commandQueue.push(cmd);  // Lock-free, audio thread will process

  return true;
}
```

**In audio thread (`AudioEngine::processAudioCallback()`):**

```cpp
void AudioEngine::processAudioCallback(float** outputs, int numChannels, int numSamples) {
  // Process pending commands (lock-free)
  while (auto cmd = m_commandQueue.pop()) {
    if (cmd->type == CommandType::RESTART_CUE_BUSS) {
      auto* cueBuss = findCueBuss(cmd->handle);
      if (cueBuss) {
        // CRITICAL: Reset position to IN point (sample-accurate, no gap)
        cueBuss->currentPosition = cueBuss->metadata.trimInSamples;
        cueBuss->isPlaying = true;  // Ensure playing state

        // NO stop/start gap - just reset position and continue rendering
        // Next processAudio() call will read from new position seamlessly
      }
    }
  }

  // Continue normal audio rendering...
  renderCueBusses(outputs, numChannels, numSamples);
}
```

**Key Implementation Details:**

1. **No stop/start gap:** Just reset `currentPosition`, don't change playback state
2. **Sample-accurate:** Position reset happens between audio buffers (±0 samples)
3. **Lock-free:** Use command queue (existing pattern in SDK)
4. **No allocations:** All state changes are atomic updates
5. **Respect fade-in:** After restart, apply fade-in from metadata (if configured)

---

## Acceptance Criteria

### Functional Requirements

- [ ] `restartCueBuss(handle)` restarts playback from IN point even if already playing
- [ ] Restart is seamless (no audible gap, glitch, or pop)
- [ ] Position reset is sample-accurate (±0 samples from IN point)
- [ ] Method is idempotent (safe to call multiple times)
- [ ] Invalid handles return `false` without crashing

### Performance Requirements

- [ ] Restart latency <1ms (audio thread processes command within 1 buffer)
- [ ] No allocations in audio thread
- [ ] No blocking operations (lock-free command queue)
- [ ] CPU overhead <0.1% (position reset is trivial)

### Testing Requirements

```cpp
TEST(CueBussTest, RestartFromInPoint) {
  auto engine = createAudioEngine(48000, 512);
  auto handle = engine->allocateCueBuss("test.wav");

  engine->startCueBuss(handle);
  engine->processAudio(/* ... */);  // Play 512 samples

  // Position should be at sample 512
  EXPECT_EQ(engine->getCueBussPosition(handle), 512);

  // Restart should reset to IN point (0)
  EXPECT_TRUE(engine->restartCueBuss(handle));
  engine->processAudio(/* ... */);

  // Position should be back at IN point + 512 samples
  EXPECT_EQ(engine->getCueBussPosition(handle), 512);
}

TEST(CueBussTest, RestartWithTrimIn) {
  auto engine = createAudioEngine(48000, 512);
  auto handle = engine->allocateCueBuss("test.wav");
  engine->updateCueBussMetadata(handle, 48000, /* trimOut */ 96000, /* fades... */);

  engine->startCueBuss(handle);
  engine->processAudio(/* ... */);  // Play from sample 48000

  // Position should be trimIn + 512
  EXPECT_EQ(engine->getCueBussPosition(handle), 48512);

  // Restart should go back to trimIn
  EXPECT_TRUE(engine->restartCueBuss(handle));
  engine->processAudio(/* ... */);

  EXPECT_EQ(engine->getCueBussPosition(handle), 48512);
}

TEST(CueBussTest, RestartSeamlessNoGap) {
  // Capture audio output before/after restart
  // Verify no silence gap, no pop, no glitch
  // (Advanced test - use FFT or zero-crossing detection)
}
```

---

## Related SDK Methods (For Context)

**Existing Cue Buss API:**

```cpp
// Allocate Cue Buss (returns handle)
orpheus::ClipHandle allocateCueBuss(const juce::String& filePath);

// Start playback (idempotent - does NOT restart if already playing)
bool startCueBuss(orpheus::ClipHandle handle);

// Stop playback
bool stopCueBuss(orpheus::ClipHandle handle);

// Update metadata (trim points, fades)
void updateCueBussMetadata(orpheus::ClipHandle handle, int64_t trimIn, int64_t trimOut, ...);

// Get current position (sample-accurate)
int64_t getCueBussPosition(orpheus::ClipHandle handle);
```

**Proposed Addition:**

```cpp
// ✨ NEW: Restart playback from IN point (seamless, no gap)
bool restartCueBuss(orpheus::ClipHandle handle);
```

---

## Use Cases in OCC

### 1. Edit Dialog < > Trim Buttons (Primary Use Case)

**Workflow:**

1. User opens Edit Dialog for a clip
2. Clicks ► (Play) to start preview playback
3. Clicks `IN >` button repeatedly to nudge IN point forward (1/75 sec increments)
4. **Expected:** Each click restarts playback from new IN point seamlessly
5. **Current:** Each click causes audible stop/start gap (unprofessional)

**Code:**

```cpp
// In ClipEditDialog.cpp (IN > button handler)
m_trimInIncButton->onClick = [this]() {
  int64_t increment = m_metadata.sampleRate / 75;  // 1/75 sec (SpotOn standard)
  m_metadata.trimInSamples += increment;

  // Update preview player
  if (m_previewPlayer) {
    m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    // ↑ This calls restartCueBuss() internally if playing
  }
};
```

### 2. Main Grid "Re-Fire" Behavior (Future Enhancement)

**Workflow:**

1. Clip is playing in main grid (e.g., background music)
2. User clicks the same clip button again to "re-fire" it
3. **Expected:** Clip restarts from beginning (professional soundboard behavior)
4. **Alternative to:** Stop then start (which creates gap)

**Code:**

```cpp
// In ClipButton.cpp (main grid)
void ClipButton::mouseDown(const juce::MouseEvent& e) {
  auto handle = getClipHandle();

  if (m_audioEngine->isClipPlaying(handle)) {
    // Re-fire: Restart from beginning (no gap)
    m_audioEngine->restartCueBuss(handle);
  } else {
    // Start normally
    m_audioEngine->startCueBuss(handle);
  }
}
```

### 3. Loop Restart on Metadata Change (Future Enhancement)

**Workflow:**

1. Clip is looping with OUT point at 10 seconds
2. User changes OUT point to 5 seconds in Edit Dialog
3. **Expected:** Loop immediately restarts from IN point to honor new OUT point
4. **Alternative to:** Wait for current loop iteration to finish (confusing UX)

---

## Timeline and Priority

**Priority:** **High** - Blocking OCC v0.3.0-alpha completion

**Current OCC Status:**

- ✅ Sprint 1 Complete: Broadcast-safe playback
- ✅ Sprint 2 Complete: Loop, Fade, Audio Settings
- ⏳ Sprint 3 Blocked: Edit Dialog < > buttons (requires this API)

**Requested Timeline:**

- **Week 1:** SDK team reviews proposal, confirms feasibility
- **Week 2:** Implementation in `AudioEngine` (M2 module)
- **Week 3:** Testing and integration with OCC
- **Week 4:** OCC v0.3.0-alpha release with seamless restart

**Impact if Delayed:**

- OCC Edit Dialog workflow remains unprofessional (audible gaps)
- Beta testers report poor editing experience
- Competitive disadvantage vs SpotOn/QLab (both have seamless restart)

---

## Alternative Workarounds (If SDK Change Not Feasible)

### Option 1: Accept the Gap (Not Recommended)

**Pros:** No SDK changes required
**Cons:** Unprofessional UX, competitive disadvantage, beta tester complaints

### Option 2: Disable Restart While Playing (Not Recommended)

**Workflow:** < > buttons only work when playback is stopped

**Pros:** No audible gap
**Cons:** Breaks SpotOn-style workflow (industry standard for soundboards)

### Option 3: Use Fade-Out/Fade-In to Mask Gap (Partial Solution)

**Workflow:** Apply 10ms fade-out before stop, 10ms fade-in after start

**Pros:** Reduces audible "pop" to smooth crossfade
**Cons:** Still noticeable gap, adds 20ms latency, not sample-accurate

**Code:**

```cpp
void PreviewPlayer::play() {
  if (m_isPlaying) {
    // Apply 10ms fade-out
    m_audioEngine->updateCueBussMetadata(m_cueBussHandle, ..., /* fadeOut */ 0.01f, ...);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Wait for fade
    m_audioEngine->stopCueBuss(m_cueBussHandle);
  }
  // Apply 10ms fade-in
  m_audioEngine->updateCueBussMetadata(m_cueBussHandle, ..., /* fadeIn */ 0.01f, ...);
  m_audioEngine->startCueBuss(m_cueBussHandle);
}
```

**Verdict:** Hacky, not sample-accurate, still has gap

---

## Summary

**Request:** Add `restartCueBuss(ClipHandle handle)` API for seamless, gap-free clip restart

**Why Needed:**

- OCC Edit Dialog < > trim buttons require instant restart from new IN point
- Current stop/start workaround creates audible gap (unprofessional)
- Industry standard workflow (SpotOn, QLab) expects seamless restart

**Implementation Complexity:** Low (position reset in audio thread, ~50 LOC)

**Impact:** High (unblocks OCC v0.3.0-alpha, enables professional editing workflow)

**Timeline:** Requested for Week 2-3 implementation, Week 4 OCC integration

---

## Contact

**OCC Team Lead:** Chris Lyons
**Slack Channel:** `#orpheus-occ-integration`
**Related Issues:** OCC Sprint 3 (Edit Dialog polish)
**Related Docs:** `apps/clip-composer/docs/OCC/OCC021` (Product Vision)

---

**Appendix: Log Evidence of Current Issue**

From `/tmp/occ.log` (2025-10-27 testing session):

```
[Line 74] ClipEditDialog: > button clicked - New IN: 1280
[Line 75] WaveformDisplay: setTrimPoints called with [1280, 14438656]
[Line 76] PreviewPlayer::setTrimPoints() CALLED - IN: 1280, OUT: 14438656, isPlaying: YES
[Line 79] PreviewPlayer: Trim points changed while playing, restarting from new IN point
[Line 80] AudioEngine: Stopped Cue Buss 10001          ← AUDIBLE GAP STARTS HERE
[Line 81] PreviewPlayer: Stopped for synchronous restart
[Line 82] AudioEngine: Started Cue Buss 10001          ← AUDIBLE GAP ENDS HERE (~10-20ms)
[Line 83] PreviewPlayer: Restarted playback from IN point
```

This pattern repeats on every < > button click, creating a "stuttering" effect that breaks professional editing workflow.
