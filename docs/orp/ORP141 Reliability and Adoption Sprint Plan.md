<!-- SPDX-License-Identifier: MIT -->

# ORP141 — Reliability and Adoption Sprint Plan

**Document type:** Product and engineering sprint plan  
**Status:** Implemented on `main` — R0/R1/R2/R4 complete; R3 evidence and gated R5 work explicitly deferred
**Scope:** Orpheus SDK core, packages, release artifacts, and SDK-owned conformance fixtures only. No child-app source changes, submodule-pin updates, application CI work, or UI feature work are deliverables of this plan.
**Related:** [[ORP136 TODO and Incomplete-Feature Triage]] · [[ORP137 Hardening Program Completion and Downstream Follow-ups]] · [[ORP135 LATER Sprint - Platform Leadership Bets]] · [[ORP142 Downstream Consumer Adoption Notes]] · FreqFinder [[FRQ033 Orpheus SDK Release Package Refresh for Analysis Facade]] · FourTrack [[FTR027 SDK Note - Real-Time Sample-Accurate Event Primitive]] · [[FTR028 SDK Note - Routing Matrix Block-Size Ceiling]]

---

## Implementation checkpoint — 2026-07-14

- Merged to `main` via GitHub PR #206 as `f5d703cd`.
- R0 release truth is complete: CMake-sourced version metadata, installed-target
  manifest, clean-prefix fixtures, package/ABI CI, support matrix, and release
  checksum/SBOM/provenance generation.
- R1 correctness is complete: deterministic routing snapshot provenance,
  stereo metering, atomic scene routing/clip recall, voice-cap refusal events,
  CoreAudio active-voice telemetry, large-block content checks, and canonical
  transport event timestamps.
- R2 media integrity is complete: provider-backed streaming SHA-256, versioned
  media references, explicit verified/unverified/missing/mismatch outcomes,
  mismatch refusal, atomic session save/backup recovery, schema migration, and
  installed-package coverage.
- R3 implementation is committed: all public factories carry DLL export
  annotations; WASAPI shared-mode enumeration/playback and negotiated
  capabilities are implemented; Windows compile/package tests are required;
  Linux provider boundaries and conformance gates are documented. Hosted CI is
  not real-device evidence. Promotion of WASAPI remains blocked until the
  self-hosted `wasapi-hardware-acceptance` workflow publishes a passing record.
- Local verification after R4 observed all 143 configured contracts passing.
  GitHub reports repository Actions as disabled (`enabled: false`), so
  latest-commit Windows CI run `29316779217` remains queued without job records.
  The user elected to keep Actions disabled; hosted Windows package/ABI proof
  is therefore deferred, not inferred from workflow configuration or macOS
  verification.
- GitHub reports zero self-hosted runners for this repository. A direct dispatch
  of `wasapi-hardware-acceptance.yml` also returned API `404` because that new
  manual workflow is not yet on the default branch. Real-device evidence
  requires the workflow to land plus a matching
  `[Windows, x64, audio-hardware]` runner. The R3 evidence jobs remain
  incomplete and Windows/WASAPI support is not promoted.
- R4 is complete. The public `SessionGraph` header now exposes stable
  `SessionId`/`TrackId`/`ClipId` edits, canonical `TimeRange` snapshots,
  coalesced revisioned change sets, rollback-on-destruction transactions, and
  snapshot restore for application-owned undo/redo. Runtime transport and
  application presentation state remain outside the transactional edit domain.
- The ShmUI-JUCE import manifest now declares its hash algorithm and is checked
  against all 53 imported files in CTest, alongside its source revision,
  exported targets/components, JUCE modules, token contract, and default-off
  OpenGL feature gate.
- A pinned, checksum-verified JUCE 8.0.4 fixture builds and runs an actual
  `add_subdirectory` consumer against `Orpheus::shmui_juce`, asserts that the
  OpenGL target and compile definition are absent, and runs in the Ubuntu
  Release CI leg.
- The transport now owns a public `RealtimeTelemetry` bridge: a fixed 64-slot
  SPSC ring with configurable block decimation, monotonic sequence/drop
  reporting, canonical post-block `TimePoint`, callback/underrun diagnostics,
  active-voice count, and fixed group/master meters. Full rings drop the newest
  capture rather than blocking or overwriting unread data.
- Telemetry is presentation-neutral by contract. FFTs, histories, smoothing,
  analyzer selection, and UI view models remain message-thread application
  state; the SDK audio path only records and publishes bounded POD snapshots.
- Local verification passed four queue/cadence/diagnostic tests, the live
  transport integration test, and the strict in-repository realtime audit. A
  clean-prefix `find_package` fixture also compiles and runs the public
  `SessionGraph` transaction/snapshot and `RealtimeTelemetry` APIs, including
  the transport-owned telemetry accessor, without any private header.
- R5 evidence review found one conditional requirement: FourTrack may consider
  the one-shot voice utility if R5 lands. ORP142 records no second independent
  consumer requirement, and R1–R4 have not shipped/soaked together. The R5
  implementation jobs therefore remain gated and are not started.
- The Release Operating Model section is explicitly deferred to a later
  session; none of its remaining gates are represented as complete here.

## 1. Decision

Orpheus should next become **easy to trust, easy to consume, and difficult to misuse**. The completed ORP133/ORP134 hardening work already provides the right substrate: realtime-safe prepared/streaming sources, deterministic offline rendering, identity/time value types, graph vocabulary, capture, writer, and analysis primitives.

Do **not** begin plugin hosting, network audio, realtime ML, or a broad C++-idiom rewrite. Those expand the surface before the released surface, media integrity, routing contracts, and supported-platform story are dependable. Plugin hosting and device-specific control drivers remain adapters, not core.

The work below is sequenced by:

1. **Integrity and reliability:** a valid artifact must resolve the intended API, render deterministically, report faults truthfully, and preserve/verify media identity.
2. **Useful functions:** finish APIs that claim to exist, then add only cross-host primitives with an identified adopter.
3. **Adoption:** make the SDK package, documentation, and SDK-owned conformance fixtures sufficient for downstream consumers; each app independently decides when to adopt a release.

---

## 2. Verified starting position

### Strong foundation to preserve

- `ctest --test-dir build-verify --output-on-failure` completed successfully for all **134** configured tests on 2026-07-13, including realtime static/runtime audits, render-hash determinism, installed CMake consumption, CoreAudio, capture, analysis, writer, routing, and ABI-link tests.
- ORP137 records completed strict in-repo realtime gating: no allocation or media I/O in transport rendering, silence plus `BufferUnderrun` on streaming cache miss, and bit-identical render hashes across supported block sizes.
- Clip Composer consumes SDK `v0.3.2`; it has adopted `prepareClipAudio`, panic, routing access, and independent voice sources. FourTrack has adopted the ORP134 writer, capture ring, and streaming source primitives. FreqFinder consumes the analysis facade and core DSP/metering/routing surfaces. These are real consumers, not hypothetical targets.
- The FTR027 live-tempo correction and FTR028 block-ceiling correction are already present in the current source: transport publishes session tempo through an atomic cache, and `processRouting()` chunks arbitrary-sized calls behind public `maxBlockFrames()`. These are regression-contracts to protect, not planned reimplementation.

### Gaps that block a leading SDK

| Priority | Gap | Evidence / impact | Required outcome |
| --- | --- | --- | --- |
| P0 | **Release artifacts are not trustworthy.** | `project(orpheus VERSION 0.3.2)` is authoritative, while README/API documentation retain older labels; FRQ033 reports a local package advertising `0.2.0` and missing the analysis facade. FreqFinder must force source-tree resolution. | A clean installed package exposes the current headers, targets, transitive links, version, and ABI contract; every consumer can use it without a source fallback. |
| P0 | **Windows is not a supported release gate.** | CI intentionally makes Windows manual because shared-library factory symbols are not all exported. Linux has no production backend commitment; the architecture correctly says ALSA, JACK, and PipeWire capabilities must stay distinct. | Declare and test a truthful platform matrix, repair Windows shared ABI/export conformance, then promote platform support only after real-device acceptance tests. |
| P0 | **Some shipped measurements and workflow features are incomplete or misleading.** | ORP136 identifies zero routing snapshot timestamps, one-channel-only stereo metering, swallowed active-clip-cap errors, a CoreAudio active-clip count fixed at zero, and incomplete scene capture/recall. | Correct values or explicit failures; no successful-looking API result may conceal unavailable functionality. |
| P1 | **Media/session integrity is incomplete.** | `IAudioFileReader` still has a SHA-256 TODO. Structural JSON validation alone cannot prove that a session resolves the original media bytes. | Versioned media fingerprints, verified load policy, atomic session writes, and diagnostics that distinguish missing, changed, and unreadable media. |
| P2 | **Cross-host control and one-shot triggering are absent.** | FourTrack has a local click implementation because the SDK has no RT-safe, sample-offset one-shot voice primitive; the ask is reusable but non-blocking. | A standalone preloaded-PCM trigger/voice utility with bounded polyphony, only after a second adopter confirms the same contract. |
| P2 | **Native UI sharing can drift.** | ShmUI owns the stable add-only Orpheus design-token contract; JUCE sources are maintained there and copied into `packages/shmui-juce`. A prior package regression broke `add_subdirectory` consumers and an unconditional OpenGL include widened linkage. | A versioned, tested ShmUI-JUCE import contract; core remains UI-free and apps receive compatible components/tokens predictably. |
| P3 | **Advanced platform bets need evidence, not aspiration.** | Automation, spatial layouts, control mapping, ABI facades, and WASM/mobile remain correctly dependency-gated in ORP135. | Each becomes a small proposal only when a named app commits to adoption and the prerequisite contract has soaked in releases. |

---

## 3. Product contract and non-negotiable gates

Every sprint that touches audio processing must preserve: offline-first operation; deterministic render output; no allocation, locks, I/O, logging, or unbounded work in callbacks; and host-neutral core semantics.

A public capability is complete only when all of the following are true:

1. The installed package exposes it through a documented target and a stable header.
2. A clean downstream consumer compiles, links, and exercises it without source-tree fallback.
3. Success, unavailable capability, invalid input, and runtime fault have distinct, queryable outcomes.
4. The behavior has a regression test that fails on a plausible implementation error.
5. Any realtime path is covered by the allocation/I/O harness, deterministic render test, and ThreadSanitizer where cross-thread state changes.

No API is advertised as complete while it returns placeholder values, swallows a resource-cap error, silently produces silence, or depends on undocumented build limits.

---

## 4. Sprint sequence

### Sprint R0 — Release truth and installed-consumer conformance

**Goal:** make `v0.3.2` a consumable SDK release rather than a source checkout that happens to work.

**SDK deliverables**

- Generate the version from the CMake project value everywhere: package config, API index, README banner, examples, and release metadata. CI rejects divergent version claims.
- Define and publish the supported installed-target manifest, including stable aliases for every SDK-supported consumer scenario. Build, install, and CPack that manifest from a clean tree; verify each target's headers and transitive requirements from the installed prefix.
- Add an installed-package conformance fixture that requires the current version, includes `audio_analysis.h`, creates an FFT/analysis call, and links only documented exported targets. This is the SDK-side closure for FRQ033.
- Audit all shared-library C ABI factories for `ORP_EXPORT`; turn the existing Windows ABI-link failure into a required Windows CI gate after the exports are correct.
- Publish a support matrix with exact status: CoreAudio/Dummy support, Windows status, Linux backend status, supported compiler/library combinations, and known unavailable capabilities. Do not market planned backends as shipped.
- Produce checksummed release artifacts plus SBOM/provenance metadata; sign only with the existing release process, never ad-hoc secrets in CI.

**SDK-owned conformance**

- Maintain clean-prefix consumer fixtures for analysis/audio utilities, transport/routing, writer/input, diagnostics, and optional ShmUI-JUCE configuration. They model the documented requirements of downstream applications but do not build, modify, or pin any child-app repository.
- Downstream validation and adoption suggestions are deliberately separated into [[ORP142 Downstream Consumer Adoption Notes]]; they are neither R0 deliverables nor release gates.

**Exit gate:** a fresh machine/prefix can consume every installed-target-manifest scenario through SDK-owned fixtures; Windows ABI-link and package tests are required CI, not manual-only.
---

### Sprint R1 — Correctness closure and observable routing contracts

**Goal:** remove known false measurements, incomplete scene behavior, and inconsistent time/event semantics.

**SDK deliverables**

- Fix ORP136 RT-1 and RT-2 without introducing wall-clock nondeterminism. Give each routing snapshot a monotonically increasing `captureRevision` for deterministic ordering; retire the fabricated zero timestamp. A millisecond display value, if retained for UI provenance, is supplied by an injected control clock and excluded from serialized/render-hash identity; an optional `TimePoint` is recorded only when a caller supplies an audio-position coordinate. Meter stereo peak/RMS/true-peak/LUFS over both channels.
- Complete advertised scene capture/recall behavior. Capture/restore clip assignments through a minimal `SessionGraph` extension; wire recall's stop-playback requirement through an explicit transport dependency. A scene recall either applies the documented complete state or fails without partial mutation.
- Turn active-clip global-cap refusal into a transport event/error, and report the real active-clip count through CoreAudio diagnostics.
- Preserve FTR028's current public `maxBlockFrames()` and internal allocation-free chunking as an installed-package contract. Add explicit content tests for 2048, 2049, and 4096 frames; no offline host should need a private buffer limit or be able to mistake rejected routing for a valid silent render.
- Use the existing immutable tempo cache to populate every `TransportEvent` position consistently. Restart and seek events currently stamp `beats = 0`; derive their beat view from the same sample position and tempo snapshot used by `getCurrentPosition()`. Document the sample-canonical source of truth and block-boundary semantics; defer tempo maps and time signatures.

**Verification**

- Regression tests for asymmetric stereo signals, deterministic snapshot revision ordering and serialization/hash invariance, scene assignment + stop semantics and rollback, active-clip refusal, CoreAudio count publication, and 2048/2049/4096-frame routing content. Test non-120 BPM and control-thread tempo changes at block boundaries for both queried positions and restart/seek event stamps; render hashes must remain deterministic.
- SDK-owned routing conformance verifies 4096-frame output content against the installed package, proving that a caller cannot receive a valid-looking silent result from the documented large-block path.
- Debug ASan/UBSan, TSan transport/routing suites, realtime audit, and deterministic render hashes remain green.

**Exit gate:** all ORP136 items RT-1, RT-2, TC-1, and CA-1 are closed; scene functionality is end-to-end complete; FTR027/FTR028 time and routing contracts are protected by package-mode regressions.

---

### Sprint R2 — Verified media and recoverable sessions

**Goal:** ensure a saved session can identify its media and report integrity failure safely.

**SDK deliverables**

- Replace the reader's hashing TODO with streaming SHA-256 through a pinned, audited provider; do not write custom cryptography. Hashing and fingerprint comparison occur only during control/background preparation, never in a callback. Validate standard test vectors and file-size/chunking invariance.
- Add a versioned `MediaFingerprint` to session media references: algorithm, digest, byte length, and optional immutable capture metadata. Relative path remains a locator, never the integrity identity.
- Add media resolution states (`Verified`, `Missing`, `Changed`, `Unreadable`, `Unsupported`) with diagnostic context. Playback preparation must refuse a required verified source whose bytes do not match; policy for optional/unverified legacy media is explicit and backwards-compatible.
- Add atomic session save and crash-recovery load helpers: serialize + validate, write a sibling temporary file, flush/replace, and preserve the prior valid document on failure. Durability policy remains host-configurable; no callback path performs filesystem work.
- Add schema migration tests for legacy sessions without fingerprints and golden JSON round trips. Include a deterministic package of media fixtures represented as text-safe test data.

**SDK boundary**

- Export `MediaFingerprint`, resolution-state, atomic-save, and migration semantics through installed headers and SDK-owned clean-prefix fixtures.
- Package documentation names the generic integration boundary, but presentation, persistence, capture, import, export, and adoption work are outside this plan; see [[ORP142 Downstream Consumer Adoption Notes]] for non-binding suggestions.

**Exit gate:** changed media, deleted media, corrupt session write, and legacy session migration are all reproducible and have deterministic outcomes.

---

### Sprint R3 — Platform truth: Windows release readiness, then Linux capability design

**Goal:** make portability a demonstrated product property rather than a roadmap label.

**SDK deliverables**

- Finish Windows DLL export coverage, installed-package ABI conformance, and real-device WASAPI capability reporting. The driver must distinguish shared and exclusive modes, expose actual sample-rate/buffer/channel capabilities, and follow the same stop/reconfigure/callback-lifetime rules as CoreAudio.
- Promote Windows Debug/Release build, test, ABI-link, and package conformance to required CI only after the preceding gate is green.
- Publish a Linux backend decision record. ALSA, JACK, and PipeWire are distinct targets with separately reported capability limits; no generic "Linux driver" abstraction hides latency, routing, or device-change behavior.
- Implement the first Linux backend only when its SDK conformance harness is complete and its support tier is approved. Run it through the same underrun, device-change, realtime, and long-session acceptance suite used for CoreAudio/WASAPI.

**Exit gate:** Windows is a supported release tier with a real-device acceptance record; Linux support status is explicit and independently testable.

---

### Sprint R4 — Shared workflow primitives and UI integration contract

**Goal:** complete functions that reduce app-local duplication while keeping core narrow.

**SDK deliverables**

- Establish a `SessionGraph` change/transaction contract sufficient for scene assignment restore and app undo/redo integration. Stable IDs and `TimePoint`/`TimeRange` remain the canonical references; raw pointers never cross persistence or cross-thread boundaries.
- Add a versioned ShmUI-JUCE synchronization manifest: upstream source revision, exported component list, required JUCE modules, optional OpenGL feature flag, and token-contract version. The SDK import update changes the manifest and a content hash; Orpheus CI validates the imported package against that local manifest and builds a minimal `add_subdirectory` consumer without OpenGL. External source-revision confirmation is non-binding guidance in [[ORP142 Downstream Consumer Adoption Notes]].
- Expose SDK realtime telemetry as fixed-capacity, decimated snapshots for UI/message threads. Do not move UI analyzers into core; provide only the transport/routing/diagnostic bridge that prevents callback-side work and duplication.
- Do not add an `AnalysisSource` or UI-snapshot view model in this sprint; those remain application state. Core exposes only generic fixed-capacity telemetry with explicit thread and lifetime contracts.

**SDK boundary**

- The SDK owns the ShmUI-JUCE import manifest, optional-feature gates, telemetry API, installed headers, and conformance fixtures.
- Component choice, design-token consumption, scene presentation, and application-specific analyzer state remain outside this repository.

**Exit gate:** SDK consumers have a repeatable, feature-gated ShmUI-JUCE package and public telemetry API; no private SDK header is required.

---

### Sprint R5 — Consumer-backed musical control primitives

**Goal:** add the smallest reusable control feature with proven adoption demand.

**Entry criteria:** two independent, existing consumer requirements substantiate a host-neutral contract; R1–R4 have shipped and soaked in at least one release. No consumer migration is part of this sprint.

**Gate review (2026-07-14):** not met. [[ORP142 Downstream Consumer Adoption
Notes]] records only FourTrack's conditional interest and explicitly creates no
SDK requirement. No second independent consumer requirement is documented, and
R1–R4 have not shipped and soaked together. Per the exit rule below, do not add
the voice primitive, policy surface, tests, or migration API in this checkpoint.

**SDK deliverables**

- Specify and implement a standalone preloaded-PCM one-shot voice utility: control-thread sample load/preallocation; audio-thread sample-offset trigger; fixed voice pool; explicit retrigger/steal policy; no allocation, lock, or I/O in `trigger`/`render`.
- Keep musical scheduling, time signatures, tempo maps, content generation, and host-specific interaction rules outside the primitive. The SDK provides mechanical bounded voice allocation and mixing only.
- If adoption evidence instead favors automation, replace this sprint with ORP135 B1: sample-accurate automation lanes evaluated deterministically at block boundaries. Do not start plugin processors before automation and graph contracts are stable.

**Verification and exit gate**

- Exact sample-offset, retrigger, steal-order, channel, render-hash, zero-allocation, and bounded-polyphony tests run in the SDK fixture suite.
- Publish the API and SDK migration guidance. Any consumer adoption is separately scoped outside this repository; see [[ORP142 Downstream Consumer Adoption Notes]]. Absent a second independent requirement, do not add the feature.

---

## 5. Release and adoption operating model

**Execution status:** deferred as a unit to a later session. The following jobs
are recorded, remain incomplete, and are not prerequisites for closing the
current R4 implementation checkpoint:

- Enforce the CMake single-source versioning policy.
- Run installed ABI compatibility and migration checks.
- Model consumer needs only in SDK-owned fixtures.
- Preserve the realtime release-blocker suite.
- Maintain a hardware-backed long-running soak scenario.
- Gate release supply-chain metadata and dependency audits.
- Update public contract documentation atomically with API changes.
- Verify overall installation and truthfulness criteria.

| Practice | Required rule |
| --- | --- |
| Versioning | CMake version is the sole source; generated docs/package metadata consume it. Public C ABI and C++ ABI compatibility are reported separately. |
| Compatibility | Every release runs installed-header compile/link tests, ABI diff checks for exported C ABI, and a migration note for changed contracts. `AnyNewerVersion` package compatibility remains only if the ABI policy proves it safe. |
| Consumer evidence | SDK-owned fixtures model downstream requirements against an installed candidate package. Downstream adoption is non-blocking and documented separately in [[ORP142 Downstream Consumer Adoption Notes]]. |
| Realtime quality | Realtime static audit, allocation/I/O harness, deterministic render hashes, sanitizer suite, and TSan contract tests are release blockers for affected surfaces. |
| Long-running confidence | Maintain a hardware-backed soak scenario that records underruns, device transitions, media cache misses, and render hashes; publish the observed limits, not aspirational claims. |
| Supply chain | Release artifacts have checksums, SBOM/provenance, pinned dependency revisions, and dependency/audit gates that cover C++ dependencies as well as the Node tooling used by the repository. |
| Documentation | Header reference, support matrix, release notes, migration guide, examples, and package targets update in the same PR as a public contract change. |

---

## 6. Deliberately deferred work

- Plugin scan/load/hosting (VST3, AU, LV2): adapter responsibility, only after automation and processor-node contracts prove stable.
- Network audio, OSC/MIDI device drivers, control-surface transports: device/network adapters; core may later provide the MPSC control dispatcher if consumer evidence requires it.
- Spatial/ADM public API: requires channel-layout governance; extend existing downmix policy and ADM work instead of exposing an immature second model.
- WASM/mobile and cross-language facades: require a frozen package/ABI policy and a concrete consumer.
- C++20 cosmetic modernization: apply selectively where it improves a proven contract; do not trade realtime clarity for fashionable abstractions.

---

## 7. Completion criteria

This plan is successful when Orpheus can be installed through its documented targets and SDK-owned consumer fixtures without source fallback; reports truthful media, routing, device, and capacity outcomes; supports each claimed release platform through required CI and real-device evidence; preserves realtime and render determinism; and grows its API only from demonstrated cross-host demand.
