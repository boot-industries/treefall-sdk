<!-- SPDX-License-Identifier: MIT -->

# ORP133 — NOW Sprint: Realtime Callback Safety & Contract Truth

**Horizon:** Sprint 1 (immediate) · **Parent:** [[ORP132]] · **Verification:** [[ORP136]]
**Status:** Proposed — ready to execute
**Scope:** 100% local to `orpheus-sdk/`. No app-repo changes.
**Risk:** Low–Medium. Contained changes to a single hot file + docs.
**Suggested branch:** `feat/orp133-contract-truth`

---

## 1. Goal

Ship confidence and honesty before deeper surgery: remove the one remaining
`std::function` hazard on the audio→UI path, resolve the `StopGroup` no-op, make the
command-queue threading contract explicit and enforced, and correct every stale/
untrue statement in the public docs and version metadata.

**None of this touches the file-read-on-audio-thread problem** — that is deliberately
deferred to ORP134 where it gets the runtime harness and its own risk budget. NOW is
about the changes that are cheap, safe, and make the SDK's public contract true.

---

## 2. Why now (evidence)

| Task | Evidence | Current state |
|------|----------|---------------|
| Outbound `std::function` ring | `transport_controller.h:364-370`, `transport_controller.cpp:1334-1355` | Audio thread stores/moves `std::function<void()>` into the callback ring. The inbound command queue is already POD (ORP127) — this is the *last* `std::function` on a hot path. |
| `StopGroup` no-op | `transport_controller.cpp:755-758` | Literal `// For now, this is a no-op`. Public `stopAllInGroup()` silently does nothing. |
| Command producer contract | `transport_controller.cpp:1293-1305` vs `include/orpheus/transport_controller.h:153-160` | Impl is strict SPSC ("UI thread is the sole producer"); public header says start/stop are "callable from UI thread" and queries "from any thread" — under-specified for multi-producer hosts. |
| Version drift | `README.md:9` (`v1.0.0-rc.1`) vs `CMakeLists.txt:4` (`0.3.0`) | Two different "current versions" in the same repo. |
| Stale app paths | `README.md:345,352,364,441-444,482-499`; `ARCHITECTURE.md:369,550,811-812`; `docs/API_SURFACE_INDEX.md:111-125`; `ROADMAP.md` (OCC paths) | Docs still describe in-tree `apps/clip-composer/` and an obsolete `ORPHEUS_ENABLE_APP_CLIP_COMPOSER` build path. Clip Composer was extracted (ORP131). |

---

## 3. Work items

### G1 — Replace the outbound `std::function` callback ring with POD events

**Problem.** `std::array<std::function<void()>, 256> m_callbackRing` plus
`postCallback(std::function<void()>)` invoked from `processAudio()` violates the
repo's own callback rule (`REALTIME_AUDIT.md`: no "`std::function` ownership changes
on realtime callbacks"). Small-object optimization may hide allocation today, but it
is not a portable guarantee.

**Design.** Introduce a fixed POD event and enqueue only that from the audio thread;
translate to host callbacks on the UI thread in `processCallbacks()`.

```cpp
enum class TransportEventType : uint8_t {
  ClipStarted, ClipStopped, ClipLooped, ClipRestarted, ClipSeeked, BufferUnderrun
};

struct TransportEvent {            // trivially copyable POD
  TransportEventType type;
  ClipHandle         handle;
  uint32_t           voiceId;
  TransportPosition  position;     // already POD
};
```

- Replace `m_callbackRing` with `std::array<TransportEvent, CALLBACK_QUEUE_SIZE>`.
- Audio thread writes `TransportEvent`s (no `std::function`, no captures).
- `processCallbacks()` switches on `type` and calls the existing `ITransportCallback`
  virtuals (`onClipStarted`, …, `onBufferUnderrun` — the latter already declared,
  `transport_controller.h:145`).
- Keep the existing SPSC ring mechanics, memory ordering, and `m_droppedCallbackCount`
  diagnostic — only the payload type changes.

**Constraint.** Preserve current callback *ordering* and *timing* semantics. Event
emission points must map 1:1 to today's `postCallback` sites.

**Downstream note:** the public `ITransportCallback` interface is unchanged, so
FourTrack/Clip Composer see no API break — this is an internal payload swap.

### G2 — Resolve `StopGroup` (implement or remove)

Two acceptable outcomes; pick based on whether group identity exists in the session:

- **Implement:** wire `stopAllInGroup(groupIndex)` to stop active voices whose clip
  belongs to `groupIndex`. Requires a group→clip mapping the audio thread can read
  lock-free (a published snapshot, mirroring the ORP127 voice-snapshot pattern), since
  the TODO's blocker is "get clip group assignments from SessionGraph."
- **Remove/deprecate:** if group semantics belong in the app facade (Clip Composer
  models its own groups), mark the ABI/interface method deprecated, return
  `SessionGraphError::NotSupported`, and document that group-stop is an app concern.
  Delete the dead `StopGroup` command arm.

**Decision input needed:** does any *SDK-level* consumer need group-stop, or is it
purely a soundboard-facade concern? Default recommendation: **remove from core**,
because Clip Composer already owns grouping and no other consumer uses it — but the
executing agent must confirm against `scene_manager.h`/routing before deleting.

### G3 — Make the command-queue producer contract explicit & enforced

- **Document** the real contract on the public interface
  (`include/orpheus/transport_controller.h`): control-mutating methods are
  **single-producer** (one control thread) unless the host serializes access; queries
  are lock-free readers of a published snapshot.
- **Enforce in debug:** add a lightweight producer-thread check in `postCommand()`
  (capture the first producer `std::thread::id`; `assert` subsequent producers match)
  compiled only under `!NDEBUG`. Zero cost in release.
- **Provide the escape hatch (doc-only for NOW):** note that hosts with multiple
  control sources (UI + MIDI + OSC) must funnel through a single dispatcher; a first-
  class SDK MPSC dispatcher is deferred to ORP135.

This closes the contract gap without changing the queue's fast path.

### G4 — Version alignment (single source of truth)

- Choose the truthful version. Given `CMakeLists.txt` says `0.3.0` and the ABI is
  `1.0`, decide: is the SDK `0.3.x` (pre-1.0 library, 1.0 ABI) or `1.0.0-rc.1`?
  **Recommendation:** make CMake authoritative and set README/CHANGELOG to match the
  CMake version; the "v1.0.0-rc.1" README banner appears aspirational vs. the build.
  Confirm against `CHANGELOG.md` history before flipping.
- Wire the version once (CMake `project(... VERSION ...)`) and have README/docs
  reference it rather than restating a literal.

### G5 — Documentation truth pass

Fix every stale statement surfaced in §2 so the SDK contract matches the repo:

- README: remove `apps/clip-composer` working-dir and OCC doc-path sections; replace
  with "Clip Composer is an external downstream repo (`chrislyons/clip-composer`) that
  consumes this SDK as a submodule."
- `ARCHITECTURE.md`: delete the `ORPHEUS_ENABLE_APP_CLIP_COMPOSER` build recipe and
  in-tree OCC paths; update the Applications section to list `apps/wave-finder/`
  (app-platform smoke test) and describe external consumers.
- `docs/API_SURFACE_INDEX.md`: correct the `apps/` description; keep the archived-
  TypeScript note but ensure it doesn't imply live packages.
- `ROADMAP.md`: repoint OCC references to the external repo; re-express M4/M6 items
  that this program now owns as pointers to ORP134/ORP135.
- Clarify status of `packages/occ-app-platform` and `packages/shmui-juce` (active,
  transitional, or archived) — one authoritative sentence each.

### G6 — Docs-path validation in CI (prevent regression)

Add a lightweight CI check (extend the existing lint job) that fails on:
- internal doc links pointing at nonexistent paths,
- references to `apps/clip-composer` (now external),
- references to removed CMake options.

This is the guardrail that keeps G5 from rotting. See ORP136 §"Docs & API gates."

---

## 4. Acceptance gates (Definition of Done)

- [ ] No `std::function` remains on any audio-thread path; `tools/realtime_audit.py`
      no longer reports a `std::function` finding for the callback ring.
- [ ] `callback_queue_stress_test.cpp` passes with the POD event ring; a new test
      asserts event **ordering** is preserved vs. the old behavior.
- [ ] `voice_state_tsan_test.cpp` and the full transport suite pass under TSAN and
      ASan/UBSan.
- [ ] `stopAllInGroup()` either does what it says (with a test) or returns a
      documented `NotSupported` (with a test) — never a silent no-op.
- [ ] Debug producer-thread assertion present in `postCommand()`; release unaffected.
- [ ] `README`, `CMakeLists.txt`, `CHANGELOG` agree on exactly one version.
- [ ] `grep -r "apps/clip-composer" README.md ARCHITECTURE.md ROADMAP.md docs/` returns
      nothing (or only historical/archival mentions clearly labeled as such).
- [ ] New docs-path CI check is green and would fail on a reintroduced stale path.
- [ ] `CHANGELOG.md` updated; public header docs updated where contracts changed.

---

## 5. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Event-ring change alters callback ordering/timing | Add an explicit ordering test; map emission points 1:1; review under TSAN. |
| Removing `StopGroup` breaks a hidden consumer | Grep all consumers (in-repo + adapters) first; prefer deprecate-and-return-NotSupported over hard delete if any doubt. |
| Version flip confuses downstream pins | Announce in `CHANGELOG`; the app repos pin by commit, so no runtime break — just clarity. |
| Doc cleanup surfaces *more* stale assumptions | Expected and welcome; capture extras as ORP134 follow-ups rather than expanding this sprint. |

---

## 6. Downstream follow-up (separate sprints — NOT this session)

- None required for G1–G6: the public `ITransportCallback` and transport API are
  unchanged. App repos need no edits from this sprint. (This is the payoff of keeping
  NOW additive/internal.)

---

## 7. Out of scope for NOW (goes to ORP134)

- Streaming/prepared-media reader (file reads off the audio thread).
- Identity/time primitives, graph seam, render-hash, writer, analysis, recorder.

*Next: [[ORP134]] once this merges.*
