<!-- SPDX-License-Identifier: MIT -->

# ORP146 — Child-App Prospective User Research

**Document type:** Market and product-discovery research  
**Status:** Research baseline; non-binding  
**Date:** 2026-07-14  
**Scope:** Five prospective end-user types each for FreqFinder, FourTrack, and
Orpheus Clip Composer, including the workflow each product should earn the right
to serve.  
**Out of scope:** Product commitments, customer validation, pricing, feature
parity with adjacent products, and cross-repository implementation work.

---

## 1. Method and evidence boundary

This is a second-stage product-discovery record. It narrows the broad SDK-adopter
research in ORP145 to the people operating the three applications themselves.
The personas are hypotheses, not established demand. Each has a concrete
workflow, a current-fit assessment, an explicit boundary, and a proof criterion.

Research began with each product's foundational vision because that is where its
intended job and deliberate exclusions are stated. It was then reconciled with the
most recent relevant records before creating a persona:

- **FreqFinder:** the current README is the capability baseline; the product
  specification and product-market analysis supply the intended workflows and
  historical segmentation. The historical analysis is not used as evidence of
  current gaps because several listed gaps have since changed [1]–[3].
- **FourTrack:** FTR001 defines the deliberately constrained by-ear recorder;
  the current README records the M2 macOS shell and defers iOS/iPadOS; FTR010
  confirms that higher-track Multitrack is a separate, planning-only sibling,
  not a FourTrack persona [4]–[6].
- **Clip Composer:** OCC021 establishes its intended broadcast, theatre, live,
  and installation markets, while OCC164 is the capability truth source as of
  2026-07-14. Any historical cross-platform, multichannel, remote-control, or
  feature claim is subordinated to OCC164's verified/partial/absent matrix [7],
  [8].

A persona is therefore not a reason to add a feature. It is a testable statement:
**if this person cannot complete the stated workflow with the explicitly declared
product boundary, the product does not yet serve that persona.**

### 1.1 Common journey model

Each persona follows five checkpoints:

1. **Job trigger** — the operator has an audio task that existing tools make
   slow, opaque, distracting, or operationally risky.
2. **Qualification** — the operator checks the product's current platform,
   workflow, and capability boundary before relying on it.
3. **Core loop** — the smallest repeatable action that produces value.
4. **Boundary** — the job that must remain outside this product, or a capability
   that is not yet truthful to advertise.
5. **Evidence of fit** — an observable outcome that should decide continued use.

---

## 2. FreqFinder

### 2.1 Product posture

FreqFinder is a C++20 JUCE plugin and standalone application for real-time
frequency analysis, harmonic detection/visualization, MIDI frequency reference,
and a nine-oscillator synthesis bank [1]. The current product exposes Analyzer,
Generator, and Loudness workspaces; internal-generator, audio-input, and sidechain
analysis sources; gate/latch playback; LUFS/True Peak/RMS modes; and PNG snapshot
export [1].

The five personas below match the foundational product's intended EQ work, sound
design, mastering reference, education, and resonance analysis use cases [2].
They deliberately exclude an asserted “generic podcast/streaming” market: the
historical product-market analysis assessed it as low fit [3].

### 2.2 Mixing engineer — diagnose a troublesome spectral relationship

- **Job trigger:** A mix contains masking, ringing, or an unstable low-frequency
  relationship that cannot be identified confidently by ear alone.
- **Qualification:** The engineer loads FreqFinder in a supported host or opens
  the standalone application, checks the relevant input/sidechain route, and
  confirms that the analysis view is not being mistaken for an EQ or corrective
  processor.
- **Core loop:** Analyze programme material, identify the fundamental and related
  partials, compare against a generated reference where useful, then make the
  actual EQ or mix change in the host's chosen processing chain.
- **Boundary:** FreqFinder does not replace the host DAW, an equalizer, or the
  engineer's judgement. The historical analysis must not be used to claim current
  absence of sidechain or loudness support; the current README records both
  multi-source analysis and loudness views [1], [3].
- **Evidence of fit:** The engineer can explain a repeatable spectral decision,
  preserve a PNG analysis snapshot when needed, and hear the intended mix result
  after the actual correction.

### 2.3 Sound designer — build and inspect harmonic character

- **Job trigger:** A designer needs to explore the audible difference between a
  fundamental and controlled even/odd partial combinations without swapping
  between separate tone-generator and analyzer tools.
- **Qualification:** The designer confirms that the current oscillator bank,
  partial-slot behavior, MIDI interaction, and gate/latch modes are sufficient
  for the specific experiment [1].
- **Core loop:** Establish a fundamental, add or remove partials, inspect spectrum
  and waveform feedback, adjust glide/level, then capture the resulting settings
  or use the knowledge in the target sound-design tool.
- **Boundary:** FreqFinder is a harmonic exploration instrument, not a full
  multi-waveform synthesizer, sampler, patch librarian, or final sound-delivery
  pipeline. A request outside its explicit oscillator/analysis workflow is not
  evidence that the product should become a general synth.
- **Evidence of fit:** The designer can create, compare, and communicate a
  desired harmonic recipe faster and with less ambiguity than with a detached
  generator plus analyzer workflow.

### 2.4 Mastering or calibration engineer — create a bounded reference check

- **Job trigger:** A mastering room, test chain, or calibration procedure needs a
  known reference frequency plus visible level/loudness evidence.
- **Qualification:** The engineer confirms the selected meter mode, output path,
  and the distinction between a reference signal and a certified measurement
  system. The product's current loudness modes are documented, but no standards
  compliance or calibration certification is implied [1].
- **Core loop:** Generate a known tone, observe the selected level/loudness view,
  route it through the controlled local chain, document the observation, and stop
  the generator cleanly.
- **Boundary:** Room correction, measurement-microphone calibration, conformance
  reporting, and final delivery compliance require the engineer's measurement
  chain and chosen standards tools.
- **Evidence of fit:** Repeating the same local reference procedure yields a
  traceable comparison point and avoids accidentally using programme material as
  an uncontrolled reference.

### 2.5 Audio educator — make harmonic relationships audible and visible

- **Job trigger:** An instructor needs students to connect a pitch, its harmonic
  series, waveform, and spectrum without teaching through static diagrams alone.
- **Qualification:** The instructor checks that the available displays, MIDI or
  on-screen note interaction, partial controls, and accessible interaction path
  fit the lesson and the classroom audio environment [1], [2].
- **Core loop:** Select a fundamental, add one partial family at a time, let
  students see and hear the resulting relationship, then reset and repeat with a
  contrasting example.
- **Boundary:** Curriculum, assessment, notation instruction, accessibility
  accommodations, and classroom device management are education-product work,
  not DSP features.
- **Evidence of fit:** A student can predict and then identify the change made by
  an even or odd partial before the visual answer is revealed.

### 2.6 Independent producer or performer — audition a MIDI-controlled reference

- **Job trigger:** A musician or producer wants a low-latency, MIDI-addressable
  pitch/harmonic reference while writing, rehearsing, or checking an arrangement.
- **Qualification:** The user verifies their host format/platform, MIDI route,
  gate-versus-latch expectation, and avoids treating a pre-release product as a
  show-critical instrument without an application-specific rehearsal [1].
- **Core loop:** Send a MIDI note, audition the generated reference and selected
  partials, use the analysis/loudness views to understand the result, then return
  to the arrangement or performance workflow.
- **Boundary:** Set-list management, controller mapping beyond the exposed MIDI
  behavior, multitimbral performance, backing-track playback, and show
  redundancy belong to other products.
- **Evidence of fit:** Note-on/note-off and chosen gate/latch behavior remain
  predictable in a representative host session, and the tool reduces pitch or
  harmonic uncertainty rather than adding live-operation risk.

---

## 3. FourTrack

### 3.1 Product posture

FourTrack is a deliberately limited, offline-first multitrack recorder: arm one
track, record, play back, layer, bounce four tracks to one, and continue. It has
no timeline, clips, waveforms, MIDI, plugin hosting, accounts, runtime telemetry,
or runtime network calls [4]. The current M2 state is a macOS SwiftUI shell over a
headless C++20 core with deterministic record/playback/bounce/import/export;
iOS/iPadOS remain deferred [5].

The persona set below is constrained to the musician-tier FourTrack job. EightTrack
may share the ethos later; the high-channel-count Multitrack proposal is explicitly
a different product and is not counted as a FourTrack user [6].

### 3.2 Solo songwriter — capture a first complete sketch

- **Job trigger:** A songwriter needs to turn a vocal, instrument, and a few
  overdubs into a complete listening sketch without entering a visual timeline or
  DAW editing session.
- **Qualification:** The songwriter confirms current macOS availability, a usable
  mic/interface path, a session sample-rate choice, and acceptance of four tracks
  with one armed track at a time [4], [5].
- **Core loop:** Record a foundation, play it back, arm one empty track, overdub,
  balance by ear using gain/pan/meters, and bounce when another layer is needed.
- **Boundary:** Comping, clip repair, piano roll, plug-ins, collaboration, and
  cloud-account workflows are expressly outside the product [4].
- **Evidence of fit:** The songwriter reaches a stable, listenable four-track
  sketch without being diverted into screen-led editing or losing the intent of
  the original performance.

### 3.3 Solo instrumentalist — practice overdubs through listening

- **Job trigger:** A player needs a rehearsal environment that makes them respond
  to existing layers by ear rather than edit visual regions.
- **Qualification:** The player checks headphone/interface monitoring, especially
  the product's built-in mic/speaker feedback safety rule, and understands that
  only one new track can be recorded at once [4].
- **Core loop:** Play back a reference layer, monitor safely, record one response,
  listen to the sum, then decide whether to retain, replace, or bounce the result.
- **Boundary:** Automated loop regions, visual punch editing, MIDI backing,
  analysis views, and track-by-track retiming would violate FourTrack's stated
  product discipline.
- **Evidence of fit:** The player can practice and evaluate musical timing by
  listening, with no feedback incident and no accidental multi-track arm state.

### 3.4 Committed lo-fi producer — use bounce as an arrangement decision

- **Job trigger:** A producer values constraint and wants a recorded checkpoint
  that forces an arrangement/mix decision rather than infinite reversible edits.
- **Qualification:** The producer studies the bounce operation: an offline render
  sums selected tracks through gain/pan, archives prior audio, writes a bounce,
  and creates a versioned checkpoint that can be reverted [4].
- **Core loop:** Build four layers, commit a musical balance with bounce-to-one,
  free tracks for the next idea, and continue from the resulting recorded state.
- **Boundary:** FourTrack does not promise the reversibility or granular editing
  of a DAW. A user who needs unlimited alternatives or non-destructive clip edits
  should choose a different product before committing audio.
- **Evidence of fit:** The bounce preserves the intended mix, creates a recoverable
  checkpoint, and enables the next arrangement step rather than becoming data loss.

### 3.5 Rehearsal-room arranger — make a quick local reference

- **Job trigger:** A musician needs a simple, local rehearsal reference built from
  successive live takes, not a multi-input live-recording rig.
- **Qualification:** The user confirms the one-at-a-time record model and the
  availability of an appropriate local input/output device. It does not use
  FourTrack for simultaneous band capture [4], [6].
- **Core loop:** Lay a guide part, add rhythm/harmony/vocal overdubs in sequence,
  bounce a compact reference, and export the application-supported result for the
  rehearsal group through the user's chosen sharing method.
- **Boundary:** Multichannel capture, timecode, remote control, and simultaneous
  record-arm are Multitrack-class concerns, not FourTrack scope [6].
- **Evidence of fit:** The group receives a coherent rehearsal reference with a
  known session history; the operator never mistakes the app for a live multitrack
  recorder.

### 3.6 Mobile capture-minded musician — evaluate, do not assume, portability

- **Job trigger:** A songwriter wants the same simple capture/bounce discipline
  away from a studio and is attracted to the future iOS/iPadOS direction.
- **Qualification:** Today the user must stop at the platform gate: the current
  shipped shell is macOS, while iOS/iPadOS is deferred [5]. The persona is useful
  precisely because it prevents a false mobile-availability claim.
- **Core loop:** On current macOS, use the same local record/overdub/bounce loop;
  if mobile workflow is mandatory, choose another available tool until the stated
  platform ships and is verified.
- **Boundary:** No promise is made for current iOS background behavior, headset
  handling, document sync, or device-specific capture until the planned shell
  exists [4], [5].
- **Evidence of fit:** The product earns this persona only after a real iOS/iPadOS
  workflow is released and can complete a complete capture-to-export session.

---

## 4. Orpheus Clip Composer

### 4.1 Product posture

Clip Composer is a professional local soundboard/plauout application whose intended
markets are broadcast, theatre, live performance, and installations [7]. Its
current verified baseline includes grid/keyboard clip launch, stop-all/panic,
loop/trim/fades/gain, session save/load, missing-media relink, rolling backups,
MIDI Note-On foundation, centralized authorization, readiness/start admission,
durable journal/ledger, support bundles, and a tagged SDK v0.4.0 compatibility
smoke [8].

The product is actively pre-release. Current restrictions matter: physical
multichannel/CoreAudio routing is not claimed; groups and output surfaces are not
yet true buses; cue/PFL has no isolated hardware route; typed OSC, GPI, timecode,
scheduling, and redundancy are absent; and 24-hour multichannel/fault soak evidence
is still a release requirement [8]. The five personas below therefore trace the
verified local workflow rather than the historical full vision.

### 4.2 Broadcast board operator — execute local clip playout safely

- **Job trigger:** An operator needs immediate, repeatable launch of local
  jingles, beds, effects, or announcements from a grid under time pressure.
- **Qualification:** The operator validates the selected device, output readiness,
  media resolution, session state, keyboard/mouse mapping, and unconditional stop
  and panic paths before relying on the board [8].
- **Core loop:** Load a known session, trigger a prepared grid clip, observe its
  local status, use stop-all/panic when required, and preserve evidence through
  the application journal/ledger and support diagnostics.
- **Boundary:** Broadcast scheduling, traffic reconciliation, as-run integration,
  remote/follower redundancy, and physical multichannel bus assignment are not
  current end-to-end Clip Composer workflows [8].
- **Evidence of fit:** A rehearsed board scenario launches the intended local clip,
  blocks unsafe new starts truthfully, and leaves a reviewable operational record
  without blocking panic.

### 4.3 Radio production director — prepare and recover a reusable session

- **Job trigger:** A production director needs a repeatable local soundboard
  session with stable button identities, editable metadata, and recoverable media
  rather than manually rebuilding carts every shift.
- **Qualification:** The director checks session migration/load behavior, media
  path resolution, missing-media relink, backup health, and mutation lock policy
  before authorizing a session for use [8].
- **Core loop:** Import/assign local media, name/color/group clips, set verified
  metadata such as loop/trim/fade/gain, save the session, validate it, then recall
  it for a producer or operator.
- **Boundary:** Versioned show-package import/export, full media-library search,
  checksum-preflight packages, and scheduler workflows are partial or absent, not
  release-ready claims [8].
- **Evidence of fit:** A saved session restores its grid identity and either
  resolves or explicitly flags each missing asset; no partial session mutation is
  presented as successful recovery.

### 4.4 Theatre sound operator — run a bounded cue rehearsal

- **Job trigger:** A theatre operator needs a compact local cue surface for
  rehearsing or executing prepared effects/music where correct start, stop, trim,
  and fade behavior matter.
- **Qualification:** The operator rehearses the exact macOS/device path, checks
  each cue's media/readiness, verifies panic and stop-all, and does not rely on
  absent timecode, GPI, OSC, independent cue, or physical multichannel routing
  capabilities [8].
- **Core loop:** Recall a prepared session, trigger a named grid cue by mouse or
  keyboard, use the validated clip metadata behavior, and return to a known state
  between rehearsal passes.
- **Boundary:** This is not yet a complete show-control system: persistent cue
  links, duck/exclusive groups, typed external control, show playlists, and
  separate cue hardware routes remain absent [8].
- **Evidence of fit:** The rehearsal establishes that every relied-on cue has a
  truthful local audio path and operator recovery action; unimplemented control
  integrations are not smuggled into the show plan.

### 4.5 Live-performance playback operator — trigger prepared local stems or effects

- **Job trigger:** A performer or operator needs responsive local playback of a
  defined set of clips with a clear emergency stop path and visible readiness.
- **Qualification:** The team tests the actual hardware path, measured start
  admission under expected load, stop/panic, media integrity, and sleep-prevention
  behavior. It treats current physical routing/multichannel evidence as pending
  rather than assuming a stage-grade system [8].
- **Core loop:** Prepare a page of clips, launch through the common dispatcher via
  grid/keyboard/MIDI foundation, monitor the application's local state, and stop
  safely when the performance requires it.
- **Boundary:** Automated set sequencing, controller-specific behavior beyond the
  current MIDI foundation, timecode, external synchronization, redundant machines,
  network streaming, and guaranteed multichannel routing are outside the verified
  workflow [8].
- **Evidence of fit:** A full representative set can be rehearsed without an
  unsafe start or ambiguous rejection, and the operator can describe what remains
  local/manual.

### 4.6 Installation or museum technician — maintain an unattended local playout node

- **Job trigger:** A technician needs an audio-first local node with durable
  session recovery, known media, health visibility, and support evidence for an
  exhibit or public experience.
- **Qualification:** The technician performs startup/recovery, media-resolution,
  backup, storage, power/sleep, and readiness drills on the actual installation
  machine. The team excludes distributed synchronization and external trigger
  protocols from the approved design until they are implemented [8].
- **Core loop:** Start the application into an approved session, validate readiness
  and media, execute the locally defined playback action, collect the bounded
  redacted support bundle when health changes, and restore from a verified backup
  if necessary.
- **Boundary:** Multi-instance synchronization, network show control, GPI,
  scheduled operation, 24/7 multichannel evidence, and enterprise-scale attraction
  control remain outside the current verified application [7], [8].
- **Evidence of fit:** A power/restart and missing-media drill recovers to a
  truthful state, and support evidence identifies the failure without exposing
  arbitrary paths, raw logs, or credentials.

---

## 5. Cross-app interpretation

The fifteen personas are intentionally not fifteen roadmap items.

| Application | Strongest current job | Honest gate | Do not infer |
| --- | --- | --- | --- |
| FreqFinder | Explore and explain frequency/harmonic relationships in a host or standalone session | supported host/platform, source routing, expected MIDI and meter behavior | a complete correction, calibration, or live-performance system |
| FourTrack | Make a constrained, by-ear overdub and bounce recording on macOS | one-track-at-a-time capture, local I/O, acceptance of no timeline/editing | current iOS availability, DAW workflow, or simultaneous multitrack capture |
| Clip Composer | Run prepared local clip playout with recovery, admission, and diagnostics | actual device path, media/session health, verified operator workflow | multichannel routing, external show control, scheduling, redundancy, or long-run release evidence |

The appropriate next discovery step is five qualitative interviews per application
with at least two participants outside the existing project ecosystem. The
interview must test the job trigger, present workflow, failure cost, platform/
hardware path, non-negotiable boundary, and the evidence that would justify
switching—not solicit a generic feature wish list.

---

## 6. References

[1] FreqFinder, “README,” *FreqFinder repository*, 2026. Local source:
`~/dev/freqfinder/README.md`. [Accessed: Jul. 14, 2026].

[2] FreqFinder, “FRQ001 Freqfinder Product Specification v021,” *FreqFinder
repository*, Nov. 2025. Local source:
`~/dev/freqfinder/docs/frq/FRQ001 Freqfinder Product Specification v021.md`.
[Accessed: Jul. 14, 2026].

[3] FreqFinder, “FRQ011 UX and Product-Market Fit Analysis,” *FreqFinder
repository*, Nov. 2025. Local source:
`~/dev/freqfinder/docs/frq/FRQ011 UX and Product-Market Fit Analysis.md`.
[Accessed: Jul. 14, 2026].

[4] FourTrack, “FTR001 Project Overview,” *FourTrack repository*, Jul. 2026.
Local source: `~/dev/fourtrack/docs/ftr/FTR001 Project Overview.md`. [Accessed:
Jul. 14, 2026].

[5] FourTrack, “README,” *FourTrack repository*, 2026. Local source:
`~/dev/fourtrack/README.md`. [Accessed: Jul. 14, 2026].

[6] FourTrack, “FTR010 Product Tiering and the Multitrack Recorder,” *FourTrack
repository*, Jul. 2026. Local source:
`~/dev/fourtrack/docs/ftr/FTR010 Product Tiering and the Multitrack Recorder.md`.
[Accessed: Jul. 14, 2026].

[7] Orpheus Clip Composer, “OCC021 Orpheus Clip Composer - Product Vision,”
*Clip Composer repository*, Oct. 2025. Local source:
`~/dev/clip-composer/docs/occ/OCC021 Orpheus Clip Composer - Product Vision.md`.
[Accessed: Jul. 14, 2026].

[8] Orpheus Clip Composer, “OCC164 Capability Reconciliation and Implementation
Program,” *Clip Composer repository*, Jul. 2026. Local source:
`~/dev/clip-composer/docs/occ/OCC164 Capability Reconciliation and Implementation Program.md`.
[Accessed: Jul. 14, 2026].
