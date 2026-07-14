<!-- SPDX-License-Identifier: MIT -->

# ORP143 — Reliability and Adoption Sprint Completion and Child-App Handoff

**Document type:** Sprint completion record + downstream handoff  
**Status:** SDK implementation merged; evidence-gated work explicitly deferred  
**Date:** 2026-07-14  
**Parent plan:** [[ORP141 Reliability and Adoption Sprint Plan]]  
**Downstream guidance:** [[ORP142 Downstream Consumer Adoption Notes]]  
**Delivery:** GitHub PR #206, merged to `main` as `f5d703cd`; ORP141 completion checkpoint `babb4a87`

---

## 1. Executive summary

ORP141 converted the SDK's release, correctness, media, workflow, and integration
claims into installed public contracts with behavioral proof. R0, R1, R2, and R4
landed. R3's implementation landed, but Windows package/ABI and WASAPI
real-device acceptance evidence did not: repository Actions is disabled and the
repository has no self-hosted Windows audio runner. Windows/WASAPI therefore was
not promoted. R5 did not pass its two-consumer entry gate and was intentionally
not implemented.

The completed SDK work provides:

- release metadata and installed-target truth from CMake;
- clean-prefix consumers for documented package surfaces;
- deterministic routing, scene, transport-time, and capacity behavior;
- verified media identity and recoverable, versioned sessions;
- stable-ID `SessionGraph` transactions and pointer-free snapshots;
- a governed, feature-gated ShmUI-JUCE import;
- fixed-capacity, decimated realtime telemetry for message-thread consumers; and
- explicit boundaries that keep presentation, analyzers, musical policy, and
  child-app persistence outside the core.

No child-app source, CI, package pin, or release decision was changed in this
sprint. Each downstream team owns its adoption sprint and validation.

---

## 2. Delivery ledger

| Sprint | Result | Delivered | Evidence / disposition |
| --- | --- | --- | --- |
| R0 — Release truth | Complete | CMake-sourced version claims, installed-target manifest, clean-prefix fixtures, factory exports, support matrix, checksums, SBOM, and provenance | Installed package fixtures and release contract checks passed |
| R1 — Correctness | Complete | Routing snapshot revisions, stereo metering, atomic scene recall, voice-cap refusal events, real CoreAudio voice count, large-block routing content, canonical transport timestamps | Behavioral, sanitizer, realtime, package, and render-hash coverage passed |
| R2 — Media integrity | Complete | Provider-backed streaming SHA-256, versioned fingerprints, explicit resolution states, mismatch refusal, atomic save/recovery, migrations, golden round trips | Deterministic integrity/session and clean-prefix package coverage passed |
| R3 — Platform truth | Implementation landed; evidence deferred | Public Windows exports, WASAPI shared-mode backend and capability reporting, Windows CI/package workflow, Linux provider decision and conformance gate | GitHub Actions disabled; zero self-hosted runners; no Windows/WASAPI promotion |
| R4 — Workflow contracts | Complete | `SessionGraph` transactions/snapshots, ShmUI manifest and non-OpenGL consumer, bounded realtime telemetry, installed public workflow fixture | Full local suite and focused package/realtime checks passed |
| R5 — Musical control | Not started by design | Gate review only | One conditional adopter, not two; R1–R4 not yet shipped/soaked together |
| Release Operating Model | Deferred by user direction | Jobs recorded in §8 | No job represented as complete |

---

## 3. Public SDK contracts delivered

### 3.1 Release and package consumption

- CMake is the source for SDK version metadata consumed by package and release
  artifacts.
- The installed target manifest covers the supported consumer surfaces:
  `Orpheus::core`, `Orpheus::diagnostics`, `Orpheus::audio_utils`,
  `Orpheus::audio_io`, `Orpheus::audio_driver_manager`, `Orpheus::routing`, and
  `Orpheus::transport`.
- Clean-prefix fixtures compile, link, and execute public headers and documented
  targets without private source-tree headers.
- Release artifacts have checksum, SBOM, and provenance generation paths.
- `docs/SUPPORT_MATRIX.md` remains the authority for shipped platform status.

### 3.2 Routing, scenes, transport, and capacity

- Routing snapshots carry deterministic revision provenance rather than a
  fabricated clock value.
- Stereo meters observe both channels.
- Scene capture/recall includes clip assignment and routing state and avoids
  partial application on failure.
- Active-voice pool refusal is observable rather than silently accepted.
- CoreAudio reports the real active-voice count.
- Routing preserves content at 2048, 2049, and 4096 frames through bounded
  internal slicing.
- Transport events derive beat views from the canonical sample position and
  cached tempo at block boundaries.
- `ISceneManager::setRoutingMatrix()` is available through the package-safe
  public scene contract.

### 3.3 Media integrity and recoverable sessions

- Media fingerprints use provider-backed streaming SHA-256 outside realtime
  callbacks.
- Media references distinguish `Verified`, `Missing`, `Changed`, `Unreadable`,
  and `Unsupported` outcomes.
- Required verified media is refused when bytes do not match its fingerprint.
- Session saving uses sibling temporary files, validation, replacement, and a
  recoverable prior document.
- Legacy sessions migrate through versioned schema handling; future schema
  incompatibility is not silently treated as ordinary corruption.
- Golden fixtures protect deterministic serialization and migration behavior.

### 3.4 Session workflow contract

`include/orpheus/session_graph.h` is now the installed public header for the
workflow contract:

- stable `SessionId`, `TrackId`, and `ClipId` edits;
- canonical `TimePoint`/`TimeRange` snapshot values;
- revisioned, coalesced `SessionGraphChangeSet` publication;
- move-only rollback-on-destruction transactions;
- nested-transaction rejection;
- ID allocator watermark restoration on rollback; and
- pointer-free snapshots plus restore for application-owned undo/redo.

Runtime transport, scene-trigger state, markers, playlists, presentation, and
application undo-stack policy remain outside the transaction snapshot.

### 3.5 ShmUI-JUCE synchronization contract

- `packages/shmui-juce/shmui-juce-import.json` records the upstream Git revision,
  exported components and targets, JUCE module requirements, token-contract
  version, optional OpenGL gate, hash algorithm, and imported-content SHA-256.
- `tools/shmui_juce_manifest.py --check` validates the complete 53-file import;
  `--sync` updates the content hash after an intentional import change.
- OpenGL is default-off behind `SHMUI_JUCE_ENABLE_OPENGL`.
- A checksum-pinned JUCE 8.0.4 fixture builds and runs a real
  `add_subdirectory` consumer of `Orpheus::shmui_juce` while asserting the
  OpenGL target and definition are absent.
- ShmUI remains the design-token and JUCE-source authority; Orpheus owns the
  governed imported copy and its package test.

### 3.6 Realtime telemetry contract

`include/orpheus/realtime_telemetry.h` provides a presentation-neutral bridge:

- 64 retained fixed-capacity SPSC snapshots;
- one realtime producer and one message-thread consumer;
- configurable callback-block decimation;
- monotonic sequence numbers and explicit dropped-snapshot count;
- drop-newest behavior when full—no blocking or unread-slot overwrite;
- canonical post-block `TimePoint`;
- callback/sample/underrun diagnostics;
- active-voice count; and
- fixed-capacity group meters plus master meter.

`ITransportController::getRealtimeTelemetry()` exposes the transport-owned
bridge. Hosts must drain it on the message thread and stop producer/consumer
threads before destroying the transport.

The SDK does not own FFTs, spectral histories, display smoothing, analyzer
selection, plugin/editor state, or UI view models. Those remain child-app state.

---

## 4. Verification record

Observed on the final implementation before merge:

- full configured CTest suite: **143/143 passed**;
- `SessionGraphTransactions.*` and related invariant coverage: **8/8 passed**;
- realtime telemetry queue/cadence/diagnostic tests: **4/4 passed**;
- live transport telemetry integration: **1/1 passed**;
- strict in-repository realtime static audit: passed;
- clean-prefix `cmake_find_package` fixture: passed;
- ShmUI-JUCE 53-file manifest validation: passed;
- real JUCE 8.0.4 non-OpenGL consumer build and smoke run: passed; and
- documentation path audit: passed.

The quality review found no blocking issue in the implemented R4 scope after the
public telemetry class was exported and the new transport virtual was appended
to the interface rather than inserted among existing vtable entries.

These results do not substitute for Windows or real-device WASAPI evidence.

---

## 5. Child-app handoff matrix

### 5.1 Clip Composer

**Available now**

- Installed transport/routing targets and package-safe scene manager access.
- `ITransportController::getRealtimeTelemetry()` for message-thread meter,
  transport, voice-count, and callback-health consumption.
- Complete scene assignment/routing recall primitives.

**Team-owned adoption work**

1. Resolve an installed SDK candidate rather than a source fallback.
2. Drain telemetry snapshots on the message thread; move any analyzer work out
   of the audio callback.
3. Build app-specific histories, FFTs, smoothing, and view models outside the
   SDK.
4. Wire `ISceneManager::setRoutingMatrix()` and validate complete recall against
   existing tab/scene behavior.
5. Exercise prepare, panic/stop, routing, scene recall, and telemetry through
   existing application tests.
6. Update the app's SDK pin only in a dedicated child-app PR.

**Do not infer**

- No Clip Composer migration occurred here.
- The SDK telemetry ring is not an analyzer framework or UI state model.

### 5.2 FreqFinder

**Available now**

- Installed `Orpheus::audio_utils` and `audio_analysis.h` facade.
- Generic telemetry and media-fingerprint contracts where they reduce duplicate
  plumbing.

**Team-owned adoption work**

1. Validate the candidate through installed `find_package` resolution.
2. Remove FRQ033's source-forcing workaround only after the installed target,
   analysis header, standalone build, and existing analysis checks pass.
3. Keep plugin parameters, editor state, analyzer selection, histories, and
   view models local.
4. Treat a shared `AnalysisSource` model as future work unless another existing
   consumer proves the same contract.

**Do not infer**

- The SDK did not add or adopt a FreqFinder-specific analyzer model.

### 5.3 FourTrack

**Available now**

- Installed writer, input, prepared/streaming source, routing, time-domain,
  media-integrity, and session workflow contracts.
- Content-preserving routing at large offline block sizes.

**Team-owned adoption work**

1. Validate the installed candidate against the existing 4096-frame content
   render and sanitizer suites.
2. Retain application bounce transaction and musical scheduling policy locally.
3. Use public session transactions only where their stable-ID edit domain fits
   the application's undo/redo boundary.
4. Keep the current local click/one-shot implementation: no SDK one-shot voice
   utility shipped in this sprint.

**Do not infer**

- FourTrack's conditional interest is one requirement, not the two-consumer
  evidence needed to start R5.

### 5.4 ShmUI

**Available now**

- A versioned Orpheus import manifest, deterministic content hash, required JUCE
  modules, token-contract version, and default-off OpenGL gate.

**Team-owned synchronization work**

1. Continue treating `~/dev/shmui` as the source authority.
2. For an intentional SDK import update, change the named source revision and
   imported files together, run `tools/shmui_juce_manifest.py --sync`, then run
   the manifest check and non-OpenGL consumer.
3. Keep component selection, token use, and visual state outside Orpheus core.
4. Confirm any OpenGL use is explicit and links `Orpheus::shmui_juce_gl` only
   where required.

### 5.5 All child-app teams

- Schedule adoption in the child repository with its own owner, tests, release
  decision, and SDK pin update.
- Validate the installed package, not only an SDK source checkout.
- Do not treat SDK merge as proof that a child app has adopted the contract.
- Report contract gaps back as observable host-neutral requirements; avoid
  upstreaming app-specific presentation or policy.
- Do not advertise Windows/WASAPI support from this sprint's implementation
  alone.

---

## 6. Defining commits and merge record

| Commit | Purpose |
| --- | --- |
| `0c926eb1` | Atomic session save, recovery, and schema handling |
| `12e4f3df` | Public Windows SDK factory exports |
| `5d8fc3d3` | WASAPI shared-mode implementation and capability reporting |
| `f16b6c5c` | Versioned session conformance golden fixtures |
| `eb119d3e` | Package-safe scene manager contract |
| `5fbae07f` | Stable-ID `SessionGraph` transactions and snapshots |
| `120cabb7` | ShmUI-JUCE import manifest integrity gate |
| `c5a1fc6c` | Real non-OpenGL ShmUI-JUCE consumer |
| `3e6daffd` | Fixed-capacity decimated realtime telemetry |
| `f5d703cd` | PR #206 merge to `main` |
| `babb4a87` | Mainline completion/deferred-evidence checkpoint |

The individual commits above are the defining milestones, not a complete list of
all documentation and checkpoint commits.

---

## 7. Platform truth and release caution

R3 code exists, but its release evidence is incomplete:

- repository Actions reports `enabled: false`;
- the user elected to keep Actions disabled during sprint completion;
- Windows run `29316779217` remained queued without job records;
- the repository reports zero self-hosted runners;
- direct dispatch of the new hardware workflow returned API `404` before that
  workflow existed on the default branch; and
- no WASAPI real-device artifact was produced.

Consequences:

- do not claim hosted Windows package/ABI conformance from this sprint;
- do not promote Windows/WASAPI in the support matrix without a later passing
  hosted run and hardware record; and
- keep unsupported/unverified capability outcomes explicit in child apps.

To close this later: enable repository Actions, run Windows Debug/Release build,
test, ABI-link, and packaging on the then-current `main`; provision a trusted
self-hosted `[Windows, x64, audio-hardware]` runner; dispatch the hardware
acceptance workflow; retain its JSON artifact; then update the support matrix.

---

## 8. Explicitly deferred work

### 8.1 R5 gate

The one-shot voice utility, musical-policy boundary, sample-offset/retrigger/
steal/channel tests, and migration guidance were not implemented. Reconsider
only after:

1. two independent existing consumers document the same host-neutral need; and
2. R1–R4 ship and soak together in at least one release.

### 8.2 Release Operating Model

The user deferred this section as a unit. The recorded jobs remain incomplete:

1. enforce CMake single-source versioning policy;
2. run installed ABI compatibility and migration checks;
3. model consumer needs only in SDK-owned fixtures;
4. preserve the realtime release-blocker suite;
5. maintain a hardware-backed long-running soak scenario;
6. gate release supply-chain metadata and dependency audits;
7. update public contract documentation atomically; and
8. verify overall installation and truthfulness criteria.

No item in this list is represented as completed by ORP141 or this report.

---

## 9. Recommended next actions

1. Child-app leads review §5 and create repository-local adoption plans only for
   contracts they intend to consume.
2. A future platform-evidence session closes §7 without changing support claims
   prematurely.
3. A second independent consumer requirement, if one emerges, is recorded before
   reopening R5.
4. The Release Operating Model receives its own session and acceptance record;
   it is not mixed into child-app adoption work.

This report is the durable SDK-side completion and handoff record. ORP141 remains
the implementation plan; ORP142 remains non-binding downstream guidance.
