# OCC152 Transport Unification Smoke-Test Guide

**Status:** Ready to run (by-ear verification, deferred from OCC151 session)
**Scope:** Manual verification of the three OCC151 fixes on `feat/occ-transport-unification`
**Related:** OCC151 (sprint), ORP127 (SDK voice/SRC), ORP128 (CoreAudio device-rate gap)
**Audience:** Operator running Clip Composer locally

---

## Before you start

### Pre-flight (avoids the false alarm from this session)

The bitcrush distortion seen on 2026-07-07 was **not** a code bug — it was a
device/engine sample-rate mismatch (see ORP128). Rule it out first:

1. Open **Audio MIDI Setup**. Note the **Format / sample rate** of the output
   device you're monitoring on (e.g. MacBook Pro Speakers).
2. Launch Clip Composer via `./scripts/relaunch-occ.sh`. Its engine runs at
   **48000 Hz** by default.
3. **Make the OS output device match the engine (48000 Hz).** If they differ,
   either set the device to 48000 in Audio MIDI Setup, or set OCC's Audio
   Settings → Sample Rate to match the device. Mismatched rates = bitcrush on
   every clip; matched rates = clean. This is the single most common false alarm.
4. Use clips whose media actually resolves. The current session's imported set is
   mostly missing on disk (they load as "unresolved / missing media",
   `0 Hz, 0 ch`, and won't play). Known-good in this session: buttons **11, 16,
   17, 22** (all real 48000 Hz / 2 ch files). Load fresh clips if needed.

### Watching the logs while you test

- App stdout: `/tmp/occ_output.log`
- CoreAudio init/rate diagnostics: `/tmp/coreaudio_init.log`
- Tail live: `tail -f /tmp/occ_output.log`

A clean run has **no** new `Assertion failure`, no `Buffer underrun`, and each
start/stop appears exactly once per action.

---

## Test 1 — Grid ↔ Edit-dialog transport unification (G1)

**What OCC151 changed:** the Edit dialog no longer allocates its own cue-buss
handle for the same file. It drives the grid clip's transport directly (via
`buttonIndex`). One clip identity = one voice. This kills both desync and the 2×
gain stacking.

**Steps:**
1. Load a real clip onto a grid button (e.g. button 11).
2. **Play from the grid.** Note the loudness. Let it run.
3. Open that clip's **Edit dialog** while it's playing.
   - ✅ PASS: the dialog's transport position tracks the grid position (same
     playhead), and loudness is **unchanged** — the clip is not suddenly ~6 dB
     louder (that would be the old 2× stacking).
   - ❌ FAIL: the dialog spawns a second, independent playhead, or the clip
     audibly doubles in level when the dialog opens.
4. **Stop from the dialog.** The grid button must show stopped too (single
   source of truth).
5. Reverse it: start from the dialog, confirm the grid button reflects playing,
   stop from the grid, confirm the dialog reflects stopped.
6. Scrub / seek in the dialog while playing.
   - ✅ PASS: the grid playhead follows; no second voice appears; no doubling.

**Expected log:** one `Started clip on button N` per play, one `Stopped clip on
button N` per stop. You should **not** see cue-buss allocation for a grid clip's
dialog.

---

## Test 2 — Playgroup-scoped choke / "Stop Others" (G6)

**What OCC151 changed:** "Stop Others On Play" is scoped to the firing clip's
**playgroup** (`clipGroup` 0–3, shown as Group 1–4), not the visible tab and not
global. Decision lives in the pure `occ::shouldChokeStop` policy. Firing a clip
in Group 1 must never stop a clip in Group 2.

**Setup:**
- Load at least three real clips.
- Assign clip **X** and clip **Y** to the **same** group (e.g. Group 1), and clip
  **Z** to a **different** group (e.g. Group 2). (Group is set per-clip in the
  clip's properties/Edit.)
- Enable **"Stop Others On Play"** on clip **X**.

**Steps:**
1. Start clip **Y** (Group 1) and clip **Z** (Group 2) so both are playing.
2. Fire clip **X** (Group 1, Stop-Others ON).
   - ✅ PASS: **Y stops** (same group), **Z keeps playing** (different group).
   - ❌ FAIL: Z also stops (global choke — the old bug), or Y does not stop.
3. Repeat firing X from a **different tab** than Y lives on — scoping must follow
   the *group*, not the visible tab. Y (same group) should still stop regardless
   of which tab is on screen.
4. Sanity: fire a clip with Stop-Others **OFF** — nothing else should stop.

**Note:** this path is already covered by unit tests
(`tests/test_choke_policy.cpp`, 5 tests, passing). Test 2 is the by-ear
confirmation that the UI wires `stopOthersInPlaygroup()` to the same policy.

---

## Test 3 — Rapid re-fire / fade-overlap, voice cap 2 (voice model)

**What OCC151 changed:** every grid clip is registered
`MonoWithFadeOverlap`, transport `setMaxVoicesPerClip(2)`. Firing a live voice
restarts it in place; firing while only a fade-out tail exists starts a fresh
voice alongside the tail — so voices == 2 **only** during a fade-overlap window.
No unbounded stacking, no bitcrush from summed duplicates.

**Steps:**
1. On a clip **with a fade-out** configured (e.g. 0.6–0.8s out), hammer the
   button rapidly 5–10 times.
   - ✅ PASS: it restarts cleanly each time; at most a brief crossfade overlap;
     level stays controlled; no runaway build-up, no digital clipping/bitcrush.
   - ❌ FAIL: level climbs with each press (stacking), or it degrades into
     crushed/aliased distortion.
2. On a clip with **no fade-out**, hammer it.
   - ✅ PASS: hard restart in place, single voice, no doubling.
3. Let a clip play to its natural fade-out tail, and re-fire it *during* the
   fade.
   - ✅ PASS: the tail completes while a fresh voice starts — a smooth overlap,
     then back to one voice. Never more than 2 voices for that clip.

**Expected log:** no `Buffer underrun`, no assertions during the hammering.

---

## Test 4 — Device change safety (G2/G4) — optional but valuable

**What OCC151 changed:** device swaps quiesce the audio thread before moving the
transport/driver pointers, and restarts route through the OCC wrappers.

**Steps:**
1. With a clip playing, open **Audio Settings** and switch the output device (or
   sample rate / buffer size).
   - ✅ PASS: audio resumes on the new device with no crash, no stuck voice, no
     doubled playback. A clip that was playing restarts cleanly on the new device
     (or stops cleanly), not as a stacked voice.
   - ❌ FAIL: crash, hang, ASan report, or a duplicated/stacked voice after the
     swap.
2. **Related to ORP128:** if you switch to a device whose rate differs from the
   engine and hear bitcrush, that is the *known driver-layer gap* (device-rate ≠
   engine-rate), not a swap bug. Match the rates. Track the real fix in ORP128.

---

## If something fails

- Capture `/tmp/occ_output.log` and `/tmp/coreaudio_init.log`.
- Note: which test, which buttons/groups, exact sequence, and whether rates
  matched (rule out ORP128 first).
- For a suspected regression, the sprint code is on
  `feat/occ-transport-unification` (HEAD `b742278e`); diff against it.

## Definition of done for this guide

- [ ] Test 1: grid/dialog share one transport; no 2× gain; positions track.
- [ ] Test 2: choke stops same-group only; different group untouched; tab-agnostic.
- [ ] Test 3: rapid re-fire bounded to ≤2 voices; no stacking; no bitcrush.
- [ ] Test 4 (optional): device swap is crash-free and voice-clean.
- [ ] No new assertions / underruns in the logs across all tests.

Passing all of the above closes the by-ear verification deferred from OCC151.
T12 (pin SDK to the `v0.3.0` tag) remains separately blocked until the SDK agent
cuts that tag.
