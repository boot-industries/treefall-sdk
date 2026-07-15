<!-- SPDX-License-Identifier: MIT -->

# ORP148 — Game-Audio Developer Opportunity Research

**Document type:** Market and product-discovery research  
**Status:** Research baseline; non-binding  
**Date:** 2026-07-14  
**Scope:** What the Orpheus SDK can truthfully offer game developers now, and
host-neutral near/far opportunities that could make it more useful to game-audio
teams.  
**Out of scope:** A commitment to build a game engine, compete with established
audio middleware, ship console/mobile runtime support, or add any game-specific
feature to the current SDK.

---

## 1. Decision

**Games are a large opportunity area, not one customer category.** The useful
question is not “can Orpheus replace a game-audio engine?” It is which game-audio
jobs benefit from Orpheus's current deterministic C++ contracts, and which jobs
need a separate, staged product strategy before Orpheus can serve them credibly.

The present answer is narrow but credible:

> **Orpheus can already underpin desktop/offline game-audio tooling and controlled
> local audition/QA utilities. It is not currently a game runtime, engine plug-in,
> SoundBank pipeline, spatial renderer, or console/mobile audio platform.**

That position follows ORP145's existing game-audio persona. The persona explicitly
selects desktop/offline ingest, inspection, waveform/analysis, render, and asset QA
while excluding runtime middleware, game object/event systems, spatial rendering,
and console-platform integration [1]. ORP145 therefore covered game developers as
an **asset-tooling** opportunity; this document expands the research around that
boundary without silently converting it into a runtime roadmap.

The recommended strategy is a staged wedge:

1. **Now:** prove an engine-agnostic audio-asset QA and authoring-tool foundation.
2. **Near future:** improve reproducible asset preflight, derived analysis, batch
   validation, and tool-side audition only where two independent tool adopters
   demonstrate the same host-neutral need.
3. **Far future:** decide deliberately whether to serve bespoke game runtimes. Do
   not drift into Wwise/engine-middleware scope through a sequence of unrelated
   audio features.

---

## 2. Research model

### 2.1 Game-audio workflows are layered

Current game-audio products distinguish authoring/build from runtime behavior:

- Wwise imports media, associates it with authoring objects/events, and produces
  SoundBanks that a game loads; its automation surfaces cover import and bank
  generation [5], [6].
- Unreal's MetaSounds is an engine-resident DSP graph and gameplay-connected
  runtime system, with its own C++/Blueprint authoring path [7].
- Unity imports AudioClips with platform/compression/load settings and routes
  them through Audio Sources and Mixer Groups; its ambisonic workflow has explicit
  B-format/channel-order requirements [8]–[10].

Those sources establish a boundary, not a competitor feature list: a serious game
pipeline needs asset identity, build reproducibility, platform policy, and runtime
behavior, but the owner of each layer differs.

| Layer | Representative job | Appropriate Orpheus position today |
| --- | --- | --- |
| Asset ingest and QA | Read incoming media, reject bad assets, inspect duration/channels, fingerprint content | Strong, bounded fit |
| Derived asset analysis | Waveform, peak/loudness, spectrum/onset features, reports | Strong primitive fit; output schema/tool UX remain external |
| Tool-side audition/render | Preview an asset or controlled sequence, make deterministic offline output | Useful component fit; host builds the tool workflow |
| Build/package | Create engine-specific bank/bundle/import settings and localization outputs | Integrate around, not replace |
| Runtime playback | Event/state/parameter-driven playback, voice management, streaming, spatialization | Not a current offer |
| Engine/platform integration | Unity/Unreal/editor plug-ins, console/mobile/Web platform packaging | Not a current offer |

### 2.2 Evidence discipline

This document treats the external Wwise, Unreal, and Unity sources as evidence of
workflow boundaries, not proof that their features should be copied. A future SDK
feature still needs: two independent adopters with the same host-neutral problem,
a public-contract proposal, SDK-owned deterministic tests, an installed-package
consumer, and a release/device evidence plan [1], [2].

---

## 3. What Orpheus can offer game developers now

### 3.1 Asset identity and safe local resolution

`MediaReference` records a relative media path plus optional versioned SHA-256
fingerprint; resolution rejects absolute paths/parent traversal and exposes
Verified, Missing, HashMismatch, and related explicit states. A mismatch is not
silently usable [11].

**Useful now for:** asset-audit tools, build preflight, duplicate/moved-asset
checks, designer handoff validation, and reproducible bug reports.

**Not a claim of:** an asset database, source control integration, localization
manager, import UI, engine GUID mapping, or a replacement for a game project's
content/build system.

### 3.2 Decode, metadata, waveform, and offline analysis primitives

The public reader/extended reader supports WAV, AIFF, and FLAC through libsndfile;
waveform extraction is background-thread work and preserves a sample-range/channel
coordinate. The offline analysis facade supplies FFT/STFT, RMS/peak, integrated
LUFS, spectral centroid/rolloff, onset detection, and waveform peaks [12], [13].

**Useful now for:** an audio-asset inspector; batch waveform generation; a
content-review dashboard; loudness/peak outlier reporting; onset/feature
prototyping; and pre-import validation of supported local source media.

**Important limits:** this is not an engine importer. MP3/OGG writing is rejected,
the documented reader formats are limited, analysis allocates outside the realtime
path, and cross-platform analysis is bit-stable within a platform/toolchain rather
than promised bit-identical across all platforms [13], [14]. A game project must
own its import/compression/platform policy.

### 3.3 Deterministic local audition and offline composition components

The transport provides multi-clip playback, trims, fades, loops, seek/restart,
voice modes, stable session transactions, routing/metering, and a dummy driver for
testing/offline use [14]. `PreparedClipSource` moves short-clip decode/resampling
off the audio thread; `StreamingClipSource` offers bounded page-based long-file
playback, reporting an underrun/silence on a cache miss rather than blocking [15].

**Useful now for:** a desktop asset-review player; a sound designer's clip/region
preview tool; deterministic regression renders; a local audio QA harness; and an
internal tool that needs to reproduce a reported source/trim/fade issue.

**Important limits:** the transport is a local playback component, not a game
engine integration. It does not supply game-object binding, event/state/parameter
models, virtual voices, stream-asset memory budgeting, spatialization/occlusion,
engine frame synchronization, or console/mobile deployment. A streaming cache miss
is a truthful bounded failure policy, not an in-game streaming guarantee [15].

### 3.4 Reproducible sessions, recovery, and diagnostics

Stable-ID `SessionGraph` transactions, pointer-free snapshots, revisioned changes,
rollback, atomic persistence/migration, media recovery, and bounded realtime
telemetry support tool workflows that must reproduce an edit or a playback failure
outside the audio callback [14], [16].

**Useful now for:** an asset-review project file, a deterministic test fixture,
a regression reproducer attached to a bug, and a tool-side “what changed?” record.

**Not a claim of:** a replacement for Wwise/Unity/Unreal project formats,
source-control merge behavior, live game telemetry, profiling instrumentation,
or a production content-management service.

### 3.5 Immediate fit matrix

| Prospective game-audio customer | Can use Orpheus now? | Honest initial product |
| --- | --- | --- |
| Audio-tools team building a desktop inspector | Yes | WAV/AIFF/FLAC inspection, fingerprints, waveforms, offline analysis, reports |
| Studio build/QA team | Yes, as a component | Deterministic supported-media preflight and regression fixtures; project owns package/import rules |
| Indie team needing a local audition utility | Yes | Standalone/internal preview tool with prepared local media and explicit device support |
| Custom-engine team seeking a runtime subsystem | Only for exploration/host-owned adaptation | Prove one desktop local-playback slice; no runtime/platform support claim |
| Unity/Unreal team needing an editor/runtime plug-in | Not yet | Use the engine/middleware's normal audio path; Orpheus may support an external tool |
| Team requiring SoundBanks, event/state systems, spatial runtime, console/mobile | No | Select/build an appropriate engine or middleware integration |

---

## 4. Near-future opportunities

These are **candidate directions**, not committed features. Each is intentionally
smaller than “game audio support” and must satisfy the shared-need gate in §7.

### N-01 — Asset capability and preflight report

**Problem.** Game tools need to state whether an asset is accepted, why it is
rejected, and what observable properties were inspected before an engine-specific
import/build stage. Current media integrity identifies content safely, but codec
support/capability reporting is distributed across entry points; ORP147 records
that gap as G-07 [2].

**Candidate SDK contribution.** A public, read-only asset preflight result over a
caller-supplied file list: decoder/encoder capability, canonical input metadata,
media fingerprint/resolution, supported limits, and stable diagnostic codes. The
SDK need not invent a game asset manifest format.

**Value.** A build tool can make deterministic, machine-readable pass/fail reports
for supported source formats, while its engine adapter maps the report into Wwise,
Unity, Unreal, or proprietary build policy.

**Must remain outside core.** Asset GUIDs, localization, compression presets,
SoundBanks, Addressables/AssetBundles, engine importers, user-facing asset browser,
and project database.

**Priority:** **Near, P2 candidate.** It is a low-risk expansion of media
truthfulness if two independent tool adopters need the same report contract.

### N-02 — Versioned derived-analysis artifacts

**Problem.** Waveform/analysis values are useful in game-audio toolchains only when
they can be cached, regenerated, and tied to the exact source media and analysis
configuration. Today Orpheus supplies primitives but no canonical derived-artifact
identity.

**Candidate SDK contribution.** A small, serializable descriptor combining media
fingerprint, analysis algorithm/version, parameters, sample rate/channel selection,
and a result hash. It should define identity and invalidation, not choose a JSON,
SQLite, DDC, or cloud storage implementation.

**Value.** Tool developers can cache waveform/feature jobs safely, detect stale
outputs after asset changes, and build deterministic QA regressions without sharing
a game engine's asset database.

**Must remain outside core.** Editor UI, job scheduler, distributed cache,
content-addressable storage provider, source control, and engine metadata.

**Priority:** **Near, P3 candidate.** Build only after N-01 or after two teams
prove duplicate cache invalidation work.

### N-03 — Public offline-render composition fixture

**Problem.** Orpheus offers dummy/offline use, transport, routing, reader, writer,
and large-block-safe routing, but not one installed example showing a complete
public-only audio render. ORP147 classifies this as fixture/recipe work before a
policy-heavy render-job API [2].

**Candidate SDK contribution.** A clean-prefix fixture that renders a known
session/media set through the dummy driver, hashes the output, and reports exact
failure states. It becomes a reference for game-tool CI as well as DAW/editor/podcast
users.

**Value.** A game tools team can test a source/trim/fade/routing change without
requiring an audio device or embedding SDK internals.

**Must remain outside core.** Build-system orchestration, cancel/progress UI,
destination naming, bank packaging, platform compression, and delivery policy.

**Priority:** **Near, P1 documentation/fixture candidate.** It has low semantic
risk and establishes a reliable adoption path before new runtime APIs.

### N-04 — Format-aware PCM bed path

**Problem.** Game/VR tools may need to inspect and preserve multichannel beds or
ambisonic source assets. Unity's documented ambisonic import, for example, requires
specific B-format WAV ordering/normalization rather than a generic “multichannel”
claim [10]. Orpheus's channel-format types represent Atmos and ambisonic layouts,
but current transport/graph execution does not provide an end-to-end supported
immersive pipeline [2], [17].

**Candidate SDK contribution.** First complete ORP147 G-04's fixed channel-aware
render topology, then unify layout/channel-map identity across source, bus, sink,
and driver. For game tools, the initial value is **validate/preserve PCM layout
identity**, not spatialize it.

**Must remain outside core.** Engine-specific ambisonic importer behavior,
head-tracking, HRTF selection, spatializer plug-ins, object metadata, Dolby
licensing/branding, and room simulation.

**Priority:** **Near research / far implementation.** Do not start from a game
feature list; require a concrete shared PCM-bed workflow and layout fixtures.

### N-05 — Bounded engine-agnostic control/event seam

**Problem.** A bespoke game runtime or tool can receive actions from gameplay,
audio preview UI, test automation, and remote developer tools. The transport is
single-producer today; ORP147 G-02 identifies a bounded multi-producer ingress as
a general local-control gap [2], [14].

**Candidate SDK contribution.** The G-02 dispatcher: protocol-neutral commands,
bounded queue/refusal behavior, explicit ordering, origin token, and message-thread
results. A game engine adapter would translate its own event system into this seam.

**Must remain outside core.** Gameplay events, states/switches, real-time parameter
curves, Blueprint/C#/ECS bindings, scripting, network replication, and authoring
UI.

**Priority:** **Near, P2 shared infrastructure candidate.** Its value is broader
than games, so a game-specific API is neither required nor desirable.

---

## 5. Far-future opportunity space

### F-01 — Bespoke-game runtime foundation

A team building a custom desktop game or simulator could eventually use Orpheus for
bounded local playback, stream sources, routing, diagnostics, and scheduled events.
To be credible as a runtime foundation, however, it would first need an explicit
realtime contract for voice budget/steal/virtualization, memory budgeting and
prefetch, event scheduling, source lifecycle, failure telemetry, and engine-thread
handoff. This is materially more than an SDK transport feature.

**Decision:** watchlist only. Enter only with two independent custom-engine
adopters, a named desktop target, performance/memory budgets, and a proof that the
host-neutral portion is not already better owned by their engine/middleware.

### F-02 — Engine adapters as separate products

A Unity, Unreal, Godot, or custom-engine adapter may be useful eventually, but it
is not an SDK-core feature. Each adapter has its own build toolchain, editor/runtime
lifecycle, asset database, supported versions, and release/support burden. The
correct shape is a separate repository/package that consumes installed Orpheus
public targets and demonstrates one constrained workflow.

**Decision:** no adapter branch until an adopter commits to ownership, version
matrix, release process, and real project smoke tests. Do not add an engine
abstraction layer in core in anticipation.

### F-03 — Spatial and immersive runtime

Spatial rendering, object metadata, occlusion, room simulation, HRTF, head tracking,
and dynamic listener/game-object binding are legitimate game-audio jobs. They are
also a separate product category from PCM-bed preservation. Orpheus should first
establish format-aware offline/transport correctness (N-04/G-05) before considering
any runtime spatial layer.

**Decision:** far watchlist. No universal spatial API, no renderer license, and no
claim of game-engine parity follows from current channel-format structs.

### F-04 — Platform/runtime expansion

Game delivery commonly includes Windows/macOS/Linux plus mobile, console, web, and
VR targets. Orpheus currently has no Linux production backend, WASAPI is pending
hardware acceptance, and mobile/WASM are outside the released core [14], [18].
Console platform work additionally needs vendor access, NDA-bound toolchains, and
platform-specific compliance.

**Decision:** qualify existing desktop paths first (ORP147 G-01). Treat every new
platform/backend as a separate adoption/evidence program; never label “game
support” as a substitute for a platform matrix.

### F-05 — Game audio middleware

Wwise's authoring-to-bank pipeline and engine integration, Unreal's engine-resident
MetaSound system, and Unity's native import/mixer/runtime layers show the breadth of
a middleware product [5]–[10]. A complete middleware would need authoring data,
engine API bindings, build/package formats, profiling, memory/voice management,
spatialization, platform ports, support, and ecosystem ownership.

**Decision:** explicitly out of scope. Orpheus may become a stronger component for
custom tools and carefully bounded runtimes, but it does not become a Wwise/engine
replacement by accident.

---

## 6. Product-positioning matrix

| Position | Customer promise | Credibility today | Recommended action |
| --- | --- | --- | --- |
| Engine-agnostic asset QA toolkit foundation | Inspect, fingerprint, analyze, and validate supported local audio assets reproducibly | High | Lead with this only after a public offline fixture and capability report exist |
| Tool-side local audition component | Deterministic preview/render with explicit local device/recovery boundaries | Medium | Prove with a desktop tool consumer and public-only package fixture |
| Custom-engine desktop runtime component | Bounded local source/routing/telemetry component adapted by the host | Low/conditional | Discovery only; do not advertise as game runtime support |
| Unity/Unreal/Wwise companion integration | External tool that writes/validates inputs consumed by the engine/middleware | Medium if integration owner exists | Separate adapter/product; no core coupling |
| Full game-audio middleware | Authoring, events, banks, spatial runtime, multi-platform support | None | Out of scope |

### 6.1 Relationship to git-av

git-av and Orpheus solve complementary layers of a game-audio workflow:

| Product | Owns | Does not own |
| --- | --- | --- |
| **Orpheus SDK** | Audio decode/analysis, local audition/render, realtime-safe playback components, routing, timing, and audio diagnostics | Asset-version graph, C2PA manifest lifecycle, source provenance, timeline merge, game-engine policy |
| **git-av** | Content-addressed asset versions, branches, C2PA provenance/audit, timeline-domain history, and JSON-RPC access to those records | Audio rendering/DSP, decoder behavior, game runtime playback, routing, voice management, spatial audio |

git-av's current content-addressable vault, C2PA sidecars/audit, timeline audio
domain, and CLI/JSON-RPC surfaces make it a credible provenance companion for
game-audio assets [19], [20]. They do **not** remove the need for Orpheus game-audio
facilities. A team still needs correct decode, inspect, analysis, audition, render,
and—in a later scope—runtime audio behavior.

The first integration, if a real adopter requires it, belongs in an **external tool
adapter**, not in either core:

1. the tool asks Orpheus to inspect/analyze/render the resolved source bytes;
2. it records Orpheus's input metadata, analysis version, and output/report as
   ordinary game-tool artifacts;
3. git-av snapshots those artifacts and records their content provenance; and
4. the game engine/middleware consumes the team's own validated import/build output.

Do not force Orpheus's SHA-256 media identity and git-av's BLAKE3 vault identity
into one hash abstraction: they answer different contracts and can coexist in an
adapter record. Do not add C2PA, a vault, branching, or provenance policy to the
audio SDK merely because game teams benefit from both products.

**Strategic conclusion:** game audio is viable as an Orpheus expansion. git-av
strengthens the surrounding toolchain, but neither delays nor substitutes for an
evidence-led Orpheus path from game-asset tooling to carefully bounded bespoke
desktop runtime facilities.

---

## 7. Admission gates and next research

No feature in §4 or §5 should be scheduled merely because games are attractive.
Before a branch, collect a short evidence packet from at least two independent game
audio/tool teams:

1. Which layer is failing: ingest/QA, analysis, preview, build, runtime, or engine
   integration?
2. Which engine/middleware/platform owns the final runtime path?
3. Which source formats, channel layouts, compression targets, and localization
   rules are mandatory?
4. What output is consumed: human report, CI artifact, Wwise import, Unity import,
   Unreal import, or a proprietary tool?
5. What must be deterministic: content fingerprint, feature result, offline PCM,
   timing, or runtime behavior?
6. What is the acceptable failure: reject build, warn asset owner, fallback media,
   silence, or crash/telemetry? The SDK must not choose this product policy.
7. What is explicitly *not* delegated to Orpheus?

**Recommended first exploration:** validate N-03 with a tiny game-audio asset QA
fixture, then interview two teams about N-01. This supplies a real public adoption
path without committing to an engine plug-in, runtime, or middleware roadmap.

---

## 8. References

[1] Orpheus SDK, “ORP145 — Prospective User Journey Research,” repository
documentation, Jul. 2026. Local source:
`docs/orp/ORP145 Prospective User Journey Research.md`. [Accessed: Jul. 14, 2026].

[2] Orpheus SDK, “ORP147 — SDK Customer-Fit Gap Register and Incremental Build
Guide,” repository documentation, Jul. 2026. Local source:
`docs/orp/ORP147 SDK Customer-Fit Gap Register and Incremental Build Guide.md`.
[Accessed: Jul. 14, 2026].

[3] Orpheus SDK, “ORP143 — Reliability and Adoption Sprint Completion and
Child-App Handoff,” repository documentation, Jul. 2026. Local source:
`docs/orp/ORP143 Reliability and Adoption Sprint Completion and Child-App Handoff.md`.
[Accessed: Jul. 14, 2026].

[4] Orpheus SDK, “ORP146 — Child-App Prospective User Research,” repository
documentation, Jul. 2026. Local source:
`docs/orp/ORP146 Child-App Prospective User Research.md`. [Accessed: Jul. 14, 2026].

[5] Audiokinetic, “Managing SoundBanks,” *Wwise Help*, 2026. [Online]. Available:
https://www.audiokinetic.com/en/public-library/2025.1.9_9197/?id=managing_soundbanks&source=Help
[Accessed: Jul. 14, 2026].

[6] Audiokinetic, “Using the Wwise Authoring API,” *Wwise SDK*, 2026. [Online].
Available: https://www.audiokinetic.com/en/library/edge/?id=waapi.html&source=SDK
[Accessed: Jul. 14, 2026].

[7] Epic Games, “MetaSounds: The Next Generation Sound Sources,” *Unreal Engine
documentation*, 2026. [Online]. Available:
https://dev.epicgames.com/documentation/en-us/unreal-engine/metasounds-the-next-generation-sound-sources-in-unreal-engine
[Accessed: Jul. 14, 2026].

[8] Unity Technologies, “Import audio files into Unity,” *Unity Manual*, 2026.
[Online]. Available: https://docs.unity3d.com/Manual/AudioFiles-import.html
[Accessed: Jul. 14, 2026].

[9] Unity Technologies, “Audio Clip Import Settings reference,” *Unity Manual*,
2026. [Online]. Available: https://docs.unity3d.com/Manual/class-AudioClip.html
[Accessed: Jul. 14, 2026].

[10] Unity Technologies, “Introduction to ambisonic audio,” *Unity Manual*, 2026.
[Online]. Available: https://docs.unity3d.com/Manual/AmbisonicAudio.html
[Accessed: Jul. 14, 2026].

[11] Orpheus SDK, “media_integrity.h,” installed public header, 2026. Local source:
`include/orpheus/media_integrity.h`. [Accessed: Jul. 14, 2026].

[12] Orpheus SDK, “audio_file_reader_extended.h,” installed public header, 2026.
Local source: `include/orpheus/audio_file_reader_extended.h`. [Accessed: Jul. 14,
2026].

[13] Orpheus SDK, “audio_analysis.h,” installed public header, 2026. Local source:
`include/orpheus/audio_analysis.h`. [Accessed: Jul. 14, 2026].

[14] Orpheus SDK, “README” and “Orpheus SDK support matrix,” repository
 documentation, 2026. Local sources: `README.md`, `docs/SUPPORT_MATRIX.md`.
[Accessed: Jul. 14, 2026].

[15] Orpheus SDK, “clip_source.h,” installed public header, 2026. Local source:
`include/orpheus/clip_source.h`. [Accessed: Jul. 14, 2026].

[16] Orpheus SDK, “session_graph.h” and “realtime_telemetry.h,” installed public
headers, 2026. Local sources: `include/orpheus/session_graph.h`,
`include/orpheus/realtime_telemetry.h`. [Accessed: Jul. 14, 2026].

[17] Orpheus SDK, “audio_graph.h,” “routing_matrix.h,” and “channel_format.h,”
installed public headers, 2026. Local sources: `include/orpheus/audio_graph.h`,
`include/orpheus/routing_matrix.h`, `include/orpheus/channel_format.h`.
[Accessed: Jul. 14, 2026].

[18] Orpheus SDK, “Orpheus SDK support matrix,” repository documentation, 2026.
Local source: `docs/SUPPORT_MATRIX.md`. [Accessed: Jul. 14, 2026].

[19] git-av, “README,” repository root, Jul. 2026. Local source:
`~/dev/git-av/README.md`. [Accessed: Jul. 14, 2026].

[20] git-av, “GAV030 — S8.1 Cross-Role Collaboration Proof,” repository
documentation, Jul. 2026. Local source:
`~/dev/git-av/docs/gav/GAV030 S8.1 Collaboration Proof Report.md`. [Accessed:
Jul. 14, 2026].
