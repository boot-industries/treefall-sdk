<!-- SPDX-License-Identifier: MIT -->

# ORP255 Strategic Architecture, Competitive Posture, and Expansion Vectors

**Document type:** Strategic Architecture & Market Expansion Assessment  
**Status:** Approved Strategy Record  
**Date:** 2026-09-06  
**Scope:** Evaluation of Orpheus SDK (Treefall SDK) core strengths, architectural weaknesses, streaming seams, autonomous robotics, and adjacent domain expansions.  
**Related Documents:** [[ORP145 Prospective User Journey Research]], [[ORP147 SDK Customer-Fit Gap Register and Incremental Build Guide]], [[ORP148 Game-Audio Developer Opportunity Research]], [[ORP149 Aurora Control-Plane Opportunities for Game Audio and IoT]], [[ORP254 Suite Commercialization, Rebrand Strategy, and Tier Segmentation]].

---

## 1. Executive Summary

Orpheus SDK (transitioning under the commercial strategy in [[ORP254]] to **Treefall SDK** as the open-source MIT engine for **Boot Industries**) occupies an intentional, highly disciplined niche: **an open-source C++20 audio foundation for deterministic local playout, media/session integrity, and realtime diagnostics—engineered strictly to sit beneath an application’s domain workflow.**

It is **not** a DAW, a game engine audio runtime, a plugin host, or an all-in-one cross-platform multimedia framework. It deliberately rejects feature bloat, unproven network-audio claims, and hidden realtime hazards.

This record synthesizes the architectural strengths and weaknesses of the SDK, outlines its competitive posture against incumbent audio libraries, resolves the architectural ambiguity surrounding "streaming," and evaluates strategic expansion vectors—specifically autonomous robotics, safety-critical systems, and headless asset verification.

---

## 2. Core Strengths & Architectural Differentiators

```
┌────────────────────────────────────────────────────────────────────────┐
│                          ORPHEUS / TREEFALL SDK                        │
│                                                                        │
│   Deterministic Transport        Media & Session Integrity             │
│   ├── Lock-free command ring     ├── SHA-256 media fingerprinting      │
│   ├── Zero alloc/lock/IO/log     ├── Pointer-free session snapshots    │
│   └── Bounded voice allocation   └── Atomically recoverable schemas    │
│                                                                        │
│   Routing & I/O Hygiene          Realtime Observability                │
│   ├── N×M matrix + group choke   ├── Decimated telemetry bridges       │
│   ├── Direction-explicit devices ├── Bounded underrun/xrun counters    │
│   └── CoreAudio production depth └── Zero UI/message-thread leakage    │
└────────────────────────────────────────────────────────────────────────┘
```

1. **Audit-Enforced Realtime Safety:**
   - Realtime audio callbacks operate under zero-tolerance constraints: zero heap allocation, zero mutex synchronization, zero blocking filesystem/network I/O, and zero formatting/logging.
   - Enforced by continuous static AST checking (`tools/realtime_audit.py --fail-known-debt`) and OS-level I/O / memory instrumentation in test harnesses (`realtime_harness_test`).
   - Non-blocking cache model: a streaming miss emits silence and increments a bounded `BufferUnderrun` event; it never stalls the audio thread.

2. **Cryptographic Media Integrity & ACID Session Transactions:**
   - Content-addressable assets via versioned SHA-256 fingerprints rather than brittle file paths. Media states are explicit: `Verified`, `Missing`, `Changed`, `Unreadable`, or `Unsupported`.
   - The `SessionGraph` transaction model provides pointer-free snapshots, atomic rollback on failure, ID watermark tracking, and forward/backward schema migration without session corruption.

3. **Deep Production CoreAudio Resilience:**
   - Deep platform integration on macOS: directional input/output device identity, runtime sample-rate change negotiation, Bluetooth duplex transitions, sample-rate conversion (SRC) telemetry, and non-mutating route discovery.

4. **Package & Boundary Hygiene:**
   - Clean CMake modular targets (`Orpheus::core`, `Orpheus::transport`, `Orpheus::routing`, `Orpheus::audio_utils`, `Orpheus::diagnostics`), clean-prefix consumer fixtures, and a stabilized C ABI 1.0.

---

## 3. Current Weaknesses & Critical Gaps

1. **Production Platform Asymmetry:**
   - Production hardware support is effectively macOS CoreAudio only.
   - Windows/WASAPI is implemented in source and fake tests, but remains unpromoted due to a lack of physical-device hardware acceptance records and package/ABI verification (ORP147 G-01).
   - Linux currently lacks production drivers (ALSA, JACK, PipeWire), supporting only the Dummy driver.

2. **Public Offline Rendering Seam Deficit:**
   - While offline STFT, FFT, LUFS, and peak feature analysis exist, the public offline-rendering example historically lagged behind the realtime playback API (ORP147 G-08). Batch rendering and headless export require a first-class, documented CMake consumer fixture.

3. **Single-Producer Control Ingress:**
   - Transport commands flow through a single control interface. Multi-threaded application hosts (e.g., UI thread, background MIDI sequencer, OSC network thread) cannot concurrently push commands without external synchronization (ORP147 G-02).

4. **Stereo-Restricted Voice Pipeline:**
   - Although the routing matrix supports up to 32 output channels with channel-mapping and downmix policies, transport clip rendering remains tied to stereo pair buffers, preventing native multichannel surround or ambisonic bed playout (ORP147 G-04).

---

## 4. Competitive Posture

| Feature / Dimension | **Orpheus / Treefall SDK** | **JUCE (`juce_audio_devices`)** | **miniaudio** | **PortAudio** | **Wwise / FMOD** |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Primary Domain** | Deterministic local playout, asset QA, robust session state | Desktop plugins (VST/AU), monolithic GUI apps | Lightweight, single-header playback & decoding | Minimal cross-platform device abstraction | Commercial game-audio middleware & spatialization |
| **Realtime Safety Verification** | **Extreme** (Static AST audits, allocation hooks, CI gates) | **Advisory** (Developer discipline, prone to allocation bugs) | **Good** (C design, minimal runtime allocs) | **Basic** (Varies widely by host API backend) | **Proprietary** (Engine-optimized runtime) |
| **Asset / Session Integrity** | **Cryptographic** (SHA-256 fingerprints, atomic transactions) | **None** (Application responsibility) | **None** (Raw memory / file streaming only) | **None** (Raw PCM stream only) | **Proprietary SoundBanks** (Opaque packaging) |
| **Platform Breadth** | **Narrow** (macOS CoreAudio production; Win/Linux unpromoted) | **Ubiquitous** (macOS, Win, Linux, iOS, Android) | **Ubiquitous** (macOS, Win, Linux, iOS, Android, WASM) | **Broad** (macOS, Win, Linux) | **Ubiquitous** (Consoles, PC, Mobile) |
| **Plugin Hosting** | **None** (Explicit non-goal) | **Industry Standard** (VST3, AU, AAX host/client) | **None** | **None** | **Proprietary DSP FX** |
| **Licensing** | **Permissive MIT** | **GPLv3 or Costly Commercial** | **Public Domain / MIT-0** | **MIT-style** | **Revenue-share / Expensive Seat** |

**Strategic Wedge:** Orpheus avoids competing with JUCE on UI/plugins, Wwise on spatial game-event graphs, or miniaudio on single-file embeddability. Its defensible position is **mission-critical, unattended, high-integrity audio playout where an audio dropout, a corrupted session file, or an untracked asset change is an operational disaster.**

---

## 5. Architectural De-Risking: Streaming

Streaming spans three distinct architectures with differing safety and ownership boundaries:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                 THREE FACES OF STREAMING                               │
├───────────────────────────────┬───────────────────────────────┬────────────────────────┤
│  1. Disk/Asset Streaming      │  2. Web/Network HTTP Audio    │  3. Low-Latency Net I/O│
│     (Local Page Rings)        │     (HLS, DASH, WebSockets)   │     (AES67, SMPTE 2110)│
├───────────────────────────────┼───────────────────────────────┼────────────────────────┤
│  ✅ IN CORE & PROVEN          │  ⚠️ ADAPTER SEAM ONLY         │  ❌ REJECTED FROM CORE  │
│  - Bounded lock-free ring     │  - Non-realtime ingest worker │  - Needs PTP, NIC hooks│
│  - Zero audio-thread I/O      │  - Buffer-ahead into PCM ring │  - Handled via Aurora  │
│  - Silence on underrun        │  - Keep protocol out of core  │  - External orchestrator│
└───────────────────────────────┴───────────────────────────────┴────────────────────────┘
```

1. **Disk / Asset Streaming (In Core):** Fully implemented in ORP134/ORP168. `PreparedClipSource` and `StreamingClipSource` decouple filesystem reads entirely from the callback via worker-fed lock-free page rings.
2. **Network Media Ingest (HTTP/HLS/WebSockets):** Network protocol stacks (`libcurl`, WebSockets, TLS) MUST remain out of the core audio engine. The correct architectural seam is an abstract push-based `AudioByteStreamSource` or bounded ring buffer interface, fed by a host application worker.
3. **Low-Latency AoIP (AES67 / Dante / Ravenna):** Professional network audio requires PTP clock synchronization (IEEE 1588) and NIC pacing. Following ORP108, AoIP is excluded from SDK core. Facility-grade AoIP orchestration is owned externally by **Aurora** (`~/dev/nmos`, [[ORP149]]), with Orpheus serving as the local audio playback endpoint.

---

## 6. High-Leverage Expansion Vectors

```
                               ┌──────────────────────────────────────────┐
                               │           EXPANSION LANDSCAPE            │
                               └──────────────────────────────────────────┘
                                                    │
         ┌───────────────────┬──────────────────────┴──────────────┬───────────────────┐
         ▼                   ▼                                     ▼                   ▼
┌──────────────────┐ ┌─────────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐
│  Robotics & AVAS │ │ Headless CI/CD Asset QA │ │ Medical & Simulation │ │ Fixed Installations│
│  (ROS 2 nodes)   │ │ ("treefall-lint" CLI)   │ │ (IEC 62304 / TMR)    │ │ (Museums / Venues) │
└──────────────────┘ └─────────────────────────┘ └──────────────────────┘ └──────────────────────┘
```

### 6.1 Autonomous Robotics & Industrial Edge (ROS 2)
Autonomous Mobile Robots (AMRs), robotic surgical platforms, and Electric Vehicle AVAS (Acoustic Vehicle Alerting Systems) require high-reliability audio subsystems:
- **Zero-Allocation Safety:** Pre-allocated memory guarantees prevent thread starvation in robot watchdog cycles.
- **AVAS Engine Audio:** Continuous pitch-shifted, gain-smoothed looping (`ORP160`) under lock-free control.
- **Auditory Anomaly Detection:** Real-time STFT, spectral kurtosis, and onset detection on input microphone rings provide acoustic bearing and predictive maintenance telemetry (e.g., motor bearing failure).
- **Compliance Verification:** SHA-256 asset verification ensures critical safety chimes match certified safety assets.
- **Wedge:** An open-source ROS 2 lifecycle node (`treefall_ros2`) wrapping an ALSA-backed headless SDK instance.

### 6.2 The "SQLite of Audio Engines": Headless Asset QA & Linter
Game studios, post houses, and sound libraries manage petabytes of audio assets with zero automated CI/CD validation.
- Provide a standalone, lightweight CLI tool (`treefall-lint` / `treefall-inspect`) powered by SDK analysis and media integrity primitives:
  - Detect clipped peaks, DC offset, phase misalignment, and loudness violations (LUFS).
  - Verify asset fingerprints against source control manifests.
  - Perform bit-exact regression rendering against golden hashes.
- Builds immediate developer trust without requiring studios to replace runtime audio engines.

### 6.3 Medical Devices & Safety-Critical Simulation
- Systems subject to IEC 62304 software verification require static memory guarantees, verifiable state machines, and absence of undefined behaviors.
- Orpheus's static AST audit gates, deterministic transaction rollbacks, and decoupled telemetry bridges provide the compliance trail required for surgical haptic audio and flight simulation sound racks.

---

## 7. Execution Sequence & Strategic Priorities

1. **Step 1: Truthfulness & Public Render Fixture (Immediate / Low Risk)**
   - Ship a clean-prefix CMake fixture demonstrating public offline rendering (`G-08`).
   - Standardize codec capability preflight queries (`G-07`).
2. **Step 2: Concurrency & Control Seams**
   - Implement a bounded multi-producer single-consumer (MPSC) control queue (`G-02`) to unblock concurrent command ingress from network, MIDI, and UI threads.
3. **Step 3: Hardware Promotion for Windows (WASAPI)**
   - Execute the physical-device acceptance workflow on a dedicated Windows runner to graduate WASAPI from RC to Supported (`G-01a`).
4. **Step 4: Linux ALSA & Robotics Seam**
   - Implement a dedicated, minimal ALSA provider (`G-01b`) for headless Linux appliances.
   - Prototype the `treefall_ros2` node and headless asset linter CLI.

---

## 8. Sprint Decomposition Recommendations

To de-risk delivery, prevent architectural regression, and maintain strict realtime safety audit compliance, the expansion and hardening work is decomposed into four discrete, sequential sprints. Each sprint has an explicit entry gate, core deliverables, and verifiable acceptance criteria.

```
┌────────────────────────────────────────────────────────────────────────┐
│                       SPRINT SEQUENCING ROADMAP                        │
│                                                                        │
│  Sprint 1: Public Offline Seam & Codec Preflight (G-08 / G-07)         │
│  ├── Public CMake offline render fixture & documentation               │
│  ├── Bit-exact golden hash regression tests                            │
│  └── Codec preflight verification API                                  │
│                                 │                                      │
│                                 ▼                                      │
│  Sprint 2: High-Concurrency Control Ingress (G-02)                     │
│  ├── Bounded MPSC transport command ring                               │
│  ├── Lock-free multi-producer thread safety audit                      │
│  └── Tagged settlement telemetry integration                           │
│                                 │                                      │
│                                 ▼                                      │
│  Sprint 3: Platform Evidence & Windows Promotion (G-01a)               │
│  ├── Physical hardware WASAPI acceptance runner execution              │
│  ├── Windows package/ABI verification gate closure                     │
│  └── Promotion of WASAPI to Supported status in SUPPORT_MATRIX.md       │
│                                 │                                      │
│                                 ▼                                      │
│  Sprint 4: Headless Linux & Edge Robotics Wedge (G-01b & ROS 2)        │
│  ├── Dedicated minimal ALSA device provider (no generic Linux claims)  │
│  ├── Headless CLI asset verification & lint utility (`treefall-lint`)  │
│  └── Prototype ROS 2 lifecycle audio node (`treefall_ros2`)            │
└────────────────────────────────────────────────────────────────────────┘
```

### 8.1 Sprint 1 — Public Offline Seam & Codec Preflight (`ORP256`)

**Theme:** Developer ergonomics, documentation parity, and offline determinism.  
**Focus:** Address `G-08` and `G-07` from ORP147 without modifying realtime callback internals.

- **Core Objectives:**
  1. **Public Offline Render Fixture (`G-08`):** Author a clean-prefix installed-package consumer fixture under `tests/cmake/` demonstrating headless, non-realtime render-to-disk pipelines using only documented `Orpheus::` public targets.
  2. **Golden-Hash Determinism:** Prove bit-identical offline render hashes across various buffer sizes (256, 512, 1024, 2048 samples) and across OS runners.
  3. **Codec & Preflight Capability Registry (`G-07`):** Expose public API methods allowing hosts to query supported file formats, sample rates, and bit depths prior to allocation or playback attempts.
- **Acceptance Criteria:**
  - `ctest -R cmake_find_package` builds and runs the offline render example cleanly from an installed prefix.
  - Stale in-tree offline renderer examples are either updated to match public headers or retired.
  - `realtime_static_audit` remains green.

### 8.2 Sprint 2 — High-Concurrency Control Ingress (`ORP257`)

**Theme:** Multi-threaded control-plane safety and low-latency command ingestion.  
**Focus:** Address `G-02` (bounded multi-producer control ingress).

- **Core Objectives:**
  1. **Bounded MPSC Command Queue:** Replace the single-producer transport command boundary with a bounded, lock-free Multi-Producer Single-Consumer (MPSC) queue, allowing simultaneous event posting from UI, MIDI, OSC, and automation threads without external mutex locks.
  2. **Command Drop & Sequence Telemetry:** Ensure saturating counters track command queue saturation and drops without heap allocations or blocking waits.
  3. **Settlement Contract Conformance:** Preserve tagged start outcomes and active voice reconciliation contracts established in ORP252.
- **Acceptance Criteria:**
  - TSAN stress test: 8 concurrent threads saturating the command queue while the audio thread consumes at 96 kHz / 64 frames. Zero data races, zero deadlocks, zero heap allocations on the consumer side.
  - `realtime_harness_test` confirms memory-hook invariance and zero OS I/O.

### 8.3 Sprint 3 — Windows WASAPI Hardware Promotion (`ORP258`)

**Theme:** Platform truthfulness and evidence closure.  
**Focus:** Address `G-01a` to graduate Windows from experimental/unsupported status.

- **Core Objectives:**
  1. **Real-Device Hardware Execution:** Execute the manual `wasapi-hardware-acceptance` workflow on a verified physical Windows audio workstation.
  2. **Format Negotiation & XRun Validation:** Verify exclusive/shared mode transitions, channel mapping adherence (refusing mismatched channel counts without silent truncation), and underrun telemetry reporting under physical load.
  3. **Matrix Promotion:** Update `docs/SUPPORT_MATRIX.md` to promote Windows/WASAPI to full release-supported status backed by published acceptance artifacts.
- **Acceptance Criteria:**
  - Machine-readable hardware acceptance artifact committed to repository records.
  - Windows Debug and Release CI package/ABI fixtures pass without waivers.

### 8.4 Sprint 4 — Headless Linux ALSA & Robotics Wedge (`ORP259`)

**Theme:** Edge computing, industrial robotics, and headless tooling.  
**Focus:** Address `G-01b` and seed the robotics/asset-tooling expansion.

- **Core Objectives:**
  1. **Dedicated Minimal ALSA Provider:** Implement a focused, zero-allocation ALSA device backend (`Orpheus::alsa` / `Treefall::alsa`) adhering strictly to callback safety contracts (no ALSA plugin bloat, direct MMAP hardware access where available).
  2. **Standalone Asset Linter CLI (`treefall-lint`):** Build a lightweight command-line tool using SDK analysis primitives for CI/CD pipelines (clipping detection, LUFS compliance, SHA-256 fingerprint validation).
  3. **ROS 2 Lifecycle Audio Prototype (`treefall_ros2`):** Package a reference ROS 2 C++ node wrapping the headless ALSA engine, publishing telemetry topics and consuming robot audio cue events.
- **Acceptance Criteria:**
  - ALSA driver passes common device conformance tests in Linux CI (clean enumeration, format negotiation, xrun accounting).
  - `treefall-lint` runs headless in a GitHub Action and verifies sample audio directories with deterministic exit codes.

## 9. Visual responsiveness priority — 2026-09-10 reconnaissance

This dated addendum supersedes the sequencing in §8, not its historical evidence.
The operator's number-one objective is no avoidable visual lag in meters,
counters, playheads, or any other component across Treefall Suite.
This is a research and proposed acceptance contract, not a delivered latency fix.
The product-level criterion is a coherent physical-instrument experience:
control response, audio onset, counter motion and metering must agree in time.
Immediate feedback for an accepted input must remain distinct from confirmed
audio execution; do not fake a started voice to hide settlement latency.

### 9.1 Current implementation and measured experiment

Baseline SDK: published `ca949b2e0af63346d92bb6f2a51ac3c746c2745b`.
ShmUI governed source: `f41fbd3fe4326a37d025a6c97a74c84cfa6ea2b6`,
contract 0.6.0. ORP254 defines TR-40 (four-track mobile) and TR-80 (up-to-eight
track desktop/tablet) as product identities over the existing `fourtrack`
repository lineage. Its current source/README still use FourTrack/EightTrack
runtime tiers; the hardware-scoped split is a plan, not two separately
qualified application repositories. This housekeeping updates their shared
SDK dependency without inventing a rename or claiming new platform support.

- `include/orpheus/realtime_telemetry.h:24-31` defaults to eight callbacks per
  publication and a 64-slot SPSC ring. `src/core/common/realtime_telemetry.cpp`
  preserves unread slots and drops new publications on overflow.
- A throwaway C++ executable compiled against the current telemetry and
  diagnostics implementation exercised 520 synthetic callbacks at 48 kHz,
  512 frames. First publication was callback 8 (85.333 ms of audio), with
  64 pending snapshots and one dropped publication. Draining returned sample
  positions 4,096 through 262,144 although 266,240 frames had been produced.
  This proves cadence and stale-on-overflow behavior, not screen latency.
- The existing per-block opt-in worked in that executable (10.667 ms at
  512 frames). Each snapshot measured 18,808 bytes on this AppleClang/arm64
  build. Changing a default blindly would increase audio-thread copying and
  require maximum-topology deadline evidence; a lean presentation payload is
  preferable to making every diagnostic field run at display cadence.
- The snapshot carries canonical post-block samples and sequence/drop
  evidence, but no producer-to-presentation host-clock correlation. Sample
  position alone cannot tell a host how old a stopped or stalled snapshot is.
- Clip Composer's inspected working tree has a 60 Hz meter drain and 30 Hz
  general UI timer (`Source/MainComponent.cpp`), with no decimation override.
  Its interval accumulator drains the ring in one update, not one historic
  frame per screen refresh. The risk is old retained data after overflow and
  insufficient freshness evidence, not demonstrated slow-motion FIFO playback.
  This checkout contains uncommitted operator meter work; it was not changed.
- ShmUI's `juce/Source/Components/LevelMeter.cpp` has a separate visible-only
  60 Hz timer. A producer UI timer followed by a component timer can introduce
  another scheduling phase. PeakRms already has immediate fill attack and
  elapsed-time release handling; do not remove that work or claim it is absent.
- FreqFinder's `src/PluginEditor.cpp` uses a 34 ms timer. Its analyzer worker
  has independent scope/FFT cadence; an 8,192-frame transform needs about
  170.7 ms of samples at 48 kHz before a full window exists. That is frequency
  analysis resolution, not justification for delaying independent peak meters.
- FourTrack polls active snapshots at 60 Hz and meter-visible idle snapshots
  at 15 Hz (`SnapshotRefreshPolicy.swift`). Its separate display telemetry
  contains a host timestamp, but `ChannelMeter.swift` notes that meter snapshots
  do not. Reuse the existing timebase work rather than introduce a rival clock.

### 9.2 Established metering practice

Measurement ballistics and presentation staleness are separate contracts.
For sample peaks, retain extrema across the measurement interval so a transient
between screen refreshes is not lost. For true peak, use reconstruction-aware
measurement; taking the largest stored sample is not equivalent [9].

VU's approximately 300 ms response and PPM's standard-specific 4/10 ms
integration with slow return are intentional measurement/display conventions,
not evidence that analog instruments have literally zero response time [10].
An RMS calculation with a 300 ms smoother is not automatically a conforming VU
meter. Name and validate the selected measurement rather than equating them.

EBU Tech 3341 specifies ungated 400 ms Momentary and 3 s Short-term loudness
windows, with Short-term live updates at least 10 Hz and Integrated updates at
least 1 Hz. Crucially, it prohibits additional attack/release slowdown after
the sliding windows in EBU Mode [9]. These are measurement/update minima, not
our UI latency targets. The newest valid reading should reach the next eligible
frame without extra cosmetic attack filtering. True-peak and EBU test vectors
must validate measurement accuracy separately from visual timing.

### 9.3 Proposed first delivery: audio-aligned presentation

1. **Measure before promising.** Record audio sample interval, route epoch,
   observation publication, UI consumption and intended frame presentation.
   Report p50/p95/p99/max age, missed frames, dropped observations and route
   validity. Instrument without logging, allocating or locking in callbacks.
2. **Extend the existing clock correspondence.** `AudioProcessBlock` already
   carries `device_sample_position`, `host_time_nanoseconds` and `discontinuity`
   (`include/orpheus/audio_driver.h:303-320`). CoreAudio populates them from
   native timestamps, with conversion/chunk offsets
   (`coreaudio_driver.cpp:816-907`). Carry this evidence through transport and
   presentation instead of inventing a second clock. Add explicit rate/domain,
   epoch and validity/uncertainty where needed. Distinguish output render time
   from DAC presentation and input capture time. Preserve integer sample
   determinism; host timestamps are observations, not render identity.
   Reuse `AudioLatencyBreakdown`; unknown Bluetooth/device terms stay unknown.
3. **Separate current state from event history.** Provide a race-free bounded
   latest-state path for counters and meter state, while preserving ordered
   transport settlement events and cumulative loss counters. Preserve peaks
   over declared intervals even when intermediate UI frames are coalesced.
   A naive overwrite of an SPSC slot or ordinary shared struct is not safe.
4. **One presentation schedule per surface.** Sample the current presentation
   state once for a target frame and feed all relevant components. Prefer
   JUCE's timestamped VBlankAttachment and native display-link scheduling;
   these provide display synchronization, not immunity to a blocked UI thread
   [11]–[13]. Avoid serial independent poll/update/repaint timers.
5. **Counters follow audible position.** Project only transport states with a
   valid clock mapping to the intended display time; handle seeks, stops, loops,
   reverse/varispeed, underruns and device changes explicitly. Never predict
   future signal amplitudes or let a counter coast indefinitely on stale data.
6. **Frame budgets cover every component.** Keep file access, FFT work, waveform
   construction and full-view layout out of the frame-critical path; cache and
   invalidate precisely. Add waveform LOD where the existing processor leaves
   its pyramid TODO. An idle armed/input meter is live even when transport stops.

Proposed acceptance: no extra complete application frame of queueing after a
measurement is available for the next eligible presentation; current-state
recovery on the first eligible frame after a UI stall; no lost interval peaks;
counter error within one actual display interval plus declared clock/route
uncertainty. Qualify 60/120 Hz, multiple block sizes, resize/drag, modal UI,
many active voices, worker load, hidden/show, device changes and long stalls.
These are targets pending measurement, not guarantees already achieved.
Use audio loopback plus a photodiode/high-speed camera for audible-to-photon
acceptance; software timestamps alone cannot prove panel scanout latency.

### 9.4 Remaining SDK growth, after visual timing

| Priority | Gap | Current boundary |
| --- | --- | --- |
| Next | Package/backend qualification | CoreAudio and Dummy supported; WASAPI implementation is not supported without Windows package/ABI and physical-device evidence. |
| Next | Documentation and capability truth | Architecture/roadmap contain stale planned/preloaded-reader descriptions. ASIO CMake references an absent source file; do not advertise a usable provider. |
| Then | Waveform LOD and executable topology | LOD remains TODO; the audio-graph seam is not a complete processor execution engine. |
| Then | Offline workflows | ORP256 has implementation evidence; qualification is distinct. OTIO reconform import/diff return empty plans despite existing serialization. |
| Adopter-gated | Linux device provider | ALSA/JACK/PipeWire are absent; ORP259 is a plan, not a backend. Pick a concrete supported route and hardware gate before robotics expansion. |
| Adopter-gated | Scheduling/immersive expansion | Preserve host policy boundaries; require an actual integration rather than speculative SDK abstractions. |

ORP257's MPSC work is implemented, not an invitation to add a second ingress
queue. ORP258's record says compile-only while later source contains the fix:
reconcile qualification evidence rather than repeat the implementation.
ORP261's package import and each app's adoption are distinct milestones.
Historical hashing TODOs are superseded by current media integrity code.
`stopAllInGroup` refusing host-owned policy is not a defect. ORP262's unresolved
branches are a review/governance register, not a list of missing runtime features.
Duplicate ORP numbers and old completion statements need explicit supersession,
not bulk deletion of provenance.

### 9.5 First-class time contract and ShmUI responsibility

The follow-on operator directive is to master time within the SDK. The unit of
correctness is a coherent observation for an intended presentation instant,
not a nominal refresh-rate setting. This is broader than metering.

| Domain | Existing foundation | Required next contract |
| --- | --- | --- |
| Render/session samples | `TimePoint`, transport position | Explicit epoch and signed transport mapping through seek/loop/reverse/rate changes. |
| Device and host time | `AudioProcessBlock` sample/host pair and discontinuity; CoreAudio native timestamps [14] | Carry rate, domain and validity into presentation observations without a second timebase. |
| Audible/captured time | Directional `AudioLatencyBreakdown` | Define timestamp reference point, uncertainty and conversion delay; never blindly add latency twice. |
| Measurement time | Routing peak/RMS window frame counts | Timestamp interval endpoints, retain transients, distinguish interval peaks from current state. |
| Presentation time | JUCE VBlank timestamp; FourTrack display telemetry | Correlate the display clock explicitly and plan all components for one target frame. |
| Scheduling/deadlines | Bounded transport admission and settlement | Report late/rejected/stale states honestly; timestamp receipt is not evidence of execution or presentation. |

For a valid constant-rate segment, sample position can be derived from a
sample/host anchor and elapsed host time. That mapping must be piecewise and
epoch-qualified: extrapolation cannot cross a seek, route reset, loop boundary,
rate change or missing-clock interval without an explicit transition. A
monotonic observation clock is not the musical transport clock. Renderer
lookahead is permitted only when it describes known scheduled audio, never
fabricated future meter amplitudes.

ShmUI must keep pace: consume the SDK's timed state once per target frame and
provide an externally driven update path that does not also add an independent
component timer. Preserve standalone component usability without running both
paths together. Use elapsed-time, explicitly named ballistics, immediate
sample-peak attack, and no extra EBU-mode smoothing. Swift tokens remain governed
by ShmUI; FourTrack's renderer is app-owned and must implement the same timing
semantics. Web and native frame clocks need documented mappings, not assumed
equal epochs. Input feedback, playhead, counters and meters are all in scope.

Clock synchronization alone does not bound UI queue age or frame deadline
misses. PTP/network audio is not added to scope by this priority: first prove
local sample-to-photon behavior and real-time safety, then reuse that explicit
clock-domain contract if a network-audio adopter requires it.

### 9.6 Phase coherence and continuous responsiveness are joint gates

The operator confirms existing sample-accurate audio locks. Preserve them.
Phase coherence, audio continuity and responsive presentation are simultaneous
requirements: successful audio alone does not excuse stuttering counters,
late meters, or interaction lag. Conversely, presentation backpressure must
never enter the realtime audio path.

Distinguish sample alignment, clock drift, path/group delay and phase response.
Equal sample clocks do not by themselves cancel differential filtering or
resampling delay. For a pure delay, phase offset is `360 * frequency * delay`
degrees: one sample at 48 kHz is 20.833 microseconds, corresponding to 7.5 degrees
at 1 kHz and 75 degrees at 10 kHz. These are calculated examples, not hearing
thresholds. Frequency-dependent phase response cannot generally be corrected
with one scalar delay.

Future changes must preserve shared-channel timing, summing/null invariants,
fractional resampling alignment, declared converter/processing delays, and
capture/playback correspondence through route changes. Independently clocked
devices require explicit drift handling; clock synchronization is not proof of
phase-coherent signal paths. Any added audio delay must serve an explicitly
approved audio requirement, never conceal visual lateness.

The operator's expectation is responsiveness throughout operation, not an
average-FPS claim. Treat every measured frame-deadline miss, stale observation,
counter hitch and missed transient as a failure to explain and remediate.
p50/p95/p99 are diagnostic summaries; maximum age and failure counts are release
criteria too. Qualify sustained and burst load, all supported active/idle/input
states and interaction paths. Fault-injection recovery is a separate test, not
permission for normal-operation stalls. State the measured hardware/workload
envelope honestly; no claim of universal zero latency is established by this
reconnaissance.


### 9.7 Demand-driven resource scheduling

Temporal correctness includes cool, efficient operation. Do not solve staleness
by permanently polling every field, repainting every component or running all
analyzers at the fastest available cadence. Apple documents the energy cost of
timer wakeups and recommends event notifications and stopping unused timers
[15]; its blocking/GCD examples are control/worker-thread guidance, not permission
to block or dispatch allocating work from an audio callback.

- Keep audio processing device-driven and bounded; no busy waits.
- Use one presentation schedule for active visible motion, reading a coherent
  lean state once per frame; cache static layout and redraw only changed regions.
- Drive noncontinuous state from change notifications. Coalesce background work
  within its deadline; skip obsolete analysis jobs rather than replay a backlog.
- Stop rendering hidden/occluded surfaces and stop unneeded analysis. Retain
  transport settlement/loss accounting and meaningful peak retention. Never
  equate stopped transport with inactive capture or live input.
- Measure idle and active CPU time, wakeups, allocations, deadline misses,
  observation age and sustained thermal behavior separately. Efficiency is not
  permission to lower live-meter cadence or make counters stutter.

The same requirements apply to Treefall SDK, ShmUI, Clip Composer, FreqFinder
and the TR-40/TR-80 lineage in `fourtrack`. They are now recorded in active
architecture/repository guidance as release blockers. Policy adoption does not
mean the timing architecture has already been implemented or thermally qualified.

### 9.8 Accepted contract refinements and session logging

The operator's detailed refinements are adopted in public `ARCHITECTURE.md`
§Temporal Coherence and govern every child repository:

1. **Explicit valid clock relationships.** Observations identify domain and
   timestamp reference, samples/rate, clock/route epoch, transport segment,
   validity/discontinuity/uncertainty, sequence and measurement interval.
   Device, transport and display time remain distinct; conversions apply only
   over valid segments and must not double-count output latency.
2. **Coherent state and one target instant.** Readers receive immutable coherent
   observations, not independently changing atomic fields. Each display frame
   uses compatible observations and one presentation target, without forcing
   peak, RMS, loudness and counters into one measurement window. Predict valid
   transport positions only, never amplitudes or unconfirmed execution.
3. **Separate bounded delivery contracts.** Latest current state, interval
   extrema/coverage gaps, and ordered transport outcomes/reconciliation remain
   distinct. Bound every retry and copy volume; test stalls, overflow and
   concurrent route changes without unsafe overwrite or audio backpressure.
4. **Independent signal-path timing verification.** Preserve existing locks;
   use impulses and coherent multitone/sweep measurements for differential
   delay, drift and phase. Null tests apply only where identical outputs are
   expected. Scalar delay cannot fix arbitrary phase response.
5. **Joint efficiency/responsiveness gates.** Declare the hardware/workload
   envelope; qualify audio/frame deadlines, maximum age, counter continuity,
   retained transients, discontinuity recovery, CPU/wakeups/allocations and
   sustained thermals. Stop hidden rendering, not necessary event accounting.
6. **Precise terminology and semantics.** Use temporal coherence, clock-domain
   integrity and atomic observation publication. Do not introduce an “atomic
   time” API or instantaneous-behavior claim. Timing is part of an observation's
   meaning, not a timestamp appended after causal context is lost.
7. **First-class session logging.** Preserve event time/cause/session/epoch/
   segment/sequence separately from collection or durable-write time. Wall-clock
   display correlation is off-thread; unknown cross-clock ordering stays unknown.
   Bound queues, batching and log-view work; expose gaps and persistence failure.
   No serialization, disk I/O or allocating logger calls on the audio thread.

Source evidence for logging: `include/orpheus/errors.h:30-39` exposes generic
string logger/JSON telemetry callbacks, not a structured timed session journal.
`src/core/common/errors.cpp:78-98` copies strings and invokes callbacks
synchronously; it must not be repurposed as a realtime log queue. Clip Composer
already has app-owned `Source/Core/PlaybackLedger`: bounded pending/emergency
records and spool, sequence/gap/durable frontier health, session identity and a
writer-owned SQLite connection. Reuse and extend that boundary rather than
create a competing journal. Its UTC millisecond/sequence fields need explicit
audio/host timing correlation for the new contract; this reconnaissance does
not claim that correlation is already implemented.

### Addendum references

[9] EBU, “Tech 3341: Loudness Metering — EBU Mode,” Nov. 2023,
https://tech.ebu.ch/docs/tech/tech3341.pdf (accessed 2026-09-10).

[10] H. Robjohns, “What's the difference between PPM and VU meters?,”
Sound On Sound, Jul. 2013,
https://www.soundonsound.com/sound-advice/q-whats-difference-between-ppm-and-vu-meters
(accessed 2026-09-10).

[11] JUCE, “Timer Class Reference,”
https://docs.juce.com/master/classjuce_1_1Timer.html (accessed 2026-09-10).

[12] JUCE, “VBlankAttachment Class Reference,”
https://docs.juce.com/master/classjuce_1_1VBlankAttachment.html
(accessed 2026-09-10).

[13] Apple, “CADisplayLink,”
https://developer.apple.com/documentation/quartzcore/cadisplaylink
(accessed 2026-09-10).

[14] Apple, “AudioTimeStamp,”
https://developer.apple.com/documentation/coreaudiotypes/audiotimestamp
(accessed 2026-09-10).

[15] Apple, “Energy Efficiency Guide for Mac Apps: Minimize Timer Usage,”
https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/Timers.html
(accessed 2026-09-10).
