<!-- SPDX-License-Identifier: MIT -->

# ORP145 — Prospective User Journey Research

**Document type:** Market and product-discovery research  
**Status:** Research baseline; non-binding  
**Date:** 2026-07-14  
**Scope:** Twenty prospective *SDK-adopter* personas and the end-to-end journeys by
which each might evaluate, integrate, operate, and renew an application built on
Orpheus.  
**Out of scope:** Product commitments, pricing, market sizing, a compatibility
promise, and any claim of affiliation with REAPER or another host.

---

## 1. Decision and boundary

This document retains all twenty personas. The segments overlap at the level of
shared audio primitives, but the buyer, operational environment, failure cost,
and application boundary differ enough that reducing them to sixteen would hide
important adoption conditions.

The “user” in this document is the person or team that selects and integrates the
SDK, not the listener, performer, or venue guest ultimately served by their
application. A journey records a plausible workflow, not evidence of current
customer demand. Market-workflow statements are grounded in the sources in §6;
the Orpheus fit and exclusions are repository-specific assessments.

Orpheus is an independent, host-neutral C++20 SDK. Its primitives have historical
fork ancestry from REAPER, but the projects are not officially related. This
document therefore neither treats REAPER as a supported target nor reserves a
persona for it.

### 1.1 Current product boundary

The common SDK foundation available to these personas is deterministic sessions
and transport, sample-accurate multi-clip playback, audio-file utilities, routing,
realtime telemetry/diagnostics, and stable session transactions. CoreAudio and the
dummy driver are supported. Linux has no production device backend; WASAPI lacks
the required real-device acceptance record; ASIO needs a separately supplied vendor
SDK. Plugin hosting, network audio, mobile/WASM, and device-specific control drivers
are outside the released core [1], [2].

Consequently, each journey names an **application-owned layer**: UI, authorization,
external show-control protocol, business workflow, hardware driver, content policy,
or delivery integration. The SDK is a foundation beneath that layer—not a complete
application or an operational service.

### 1.2 Journey model

Every journey uses the same decision sequence:

1. **Trigger** — an operational or product gap creates a build/buy decision.
2. **Qualification** — the adopter checks timing, device, package, and licensing
   constraints against a representative workflow.
3. **Integration** — the adopter maps its content model and control surface to the
   public SDK contracts, then keeps domain policy at the application boundary.
4. **Operation** — operators or downstream users exercise the resulting product
   under normal and failure conditions.
5. **Proof / renewal** — the adopter decides whether the integration remains based
   on measurable application-level outcomes and the supported capability envelope.

The journey ends at the adopter’s proof point, rather than assuming purchase or
production deployment.

---

## 2. Research synthesis

### 2.1 Workflow evidence

Broadcast automation spans live-assist and fully automated modes, playlist/log
coordination, remote operations, and traffic-system integration [3]. Live show
systems organize media as cues, require output routing, and expose explicit
playback, stop, reset, and panic control [4]. Cart-wall workflows demand per-item
playback behavior, output selection, gain/fade treatment, overlap/restart/loop
modes, and prefer formats without decode-induced timing uncertainty [5].

DAWs and post-production tools share a timeline-based edit/mix workflow, but post
adds dialogue repair, transition smoothing, and delivery constraints [6]. Podcast
and broadcast delivery commonly separates a dynamic production master from a
loudness-adapted distribution derivative [7]. Game-audio authoring separates the
asset/authoring domain from runtime integration and platform packaging [8].

These sources establish the shape of adjacent workflows, not a claim that Orpheus
implements every cited product feature.

### 2.2 Adoption thesis

Orpheus is most credible when an adopter needs application-owned control around a
small, explicit realtime core: deterministic transport, bounded cross-thread
observability, canonical session state, audio-file access, and routing/metering.
It is less credible when the necessary differentiator is a currently excluded
platform or product layer—network transport, plugin hosting, mobile/WASM, or a
specific device-control stack. A responsible journey must therefore include a
capability gate before integration, not after deployment.

---

## 3. Persona journeys

### 3.1 Broadcast automation vendor

- **Trigger:** A product team must replace a fragile playout component while
  retaining its traffic, scheduling, remote-operation, and as-run integrations.
  This maps to the live-assist/automation split documented in broadcast automation
  workflows [3].
- **Qualification:** It proves deterministic playlist-to-session construction,
  scheduled transition timing, interruption/stop behavior, and callback diagnostics
  against its supported device target. It rejects the SDK for Linux-airchain
  deployment until a production Linux backend is supported.
- **Integration:** The application owns traffic reconciliation, scheduling,
  redundancy, remote control, rights, and operator UI; Orpheus owns the local
  session/transport, file access, routing, and telemetry bridge.
- **Operation:** An operator loads a log-derived session, takes items live, reacts
  to an interruption, and inspects application health based on telemetry.
- **Proof / renewal:** A controlled airchain simulation demonstrates correct
  transitions and actionable diagnostics without allocation or lock violations in
  the audio callback. Success is not merely that a playlist played once.

### 3.2 Radio station engineering team

- **Trigger:** Engineering needs a station-specific studio tool—cart playback,
  local production, or a fallback playout console—that can coexist with existing
  scheduling and studio hardware.
- **Qualification:** The team validates the exact studio device path, cart trigger
  latency, selected output routing, restart/overlap behavior, and recovery after a
  bad media reference. The device gate follows §1.1.
- **Integration:** Station automation, GPIO/control-surface drivers, network
  commands, user permissions, and content rotation remain application work.
- **Operation:** A presenter selects a cart; the application resolves media,
  applies station policy, triggers transport, meters the chosen route, and reports
  an unavailable asset before air.
- **Proof / renewal:** The team can rehearse a control-room failure path and restore
  a saved session deterministically. It does not infer generic hardware support
  from a successful dummy-driver test.

### 3.3 DAW developer

- **Trigger:** A DAW team needs portable session, transport, clip, waveform, and
  offline-render primitives without making a host-specific engine its product
  foundation.
- **Qualification:** The team maps its track/clip model to stable IDs, imports a
  representative multi-clip project, exercises seek/trim/fade/loop semantics, and
  validates package consumption in a clean consumer build.
- **Integration:** MIDI, plug-in hosting, edit UI, undo presentation, project
  format, collaboration, and device management policy remain in the DAW. Orpheus
  supplies canonical session snapshots/change sets and the realtime transport
  boundary.
- **Operation:** An editor makes an application transaction, previews the result,
  commits or rolls back, then uses message-thread telemetry for UI state.
- **Proof / renewal:** The DAW independently reproduces identical rendered output
  for a known session and confirms its application undo/redo law. No relationship
  with REAPER is implied or required.

### 3.4 Audio editor developer

- **Trigger:** A focused editor needs reliable clip trimming, fades, seek, waveform
  preprocessing, and lossless session persistence for non-destructive edits.
- **Qualification:** It tests exact IN/OUT boundary behavior, fades, waveform
  generation, media fingerprint/resolution states, and save/restore on damaged or
  moved media.
- **Integration:** The editor retains its selection model, UI gestures, annotation,
  autosave cadence, and user-facing conflict/relink workflow.
- **Operation:** A user edits a region, previews its boundary, saves a pointer-free
  snapshot, then later restores or relinks the session through application UX.
- **Proof / renewal:** Regression fixtures show that an edit does not silently move
  a sample boundary or replace an unresolved asset with the wrong media.

### 3.5 Live-performance playback software vendor

- **Trigger:** A vendor needs a local playback engine for cues, stems, click, and
  backing tracks where timing and emergency stop behavior are product-critical.
- **Qualification:** It runs simultaneous clips, group/master routing, cue seeking,
  decimated telemetry, and stop/panic-like application behavior at its intended
  buffer size on supported hardware.
- **Integration:** Set lists, performer UI, footswitch/control-surface integration,
  timecode, redundant-machine protocol, and click-content policy remain outside
  the core.
- **Operation:** A performer loads a set, fires a cue, adjusts a safe gain through
  app policy, watches application-owned status, and stops all playback on fault.
- **Proof / renewal:** A rehearsed show sequence produces expected local audio and
  leaves a diagnostic record for underrun investigation. This is not a claim of
  networked redundancy or timecode capability.

### 3.6 Theatre sound-control developer

- **Trigger:** A theatre product needs repeatable cue execution, operator
  confidence, and recoverable show state. Audio-cue systems demonstrate the
  importance of timing, levels, output patches, and explicit show-control actions
  [4].
- **Qualification:** The team validates cue-to-clip mapping, output patching,
  deterministic stop/reset behavior, and the saved show-state recovery path.
- **Integration:** Script/cue numbering, lighting/video protocol, operator
  permissions, rehearsal notes, and physical control inputs are application-owned.
- **Operation:** The operator recalls a show, executes a cue, observes application
  confirmations, handles a stop, then restores a known state between performances.
- **Proof / renewal:** A scripted technical rehearsal demonstrates that the product
  cannot confuse a cue’s identity or media resolution after a restart.

### 3.7 Touring production team

- **Trigger:** A touring engineer needs a bespoke playback tool for a fixed show
  that must be operated repeatedly under changing venue devices and time pressure.
- **Qualification:** The team runs the complete set through the actual supported
  device path, validates media integrity on the touring asset drive, and rehearses
  loss-of-media and emergency-stop procedures.
- **Integration:** Venue patch sheets, technician workflow, redundancy topology,
  timecode, control-surface mapping, and logistics remain team-owned application
  policy.
- **Operation:** Before doors, the operator validates assets and routing; during
  the show, the application presents the next action and message-thread health
  state without putting UI work in the audio callback.
- **Proof / renewal:** Repeated rehearsal runs preserve cue identities and output
  mapping. A successful laptop-only run is insufficient evidence for a tour.

### 3.8 Soundboard and cart-wall developer

- **Trigger:** A product needs rapid, repeatable one-shot playback with explicit
  restart, overlap, looping, gain, fade, and output behavior. These are the same
  decisions documented by current soundboard tooling [5].
- **Qualification:** The developer tests these per-button behaviors plus group
  routing and meter output with WAV/FLAC fixture media; it measures the application
  interaction path rather than relying on an MP3 assumption.
- **Integration:** Button layout, keyboard/remote bindings, virtual audio routing,
  stream/chat integration, permissions, and sound-library management are external.
- **Operation:** An operator presses a cart, sees the assigned state, can restart
  or overlap according to the configured policy, and can silence it decisively.
- **Proof / renewal:** Each configured mode has an application acceptance case that
  detects accidental replay, unwanted overlap, or a wrong output target.

### 3.9 DJ and remix-software developer

- **Trigger:** A developer needs dependable deck-like clip transport, loops,
  cue-like markers, gain, and media inspection in a specialized creative tool.
- **Qualification:** It validates gap-free restart/seek, loop boundaries, cue
  navigation, format handling, and multi-clip behavior at its target latency.
- **Integration:** Beat analysis, sync, library/database, controller mapping,
  streaming service access, licensing, and performance UI are product work.
- **Operation:** A performer loads local files, starts/stops and loops clips, and
  navigates markers through app commands while the UI consumes snapshots off the
  audio thread.
- **Proof / renewal:** The application verifies that cue/loop behavior is
  sample-correct for its canonical fixtures. It does not advertise streaming,
  controller, or network-audio integration through Orpheus.

### 3.10 Podcast-production platform developer

- **Trigger:** A platform needs editing, preview, waveform, render, and loudness
  support for spoken-word production; delivery policy may require separate masters.
  EBU guidance explicitly separates production and distribution choices [7].
- **Qualification:** The developer exercises reader/writer and analysis workflows,
  trim/fade boundaries, waveform preprocessing, and loudness metrics on a
  representative episode.
- **Integration:** Transcript/AI workflow, collaboration, publishing, RSS hosting,
  ad insertion, account management, and final platform-specific loudness policy
  are not SDK responsibilities.
- **Operation:** An editor prepares an episode, previews edits, validates a
  production master, then requests an application-owned delivery derivative.
- **Proof / renewal:** The product can trace a delivered file to a saved session,
  its media fingerprint, and its explicit loudness/delivery policy.

### 3.11 Post-production software vendor

- **Trigger:** A vendor needs a focused sound-editing component for dialogue, SFX,
  review, or delivery workflows, where trimming, fades, clip replacement, and
  transition smoothing are fundamental [6].
- **Qualification:** It tests project import, clip updates, sample-boundary edits,
  media recovery, render outputs, and the relevant routing/meter views.
- **Integration:** Picture conform, video synchronization, ADR/Foley workflow,
  interchange formats, review collaboration, and delivery specifications stay in
  the product layer.
- **Operation:** An editor revises a dialogue region, the application preserves
  edit intent in a transaction, and a reviewer renders/compares an approved state.
- **Proof / renewal:** A real conform regression confirms that a restored session
  maintains clip identity and edit boundaries. Video integration is not presumed.

### 3.12 Music-notation and rehearsal-app developer

- **Trigger:** A practice or rehearsal product needs dependable accompaniment,
  markers, looping, and section-level playback linked to an application score.
- **Qualification:** It validates section-to-time mapping, local asset resolution,
  marker seeking, loop boundaries, and predictable restart at rehearsal tempos.
- **Integration:** Notation layout, score import, MIDI interpretation, tempo
  semantics, pedagogy, synchronization, and multi-user rehearsal control remain
  application features.
- **Operation:** A musician chooses a score section; the app maps it to a stable
  session region, starts/loops it, and presents local progress through
  message-thread state.
- **Proof / renewal:** Section recalls consistently select the intended media and
  time range after save/restore; the product does not imply a notation engine in
  Orpheus.

### 3.13 Game-audio tool developer

- **Trigger:** A team wants desktop/offline utilities for ingest, inspection,
  waveform/analysis, render, or asset QA, not a replacement for its runtime audio
  middleware. Game workflows deliberately separate authoring from runtime
  integration and platform packaging [8].
- **Qualification:** It validates batch-safe file decoding, analysis/feature
  extraction, waveform preparation, canonical metadata, and offline output for a
  representative asset set.
- **Integration:** Engine runtime APIs, SoundBanks, event/state systems, spatial
  rendering, game-object lifecycle, and console platform integration remain in the
  game stack.
- **Operation:** A sound designer imports media into the application, reviews
  analysis, fixes metadata, and exports a validated asset/report for the existing
  game pipeline.
- **Proof / renewal:** The tool produces reproducible offline artifacts without
  asserting that Orpheus ships an in-game engine or game middleware bridge.

### 3.14 Interactive-installation studio

- **Trigger:** A studio needs local, unattended, repeatable playback/control for a
  gallery or public experience, often coupled to external sensors or media systems.
- **Qualification:** It validates startup recovery, local media integrity, fixed
  routing, session recall, long-running diagnostics, and the actual supported
  device path under the installation’s power/restart plan.
- **Integration:** Sensors, exhibit logic, video/lighting integration, remote
  support, kiosk UX, networking, and venue operations remain application/system
  work.
- **Operation:** The installation boots into a known app state, resolves approved
  media, responds to an external app event, and exposes health to a maintainer.
- **Proof / renewal:** A power-cycle and media-failure drill returns to a truthful
  state without silently substituting media. Network show control is excluded.

### 3.15 Location-based-entertainment integrator

- **Trigger:** An integrator needs deterministic local playback components for an
  attraction or themed venue with multiple zones and an external show-control
  system. Commercial show-control products describe this combined workflow [4].
- **Qualification:** It checks each local node’s session, output routing, cue
  recovery, diagnostics, and supported device driver; it explicitly models any
  cross-node synchronization outside Orpheus.
- **Integration:** Attraction timing, distributed synchronization, safety systems,
  network protocol, immersion format, and operations dashboards are integrator
  responsibilities.
- **Operation:** The show controller requests an application action; the local app
  executes its prepared session and reports local outcome/health.
- **Proof / renewal:** Integration tests distinguish local deterministic playback
  from system-wide synchronization, preventing a false network-audio claim.

### 3.16 AV systems integrator

- **Trigger:** An integrator needs a custom playback/control application for a
  campus, enterprise, public-space, or venue deployment with defined local audio
  behavior and supportability.
- **Qualification:** It verifies the required operating system, selected audio
  device, channel layout, routing, hot-swap behavior where supported, recovery, and
  installed package use before committing the project.
- **Integration:** Control processors, network management, user roles, room logic,
  dashboards, and client-specific support obligations are out of core.
- **Operation:** A site operator selects a scene or content source in the app; the
  app applies its policy and exposes a plain-language health state derived from
  telemetry.
- **Proof / renewal:** Site acceptance covers the device and routes actually sold,
  not an inferred cross-platform compatibility promise.

### 3.17 Embedded audio-device manufacturer

- **Trigger:** A manufacturer needs a deterministic C++ audio subsystem for a
  purpose-built appliance with local playback, routing, meters, and service
  diagnostics.
- **Qualification:** It first checks toolchain/architecture support, memory and
  callback constraints, its audio-driver boundary, and required hardware
  acceptance. It stops if the target is an unsupported embedded OS/driver.
- **Integration:** Board support, firmware update, codec/DSP drivers, provisioning,
  front-panel control, remote management, and manufacturing test belong to the
  device product.
- **Operation:** The appliance loads a validated configuration, plays local media,
  reports bounded health data, and fails truthfully when an asset or device is
  unavailable.
- **Proof / renewal:** Bench testing proves the complete device, including its
  manufacturer-owned driver. Core compilation alone cannot establish a shipped
  hardware capability.

### 3.18 Audio-interface and DSP-control vendor

- **Trigger:** A vendor needs a desktop companion or test host that presents
  routing, transport, device information, meters, and diagnostics around its own
  audio hardware.
- **Qualification:** It evaluates the stable routing/diagnostic targets and tests
  capability discovery rather than assuming a driver from OS name [2].
- **Integration:** USB/network protocol, firmware control, DSP parameter model,
  calibration, authentication, and native control-panel UI stay vendor-owned.
- **Operation:** A support engineer uses the application to select a valid device
  path, exercise an audio scenario, inspect meters/diagnostics, and export a
  vendor-defined support record.
- **Proof / renewal:** The product clearly separates SDK transport telemetry from
  proprietary device telemetry and does not misrepresent WASAPI/ASIO support.

### 3.19 Music-technology startup

- **Trigger:** A small team needs a credible C++ foundation for a differentiated
  desktop audio product without committing early to an all-in-one engine.
- **Qualification:** It creates a thin vertical slice: clean package consumption,
  one session, a controlled transport action, media handling, and a measured
  realtime check on its declared platform.
- **Integration:** Product differentiation—workflow, UI, commerce, collaboration,
  cloud, AI, controller support, and user research—remains the startup’s work.
- **Operation:** The team releases only the capability it has verified, collects
  application feedback, and treats unsupported-platform requests as roadmap input,
  not implicit SDK scope.
- **Proof / renewal:** A reproducible consumer fixture and a real product smoke path
  protect the team from source-tree coupling and premature claims.

### 3.20 Independent professional-audio developer

- **Trigger:** An individual developer needs reusable foundations for a narrow
  professional tool: a waveform browser, clip organizer, batch renderer, loudness
  utility, or rehearsal player.
- **Qualification:** The developer selects the smallest installed target set and
  builds a representative fixture with public headers only.
- **Integration:** The app owns its UX, document model, licensing, presets,
  portability decisions, and any service integration; it does not import internal
  headers or copy an application architecture.
- **Operation:** The developer ships a constrained local workflow, records clear
  capability limits, and consumes diagnostics outside the callback thread.
- **Proof / renewal:** A clean-prefix build plus a user-visible smoke scenario
  verifies the exact supported contract used by the tool.

---

## 4. Cross-persona adoption requirements

### 4.1 Common entry gate

Before a persona invests in product integration, it should prove all of the
following in its own repository:

1. The needed installed targets resolve through `find_package`, with public headers
   only.
2. Its declared OS, architecture, and device driver are within the support matrix.
3. Its application has a representative session/media fixture and a testable
   failure path for missing or changed media.
4. UI, persistence, policy, analysis history, and external integration run outside
   the audio callback; only the bounded telemetry bridge crosses toward the
   message-thread layer.
5. The team has a real smoke workflow and a product-level evidence record for its
   timing, routing, recovery, and output claims.

### 4.2 Product opportunities, not current commitments

The journeys reveal three non-binding opportunity clusters:

| Cluster | Personas | Shared need | Boundary that must remain explicit |
| --- | --- | --- | --- |
| Reliable local playout | 1, 2, 5–9, 14–16 | deterministic clip transport, local routing, diagnostics, recoverable sessions | scheduling, show control, redundancy, device/network protocol |
| Editorial and asset tools | 3, 4, 10–13, 20 | session transactions, file utilities, waveform/analysis, offline rendering | UI, format interchange, publishing, runtime/game integration |
| Product and hardware platforms | 17–19 | reusable C++ contracts, capability discovery, telemetry | device driver, firmware, DSP protocol, product workflow, commercial layer |

No cluster justifies flattening application-specific policy into the SDK. A future
SDK roadmap item requires independent adopter evidence, a public-contract proposal,
SDK-owned fixtures, and a release-gate plan.

### 4.3 Disqualifiers and honest handoffs

An adopter should select another component, delay the integration, or supply its
own adapter when it requires an unsupported production backend, plugin hosting,
network audio, WASM/mobile delivery, device-specific control, or a complete
broadcast/show-control/DAW product. The correct response is a documented boundary,
not an implied workaround or a false support claim.

---

## 5. Recommended next research

This document is directional research, not validation. Before allocating roadmap
work, conduct separate discovery with at least two independent prospective adopters
per opportunity cluster. For each interview, collect: current workflow, failure
cost, required platform/device, content/session model, integration surface, current
workaround, proof criterion, and what they would *not* delegate to an SDK. Do not
infer demand from historical forks, repository-adjacent applications, or competitor
feature lists.

---

## 6. References

[1] Orpheus SDK, “README,” repository root, 2026. Local source:
`README.md`. [Accessed: Jul. 14, 2026].

[2] Orpheus SDK, “Orpheus SDK support matrix,” repository documentation, 2026.
Local source: `docs/SUPPORT_MATRIX.md`. [Accessed: Jul. 14, 2026].

[3] WideOrbit, “WO Automation for Radio Version 5.0,” 2025. [Online]. Available:
https://www.wideorbit.com/press/wo-automation-for-radio-version-5/. [Accessed:
Jul. 14, 2026].

[4] Figure 53, “QLab 5 Manual: Audio Cues,” 2026. [Online]. Available:
https://qlab.app/docs/v5/audio/audio-cues/. [Accessed: Jul. 14, 2026].

[5] Elgato, “Stream Deck — Soundboard,” 2026. [Online]. Available:
https://help.elgato.com/hc/en-us/articles/360059017631-Elgato-Stream-Deck-Soundboard.
[Accessed: Jul. 14, 2026].

[6] iZotope, “Audio post production workflow 101,” 2022. [Online]. Available:
https://www.izotope.com/community/blog/audio-post-production-workflow-101.
[Accessed: Jul. 14, 2026].

[7] European Broadcasting Union, “Loudness in streaming,” *EBU R 128 s2*, 2020.
[Online]. Available:
https://tech.ebu.ch/files/live/sites/tech/files/shared/r/r128s2v1_0.pdf.
[Accessed: Jul. 14, 2026].

[8] Audiokinetic, “Integrating audio in your game,” *Wwise Fundamentals*, 2026.
[Online]. Available:
https://www.audiokinetic.com/en/public-library/2025.1.3_9039/?id=integrating_audio_in_game&source=WwiseFundamentalApproach.
[Accessed: Jul. 14, 2026].
