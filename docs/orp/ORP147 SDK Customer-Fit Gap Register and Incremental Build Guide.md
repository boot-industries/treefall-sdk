<!-- SPDX-License-Identifier: MIT -->

# ORP147 — SDK Customer-Fit Gap Register and Incremental Build Guide

**Document type:** Product-gap register and feature-branch guide  
**Status:** Research baseline; non-binding; no feature authorized by this document  
**Date:** 2026-07-14  
**Inputs:** [[ORP145 Prospective User Journey Research]], [[ORP146 Child-App Prospective User Research]], current installed public contracts, and current support status  
**Scope:** Host-neutral Orpheus SDK gaps that prevent, constrain, or make less credible a user workflow identified in ORP145 or ORP146.  
**Out of scope:** Turning Orpheus into a DAW, broadcast-automation system, show-control system, network-audio stack, plug-in host, device-control product, or child-app UI/persistence layer.

---

## 1. Decision

Orpheus already covers a substantial local foundation: deterministic clip transport,
file I/O, session transactions/recovery, routing/metering, input/output plumbing,
offline analysis utilities, and bounded realtime telemetry. The principal customer
fit problem is not an absence of primitives everywhere. It is that several
multi-persona workflows stop at a boundary where the SDK has either:

1. no qualified production device path;
2. a descriptive or component-level primitive without the host-neutral execution
   contract needed to compose it; or
3. an intentionally excluded application policy that must **not** be absorbed
   into core.

This register separates those three cases. It is deliberately a **persistent
backlog guide**, not a promise to fill every gap. A future branch takes exactly one
entry through its stated evidence, design, fixture, package, and release gates;
it must not combine neighbouring entries merely because they touch the same app.

The initial resource-preserving order is:

1. maintain truthfulness and close already-recorded platform evidence when its
   prerequisites are available;
2. prove the existing offline composition surface through one public-only fixture,
   and repair or retire any source-tree example that contradicts that proof;
3. take one small, broadly reusable control-plane contract;
4. require independent adopter evidence before musical scheduling, topology
   execution, codec expansion, or immersive work; and
5. leave product policy and unproven demand outside the SDK.

---

## 2. Evidence model and interpretation rules

### 2.1 What counts as a customer-fit gap

A gap qualifies for this register only when all three statements hold:

- a persona in ORP145 or ORP146 cannot complete a stated qualification or core
  loop, or must build reusable audio infrastructure itself;
- the missing capability can be expressed without adopting an application's UI,
  business rules, content policy, or external protocol; and
- a bounded public contract and SDK-owned fixture can prove it.

A cited persona is evidence of a *problem shape*, not evidence that a specific API
is correct or that the persona will buy/adopt it. Feature work remains conditional
on the entry gate in §5.

### 2.2 Classification

| Class | Meaning | SDK response |
| --- | --- | --- |
| **Qualified capability gap** | A broadly reusable contract is missing or cannot currently be proved on a target platform. | Candidate for a narrowly scoped SDK branch after its gate. |
| **Execution seam gap** | The SDK has descriptive data or lower-level components, but no public end-to-end execution boundary. | Design a small seam; do not jump to a framework. |
| **Application boundary** | The need is real but contains product policy, UI, protocol, service, or domain content. | Document and keep it in the adopter/application. |
| **Evidence gap** | Implementation may exist, but package, ABI, hardware, or long-run evidence is incomplete. | Close the stated release gate; do not add features to hide it. |
| **Watchlist** | A strategically valuable capability lacks enough shared demand or has high design/validation cost. | Preserve research and re-evaluate only when the gate is met. |

### 2.3 Current SDK baseline

The installed core exposes session/transport/routing/audio-I/O/diagnostic targets;
its public contracts include stable-ID transactions, media integrity/recovery,
realtime telemetry, routing through 32 output channels, input capture rings,
writer/reader utilities, and offline analysis primitives [3]–[8]. This is the
baseline against which a gap is measured—not a feature wishlist inferred from
competitors or historical documents.

Platform status is material to every claim. Core/package surfaces are supported on
macOS, Windows, and Linux, but the only supported production device backend is
CoreAudio. WASAPI's implementation remains a release candidate pending real-device
acceptance; Linux has no shipped production backend; ASIO requires a separate vendor
SDK and is not release-supported [3].

---

## 3. Gap register

### G-01 — Qualified production device coverage

**Class:** Evidence gap now; qualified capability gap for any new backend  
**Affected personas:** ORP145 broadcast automation, radio engineering, touring,
installation, location-based entertainment, AV integration, embedded hardware, and
DSP-control vendors; ORP146 Clip Composer broadcast/live/installation operators.

**Observed gap.** A local transport and routing integration cannot become a
truthful production deployment on Windows/WASAPI until package/ABI CI and the
real-device acceptance artifact exist. Linux adopters have no production device
provider at all; an ALSA, JACK, or PipeWire implementation would be a separate
capability, not a generic “Linux backend” switch [3], [4].

**Why it matters.** These personas qualify the *actual device path*, output count,
format/buffer negotiation, callback lifecycle, underrun accounting, recovery, and
hardware behavior before they integrate. A passing dummy-driver test cannot answer
that question [1], [3].

**Incremental path.**

1. **G-01a — Windows evidence closure:** when Actions and a trusted Windows audio
   runner are available, run Debug/Release package and ABI-link gates plus the
   existing WASAPI hardware workflow. Retain the artifact and update the matrix
   only after it passes. This is not new product scope.
2. **G-01b — First Linux provider discovery:** collect two independent adopters
   that name the same provider and actual hardware/deployment path. Write a
   provider-specific public target, capability identity, conformance fixture, and
   hardware acceptance plan before implementation.
3. **G-01c — Optional ASIO path:** retain as an optional vendor-SDK integration;
   do not place the SDK or a vendor binary in release artifacts without a clear
   licensing and CI plan.

**Explicit non-goals.** Device-specific mixer/control protocols, firmware,
provisioning, remote management, and “all Linux audio” support.

**Branch readiness gate.** G-01a is already defined work but depends on external
CI/hardware prerequisites. G-01b/G-01c require named adopters, provider choice,
licensing review, and a device-backed acceptance owner.

---

### G-02 — Bounded multi-producer control ingress

**Class:** Qualified capability gap  
**Affected personas:** ORP145 broadcast/radio, live playback, theatre, touring,
soundboard, DJ, installation, location-based entertainment, and AV integration;
ORP146 Clip Composer theatre/live/installation operators.

**Observed gap.** `ITransportController` accepts mutations from exactly one control
producer. Hosts with UI, MIDI, OSC, GPIO, timecode, or remote-control sources must
serialize them themselves; the public header explicitly identifies a first-class
SDK multi-producer dispatcher as future work [6]. This is a cross-cutting systems
requirement, not an argument for shipping OSC/GPI/MIDI protocols in core.

**Customer consequence.** Each integrator is forced to build a bounded arbitration
layer before it can safely attach more than one control source. Inconsistent local
implementations risk incorrect ordering, non-reproducible trigger origin, or unsafe
concurrent command submission.

**Smallest viable branch.** Export a fixed-capacity command ingress with:

- multiple non-realtime producers and one existing transport control consumer;
- explicit deterministic ordering/tie semantics and bounded overflow/refusal;
- command type, clip ID, intended `TimePoint` or next-block delivery rule, and
  opaque host-defined origin token—not protocol payloads or UI strings;
- a message-thread-observable accepted/refused/late result stream; and
- documented lifetime, shutdown, and allocation rules.

**Required proof.** SDK fixtures must cover concurrent producers, saturation,
ordering, panic precedence, teardown, and no allocation/block in the audio
callback. A clean-prefix consumer must use the public dispatcher with two mock
producers. One downstream adoption is useful confirmation but does not define the
contract.

**Explicit non-goals.** OSC, MIDI, GPI, TCP/UDP, timecode decoding, authorization,
control-surface mapping, user policy, or remote networking.

**Priority:** **P1** — first feature candidate once a branch is authorized. It
reduces duplicated unsafe plumbing across the widest group of local-control
personas while keeping policy outside core.

---

### G-03 — Sample-addressed scheduled transport events

**Class:** Qualified capability gap, subject to musical-control gate  
**Affected personas:** ORP145 broadcast automation, live playback, theatre,
touring, DJ/remix, music-notation/rehearsal, installation, and location-based
entertainment; ORP146 FourTrack rehearsal users and Clip Composer theatre/live
operators.

**Observed gap.** The SDK has deterministic sample positions, tempo conversion,
clip start/stop/seek, cue points, and current-block transport events. It does not
publish a general scheduler for a start/stop/seek action at a future sample
position. Its shared musical helper assumes four beats per bar, and the prior R5
one-shot/musical-policy work was deliberately not started because the two-consumer
and soak gates were unmet [4], [6], [9].

**Customer consequence.** Adopters that need scheduled broadcast transitions,
rehearsal section starts, show cues, or deterministic retrigger timing must either
construct their own scheduling layer or reduce their workflow to next-callback
control. That makes a broad group of timing-sensitive personas less credible,
even though their set-list, score, traffic, and show logic must remain outside the
SDK.

**Incremental path.**

1. Write a public-contract proposal for *sample-addressed actions only*: accepted
   action kinds, clock/source-of-truth, cancellation/replacement, block-boundary
   and in-block behavior, late-event rule, capacity, and result/diagnostic model.
2. Prove it with a dummy-driver render hash and fixtures spanning exact block
   boundaries, cancellation, seek/restart interaction, overload, and recovery.
3. Add tempo-map, meter, timecode, playlist, cascade, or wall-clock adapters only
   when a separate shared requirement proves each. They are not implied by the
   scheduler.

**Explicit non-goals.** Musical notation, click content, automatic playlists,
wall-clock automation, LTC/MTC protocol parsing, external show control, and
redundant-machine synchronization.

**Priority:** **P2 candidate** — high shared value but must not bypass the existing
R5 entry gate: two independent existing consumers must document the same
host-neutral need and R1–R4 must have shipped/soaked together [4].

---

### G-04 — Executable, channel-aware render topology

**Class:** Execution seam gap  
**Affected personas:** ORP145 live playback, theatre, touring, soundboard,
installation, location-based entertainment, AV integration, and hardware vendors;
ORP146 Clip Composer broadcast/theatre/live/installation users and FourTrack
recording users.

**Observed gap.** `audio_graph.h` defines a validated, serializable vocabulary for
sources, buses, sinks, sends, taps, and layouts, but states that it is descriptive
control-thread data only; the full graph engine is an ORP135 candidate [7]. The
transport currently renders a hard-wired soundboard shape. Its graph facade uses
stereo sources/groups/master, so `GraphDescription` cannot express a public,
end-to-end Clip Composer-style source-to-Cue/Master/output topology [7]. OCC164
records the downstream consequence: application work is blocked on a public
render/event/config API before it can prove multichannel physical routing and PFL
[10].

**Customer consequence.** The SDK's routing matrix alone is insufficient for a
customer to prove that an individual source's channels, a group/cue bus, and a
physical device output remain connected through the transport render path. This
blocks honest claims around PFL, multichannel stems, output patches, and
per-output metering.

**Smallest viable branch.** Do **not** build a general plugin/DSP graph. First
publish and test a transport-render configuration that binds:

- source layout and channel ordering;
- a fixed, validated source-to-bus-to-device topology;
- named but presentation-neutral output/cue/master roles;
- per-bus and per-physical-output meter snapshots; and
- the negotiated driver layout as a preflight requirement.

It must have bounded capacity and preallocation; topology mutation remains control
plane and must not allocate or rewire on the audio thread.

**Required proof.** Eight-output dummy-driver route/content tests, then a
hardware-backed CoreAudio multichannel acceptance record before a physical-device
claim. Package consumers must not downcast into a concrete transport implementation
to obtain render configuration [10].

**Explicit non-goals.** Arbitrary user-editable graphs, plug-in nodes, spatial
rendering, UI mixer layouts, room calibration, and an application-specific Cue/PFL
policy.

**Priority:** **P3 candidate.** It has strong customer value but is a broad
realtime boundary. Begin only after G-02 has established the control plane or a
narrower public render configuration can be proven independently.

---

### G-05 — Immersive and format-aware audio path

**Class:** Strategic watchlist  
**Affected personas:** ORP145 post-production, live playback, theatre,
installation, location-based entertainment, AV integration, embedded devices, and
DSP-control vendors; future child applications, whether or not current child apps
adopt it.

**Observed gap.** The SDK has more raw capacity than the current transport facade:
the routing matrix accepts 2–32 outputs and the dummy driver accepts up to 32 I/O
channels. `channel_format.h` already represents 5.1.2, 5.1.4, 7.1.4, and
ambisonic formats. But the public graph vocabulary stops at 7.1, and the current
soundboard topology/transport is stereo. There is therefore **no supported
end-to-end Atmos or immersive pipeline**, including source layout preservation,
layout negotiation, channel-map validation, output metering, render fixtures, and
hardware acceptance [7], [8].

This reconciles two superficially conflicting observations: “32 channels” denotes
matrix/output-buffer capacity; it does not establish a customer-usable immersive
format pipeline. A 7.1.4 value object is similarly not proof that transport,
routing, driver, and verification preserve it end to end.

**Incremental path.**

1. **Format-contract branch:** unify the graph and channel-format vocabulary;
   make layout/channel-map identity explicit, serializable, and validated across
   source, bus, sink, and driver capability. Include extensible/custom layouts;
   do not name a specific vendor renderer as a requirement.
2. **Bed-render branch:** extend G-04's fixed transport topology to preserve
   defined PCM beds, starting with 5.1.4/7.1.4 only if SDK-owned fixtures can
   prove channel identity and matrix routing.
3. **Immersive delivery branch:** separately decide whether object metadata,
   ADM/BWF interchange, binaural monitoring, ambisonic transforms, or a third-party
   renderer belongs in scope. None follows automatically from bed support.

**Required proof.** Channel-impulse identity fixtures for every supported layout,
explicit downmix policy, no silent channel fold-down, an installed consumer, and
real device evidence for each physical output claim.

**Explicit non-goals.** Dolby licensing/branding, object rendering, room
calibration, spatial authoring UI, and a promise of delivery-format certification.

**Priority:** **P5 watchlist.** It strengthens the SDK strategically and should
remain visible even if no current child app requires it. No urgency is assumed;
start only after G-04 is mature and independent adopter evidence identifies a
specific PCM-bed workflow.

---

### G-06 — Clip scope / choke semantics aligned with routing

**Class:** Qualified capability gap  
**Affected personas:** ORP145 radio engineering, live playback, soundboard, DJ,
and theatre; ORP146 Clip Composer board/theatre/live users.

**Observed gap.** The transport supports global stop/panic and a host-defined
“stop others except this clip” primitive, but `stopAllInGroup()` is explicitly
unsupported because the transport has no clip-to-group mapping. The routing matrix
has groups, yet those group identities are not transport voice scopes [6], [8].

**Customer consequence.** Applications requiring a music-bed choke, effect-family
exclusive group, scoped stop, or Cue-versus-programme stop must duplicate a mapping
and coordinate it with routing state. The result is easy to make inconsistent after
session restore, scene recall, or a topology change.

**Incremental path.** First determine whether G-04's source-to-bus identifiers can
be the one stable, presentation-neutral scope identity. If so, add a bounded scope
field to a transport action and define only scoped stop/choke semantics. If not,
retain this as host policy rather than creating a second group model.

**Required proof.** Sample-accurate tests for restart/fade overlap, scope
membership changes, scene/session restore, panic precedence, and a consumer fixture
that proves routing and scope cannot diverge.

**Explicit non-goals.** Button banks, colour/labels, show-programming groups,
automatic ducking, playlist policy, or app authorization.

**Priority:** **P3 dependency candidate.** Do not take it ahead of G-04, because
an independent transport group model would duplicate and destabilize routing
identity.

---

### G-07 — Codec capability truth and format expansion discipline

**Class:** Qualified capability gap for capability reporting; format additions are
watchlist items  
**Affected personas:** ORP145 radio/soundboard/DJ/podcast/post/game/independent
utility developers; ORP146 Clip Composer production directors and installation
technicians.

**Observed gap.** Current documented reader support is WAV/AIFF/FLAC, and the
writer rejects MP3/OGG. The SDK provides media identity/recovery but no single
public codec-capability registry that every import/open/export entry point can use
for preflight and diagnostics [5], [10].

**Customer consequence.** A product can discover an unsupported asset only at an
individual entry point or reproduce format assumptions in its own UI/import policy.
That is especially risky for cart/session recovery, batch QA, or portable package
workflows, where a file must be refused truthfully rather than silently substituted.

**Smallest viable branch.** Add a read-only, installed capability registry that
reports decoder/encoder/container capabilities, channel/rate bounds, and the
reason a format is unavailable. Adopt it internally at every media entry point
before adding any new codec. It must be usable without a UI and must not imply that
all system codecs are bundled or licensed.

**Explicit non-goals.** Streaming services, transcoding UI, license management,
content rights, or an unconditional MP3/OGG/ADM commitment.

**Priority:** **P4 candidate.** The registry is low-risk truthfulness work; each
new codec requires its own legal, package, determinism, and fixture decision.

---

### G-08 — Offline workflow is a recipe, not yet a consumable contract

**Class:** Documentation/fixture gap; candidate API only with repeated evidence  
**Affected personas:** ORP145 DAW, editor, podcast, post, game-tool, startup, and
independent-tool developers; ORP146 FourTrack bounce users.

**Observed gap.** The SDK supports dummy/offline operation, large-block
content-preserving routing, writers, resampling, and offline analysis. It does not
currently advertise one installed, composable offline-render job contract that owns
input enumeration, progress, cancellation, render settings, or output policy [4],
[5], [8]. A legacy source-tree `examples/offline_renderer` does not close that gap:
it includes the absent public header `orpheus/offline_render.h` and fails to compile
when examples are enabled [12]. It is neither a clean-prefix consumer nor support
evidence.

**Decision.** Do not prematurely make that job object part of core. The missing
items are largely application policy: bounce history, destination naming, progress
UI, cancel semantics, delivery profile, and retry behavior. First retire or repair
the stale example as part of an SDK-owned clean-prefix offline-render
fixture/recipe. The replacement must show how public transport rendering, routing,
reader, and writer compose, hash a known output, and report representative
media failures. Promote a shared contract only when two adopters demonstrate the
same non-policy boundary.

**Priority:** **P1 documentation/fixture remediation**, not a P1 API branch. It
may proceed before feature branches because it tests existing public composition and
corrects an invalid support artifact; it does not authorize an `OfflineRenderJob`
API.

---

### G-09 — Multichannel capture and take management

**Class:** Application boundary with a narrow future seam  
**Affected personas:** ORP145 DAW, post, AV, and embedded-device developers;
ORP146 FourTrack users.

**Observed gap.** The SDK supplies the real-time-safe capture-to-background ring
and writer components, but explicitly leaves arming, take management, punch
in/out, latency compensation, and naming to the host [9]. FourTrack's deliberate
one-armed-track workflow is therefore compatible with the SDK rather than evidence
of an SDK defect.

**Decision.** Keep take/session policy in applications. Only consider a new SDK
seam if independent adopters demonstrate that a shared, metadata-free capture
lifecycle (start/stop, overflow, discontinuity, negotiated format) cannot be built
safely from the current public input/driver contracts. Do not upstream a recorder
workflow merely to make FourTrack less constrained.

**Priority:** **Watch only.**

---

### G-10 — Cross-platform deterministic analysis reproducibility

**Class:** Evidence gap / watchlist  
**Affected personas:** ORP145 editor, podcast, post, game tools, and independent
utilities; ORP146 FreqFinder mixer, sound designer, mastering, and educator users.

**Observed gap.** The offline analysis facade intentionally allocates outside the
realtime path and is bit-stable within a platform/toolchain. It relies on `libm`
for window/twiddle values and therefore does not make a cross-platform
bit-identical analysis claim [11]. FreqFinder's real-time analyzer histories,
smoothing, source model, editor state, and view models remain rightly owned by the
application [4].

**Decision.** Preserve the application boundary. Add cross-platform reference
fixtures and error/tolerance policy only if a consumer needs reproducible exported
analysis artifacts across supported platforms. Do not put realtime analyzer UI or
plugin state into the SDK.

**Priority:** **P5 watchlist.**

---

## 4. Needs deliberately not promoted to SDK work

These needs appear in the user research but fail §2.1's host-neutrality test. They
remain visible so future branches do not smuggle them into core under another name.

| Need | Personas that surface it | Correct owner |
| --- | --- | --- |
| Traffic, as-run reconciliation, scheduling policy, content rotation, rights | Broadcast/radio | Broadcast application/service |
| OSC, GPI, MIDI mapping, timecode protocol, network command servers | Theatre/live/installation/Clip Composer | Adapter/application layer; only G-02's protocol-neutral ingress is an SDK candidate |
| Redundancy topology, distributed synchronization, network audio, safety systems | Touring/attractions/installation | Product/system integrator |
| DAW edit UI, plug-in hosting, notation, collaboration, delivery/publishing | DAW/editor/podcast/post/notation | Application/product |
| FFT history, smoothing, spectral editor views, parameter/editor state | FreqFinder | FreqFinder/application |
| Bounce policy, take policy, punch/comping, naming, cloud sync | FourTrack/recording products | Application |
| Device DSP protocol, firmware, calibration, authentication, control-panel UI | Hardware and DSP vendors | Vendor product |
| Immersive object authoring, renderer licensing, room calibration | Immersive users | Specialist product/vendor, pending a future explicit SDK scope decision |

---

## 5. Feature-branch admission and sequencing

### 5.1 Admission packet for every gap

Before an entry becomes a feature branch, record all of the following in its branch
plan:

1. the ORP145/ORP146 persona(s) and exact failed qualification/core-loop step;
2. at least two independent adopters for any new reusable behavioural contract,
   unless the work is a pre-existing evidence gate such as G-01a;
3. a public header/API proposal with lifetime, ownership, thread, capacity,
   allocation, error, and versioning rules;
4. explicit application-owned policy and non-goals;
5. an SDK-owned deterministic unit/render fixture and failure cases;
6. a clean-prefix installed-package consumer that exercises the new contract;
7. required platform/device/hardware evidence and the truth claim it would allow;
   and
8. migration, release-note, support-matrix, and public-documentation changes.

This keeps feature branches thin, verifiable, and reversible at the boundary—not
at the audio signal.

### 5.2 Recommended order

| Order | Work | Why now / why not sooner | Entry gate |
| --- | --- | --- | --- |
| 0 | G-01a Windows/WASAPI evidence | Existing implementation cannot be promoted without proof; no new API. | CI and real-device prerequisites available. |
| 1 | G-08 offline fixture remediation | A non-compiling source-tree example is not support evidence; a public-only fixture proves the actual offline composition boundary before policy/API work. | Retire or repair the stale example; a relocated-package fixture compiles/runs, hashes known output, and reports defined media failures. |
| 1a | G-02 bounded control ingress | Broad local-control reuse; narrow protocol-neutral contract. | Two concrete host control sources and API/fixture proposal. |
| 2 | G-03 sample-addressed events | Broad timing value, but carries R5/musical-control risk. | Existing R5 gate plus event semantics proposal. |
| 3 | G-04 fixed executable topology | Enables truthful output/cue/multichannel claims; high realtime design cost. | Render-config proposal and eight-output fixture plan. |
| 3a | G-06 scoped choke | Must reuse the G-04 topology identity, not invent a parallel group model. | G-04 identity decision and scoped-action fixture. |
| 4 | G-07 codec capability registry | Improves preflight truth without promising codecs. | All entry points identified; installed capability contract. |
| 5 | G-05 immersive PCM beds | Strategic strength, no present urgency; requires topology maturity. | Specific shared PCM-bed workflow, layout fixtures, device plan. |
| 5 | G-10 cross-platform analysis artifacts | Only matters when exported cross-platform analysis needs identical/tolerant output. | Two consumers and reference-artifact decision. |

G-09 and every §4 item remain outside the sequence until new evidence changes the
boundary.

### 5.3 Branch rules

- One register entry per branch. A G-04 branch does not implement G-05 immersive
  support “while the render code is open.”
- Public API begins only after the entry packet is written; no child app may depend
  on a private implementation/downcast.
- The SDK test proves the contract. A child-app adoption test may validate an
  integration but must not be the SDK's only evidence.
- A device claim requires the actual driver/device gate. Dummy-driver coverage
  validates determinism and channel identity, not hardware support.
- Unsupported outcomes must remain explicit in capability APIs, support matrix,
  package target manifest, and release text.

---

## 6. Next research, not implementation

The next informed action is **not** to open all ten gaps. First conduct G-08
evidence remediation: prove a public-only render path from a relocated install,
with a declared same-platform/toolchain hash policy and explicit media failures. If
that reveals a missing public seam, record the exact operation; do not recreate the
obsolete offline-render API by inference. In parallel, conduct focused discovery for
G-02, G-03, G-04, G-05, and G-07 with prospective SDK adopters outside the current
project family. For each, capture the required device/platform/layout,
control-source count, latency/timing proof, current workaround, failure cost, and
what the adopter refuses to delegate.

For immersive work specifically, distinguish three requests before any feature is
proposed: (a) channel-based PCM bed transport/routing, (b) layout metadata and
interchange, and (c) object/spatial rendering. The first can strengthen a neutral
SDK; the latter two carry materially different licensing, ecosystem, and product
scope.

---

## 7. References

[1] Orpheus SDK, “ORP145 — Prospective User Journey Research,” repository
documentation, Jul. 2026. Local source:
`docs/orp/ORP145 Prospective User Journey Research.md`. [Accessed: Jul. 14, 2026].

[2] Orpheus SDK, “ORP146 — Child-App Prospective User Research,” repository
documentation, Jul. 2026. Local source:
`docs/orp/ORP146 Child-App Prospective User Research.md`. [Accessed: Jul. 14, 2026].

[3] Orpheus SDK, “Orpheus SDK support matrix,” repository documentation, 2026.
Local source: `docs/SUPPORT_MATRIX.md`. [Accessed: Jul. 14, 2026].

[4] Orpheus SDK, “ORP143 — Reliability and Adoption Sprint Completion and
Child-App Handoff,” repository documentation, Jul. 2026. Local source:
`docs/orp/ORP143 Reliability and Adoption Sprint Completion and Child-App Handoff.md`.
[Accessed: Jul. 14, 2026].

[5] Orpheus SDK, “README,” repository root, 2026. Local source: `README.md`.
[Accessed: Jul. 14, 2026].

[6] Orpheus SDK, “transport_controller.h,” installed public header, 2026. Local
source: `include/orpheus/transport_controller.h`. [Accessed: Jul. 14, 2026].

[7] Orpheus SDK, “audio_graph.h,” installed public header, 2026. Local source:
`include/orpheus/audio_graph.h`. [Accessed: Jul. 14, 2026].

[8] Orpheus SDK, “routing_matrix.h” and “channel_format.h,” installed public
headers, 2026. Local sources: `include/orpheus/routing_matrix.h`,
`include/orpheus/channel_format.h`. [Accessed: Jul. 14, 2026].

[9] Orpheus SDK, “audio_input.h” and “music_timing.h,” installed public headers,
2026. Local sources: `include/orpheus/audio_input.h`,
`include/orpheus/music_timing.h`. [Accessed: Jul. 14, 2026].

[10] Orpheus Clip Composer, “OCC164 Capability Reconciliation and Implementation
Program,” *Clip Composer repository*, Jul. 2026. Local source:
`~/dev/clip-composer/docs/occ/OCC164 Capability Reconciliation and Implementation Program.md`.
[Accessed: Jul. 14, 2026].

[11] Orpheus SDK, “audio_analysis.h,” installed public header, 2026. Local source:
`include/orpheus/audio_analysis.h`. [Accessed: Jul. 14, 2026].

[12] Orpheus SDK, “offline_renderer example,” repository source and build target,
2026. Local sources: `examples/offline_renderer/offline_renderer.cpp`,
`examples/offline_renderer/CMakeLists.txt`. [Accessed: Jul. 14, 2026].
