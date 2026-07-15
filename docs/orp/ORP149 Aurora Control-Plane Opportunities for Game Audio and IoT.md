<!-- SPDX-License-Identifier: MIT -->

# ORP149 — Aurora Control-Plane Opportunities for Game Audio and IoT

**Document type:** Research conversation and integration recommendation  
**Status:** Non-binding opportunity record; no cross-repository work authorized  
**Date:** 2026-07-14  
**Scope:** How Aurora—the broadcast-orchestration product in the `~/dev/nmos`
repository—could complement Orpheus SDK in future game-audio and Internet of Things
(IoT) systems.  
**Out of scope:** Adding Aurora/NMOS/MCP/network-control code to Orpheus now;
claiming a game runtime, IoT platform, NMOS endpoint, or production device-control
integration; or changing either repository's release scope.

---

## 1. Conversation in one page

**Question: Is Aurora relevant to Orpheus game audio?**  
**Answer:** Yes, but at the *control-plane and facility-integration* layer—not in a
consumer game's audio callback or engine's frame loop.

**Question: Does Aurora make Orpheus's game-audio direction less necessary?**  
**Answer:** No. Orpheus owns audio facilities: decode/analysis, local audition and
render, deterministic transport, routing, timing, and bounded diagnostics. Aurora
owns planning and verified orchestration of external media/control infrastructure.
They address different jobs [1]–[4].

**Question: Is Aurora relevant to IoT?**  
**Answer:** Potentially, as a high-integrity orchestrator for professional audio
appliances, venue/attraction endpoints, and gateway-controlled devices. It is not
yet a generic IoT platform. Aurora's present model is route-centric, and current
security/device-control/write boundaries rule out an immediate broad IoT claim [3],
[5], [6].

**Question: What should happen now?**  
**Answer:** Preserve the seams, document the joint system model, and run one
read-only, simulated integration study before any write, protocol, or public API
work. The initial target is an Orpheus-hosted desktop/edge endpoint observed by an
Aurora adapter—not an NMOS implementation inside Orpheus.

---

## 2. The systems as they are

### 2.1 Orpheus SDK

Orpheus is a host-neutral C++20 audio SDK. Its current public contracts include
realtime-safe local clip transport, prepared/streaming clip sources, routing and
metering, driver capability discovery, stable session transactions/recovery, media
integrity, offline analysis, and bounded realtime telemetry [1], [2].

Its game-audio opportunity is deliberately staged: credible desktop/offline asset
QA and audition now; evidence-led expansion toward bounded bespoke desktop runtime
facilities later. It does **not** currently supply a game-engine integration,
consumer-game runtime, spatial renderer, mobile/console/Web platform support, or
network-audio control plane [1].

### 2.2 Aurora (`~/dev/nmos`)

The repository path is `nmos`, but the product is **Aurora**. Aurora is a
broadcast-grade orchestration control plane: a vendor-neutral `ControlService`
implements plan → stage → activate → verify, compatibility checking, snapshots and
rollback, and append-only audit; NMOS and Ember+ are pluggable southbound adapters;
MCP is its northbound agent surface [3], [5].

Aurora currently provides NMOS IS-04 discovery, IS-05 route lifecycle, IS-08 audio
channel mapping, IS-09 system/clock observations, IS-11 active stream compatibility
read, BCP-004-01 receiver capability evaluation, batch route rollback, audit, and
an intentionally narrow Lawo-level write candidate [3], [5], [6]. Its Phase 4
Lawo write work is **in progress and blocked on bench validation**; matrix/mute/
generic Ember writes, IS-12, OAuth/TLS, and generic device control remain excluded
or deferred [6].

### 2.3 The essential boundary

| Concern | Orpheus owns | Aurora owns | Must not be conflated |
| --- | --- | --- | --- |
| Audio rendering | Decode, source readiness, PCM render, transport/routing behavior | Never renders audio | A network route is not an audio callback command |
| Realtime safety | Bounded control/audio-thread contracts and telemetry bridge | Never enters the callback | MCP/RPC/MQTT work cannot run in audio render paths |
| Local device capability | Driver/format/buffer capability observed by a host | External endpoint/route compatibility and facility health | Aurora cannot infer an Orpheus driver is an NMOS endpoint |
| External routing | Exposes local render/output prerequisites | Plans/stages/activates/verifies routes where a real adapter/device supports it | Orpheus does not become an IS-05 controller/server automatically |
| Audit/recovery | Session/media recovery and audio diagnostics | Intent/route/device audit and snapshot rollback | Audio state and plant state have distinct recovery laws |
| Product policy | Host application chooses playback/session policy | Facility/operator chooses orchestration policy | Neither product absorbs game logic, UI, auth policy, or safety policy |

The integration rule follows directly: **Aurora may request or observe a
control-plane state transition; an Orpheus host application decides whether that
transition is valid, serializes it through its own control boundary, and reports a
bounded result.**

---

## 3. Game-audio opportunities

### 3.1 Where Aurora helps

Aurora is not relevant to ordinary local game playback. A consumer game needs its
own engine/event/object/voice/spatial layer; its audio render cannot depend on MCP,
HTTP, registry discovery, or a facility network being available.

Aurora becomes relevant in the *adjacent professional environment*:

| Scenario | Orpheus role | Aurora role | Value |
| --- | --- | --- | --- |
| Game-audio asset QA lab | Analyze/audition/render known source media; report bounded diagnostics | Record test-lab route/topology context and coordinate shared IP-media endpoints | A reproducible test result names both the asset/render contract and the external signal path |
| Virtual production or motion-capture stage | Local audio playback/capture component in a host tool | Plan/verify NMOS routes, channel maps, clock-domain observations, and rollback across the facility | Keeps stage routing out of the game/audio tool's private control code |
| Location-based game/attraction | Local deterministic audio node in the application | Coordinate approved external media routes and device health in the venue | Separates playback correctness from multi-vendor facility orchestration |
| Custom-engine hardware-in-loop test | Controlled Orpheus fixture produces/consumes local PCM | Drive a test topology and retain plan/activate/verify audit evidence | Tests real integration without declaring a consumer-game runtime feature |

These are meaningful game-audio opportunities, especially for studios that straddle
interactive content and IP-media production. They do not make NMOS a mandatory
engine dependency.

### 3.2 Location-based entertainment is the primary joint opportunity

Location-based entertainment (LBE)—attractions, themed venues, immersive
installations, interactive museums, and park-scale games—is where Orpheus's local
audio role and Aurora's external orchestration role reinforce one another most
directly. This is not a speculative “smart venue” label: a current themed-entertainment
deployment documents distinct operating zones, show start/stop, audio/video/lighting
control, special-event changes, and weather-related adjustment [9].

An LBE flow has four separable phases:

| Phase | Orpheus host/application | Aurora | Must remain elsewhere |
| --- | --- | --- | --- |
| Content commissioning | Verify local media, session identity, source layout, and render fixtures | Audit approved facility topology/version | Asset library, show authoring, rights, operator UI |
| Pre-open preflight | Validate local device/media/session readiness and report bounded health | Discover/validate external senders, receivers, clock/route compatibility, and stage dry-run plan | Physical safety inspection and venue sign-off |
| Show operation | Execute the application's already approved local playback behavior and preserve panic/stop independence | Activate/verify external media routes and retain auditable state transitions | Show narrative, lighting/video cues, guest interaction logic |
| Fault/recovery | Report local failure, stop or enter a host-defined safe state | Report route/clock mismatch, restore a recorded external snapshot where valid | Human incident command, evacuation/safety systems, mechanical ride control |

This gives LBE a more concrete first target than a generic IoT appliance:
**a local Orpheus audio node can remain correct when the venue network is absent,
while Aurora makes facility dependencies observable and recoverable.**

The dependency order remains strict. G-04's fixed channel-aware render topology is
needed before making physical-output/cue claims; G-02's bounded ingress is needed
before more than one local control source is admitted; G-03 scheduling is an
optional later timing primitive, not a show-control product; and G-05 immersive
PCM-bed work stays strategic [2]. The initial LBE profile must therefore describe
what exists today, not promise cues, timecode, distributed synchronization, or
immersive rendering.

**Recommended LBE proof.** Replace the abstract R1 study with a simulated
three-zone attraction fixture: one Orpheus host per zone, a deliberately missing
asset in one zone, a changed external route in another, and a panic/stop event in
the third. Aurora must dry-run/audit the facility plan; the hosts must report
truthful readiness; no simulated control result may modify the audio callback or
claim a physical route.

### 3.3 A viable first joint study: read-only endpoint observation

The first study should be intentionally modest and simulated:

1. Build an **external** `aurora-orpheus` demonstration adapter or test harness.
   It consumes only installed Orpheus public headers and Aurora's existing public
   service/adapter seams.
2. An Orpheus host publishes a *read-only endpoint descriptor*: host-owned endpoint
   identity, configured sample rate/block/channel layout, output/readiness state,
   bounded transport health, and media/session readiness. It does not claim NMOS
   sender/receiver identity.
3. Aurora reads the descriptor, associates it with a simulated facility topology,
   and produces a dry-run plan plus audit record. No Aurora tool may manipulate an
   Orpheus transport, driver, or routing matrix.
4. A deterministic fixture proves that a changed session/media/device condition
   changes the descriptor/result truthfully, without a callback allocation, network
   operation, or inferred physical route.

**Study outcome:** establish whether an external endpoint/adapter vocabulary is
useful. A passing demo is not a new Orpheus target, an NMOS implementation, or
permission for live control.

### 3.4 Future game-audio control plane: staged, never direct

If the read-only study earns real adopter interest, a later integration can advance
in small steps:

1. **Capability/report schema** — versioned, host-neutral Orpheus endpoint report
   that an adapter can translate into Aurora-specific models.
2. **Intent/result bridge** — Aurora emits a proposed named intent; the host
   validates it against local policy and returns accepted/refused/completed result.
   No protocol payload or route instruction crosses into Orpheus core.
3. **External-route coordination** — only for actual NMOS facilities. Aurora
   retains IS-05 plan/stage/activate/verify/rollback; the host performs only its
   documented local transition after the facility outcome is known.
4. **Game/engine adapters** — remain separately owned packages. Unity/Unreal/custom
   lifecycle mappings are not an Orpheus core abstraction and are not an Aurora
   southbound adapter by default.

The future Orpheus G-02 bounded multi-producer dispatcher and G-03
sample-addressed-event research are relevant internal safety seams, but neither
should be built as an “NMOS API.” They remain protocol-neutral SDK work first [2].

### 3.5 Augmented-reality audio: local pose, external facility

AR makes the real-time boundary even sharper. ARCore anchors maintain a
world-relative pose while the platform's environmental understanding updates; their
pose can change as the world estimate improves [10]. An AR application must handle
that local, frame-rate-sensitive spatial relationship in its engine/runtime. It
cannot wait for Aurora, an NMOS registry, an MCP client, or an IoT broker.

**What Orpheus can plausibly offer first:** the authoring/tool side—asset analysis,
format validation, controlled audition, deterministic offline render, and eventually
the protocol-neutral local timing/voice/topology contracts recorded in ORP147/148.
For a venue AR installation it may also be the local audio component that reports
readiness to an Aurora adapter.

**What remains outside Orpheus today:** mobile/wearable device backend, ARKit/ARCore/
OpenXR bindings, world-anchor lifecycle, head pose, HRTF/spatialization, occlusion,
shared-anchor/cloud service, game object model, and rendered AR scene policy. A
future adapter may translate an engine-owned anchor/pose into a host-owned audio
intent, but no pose or network event may cross directly into the audio callback.

**AR admission gate.** Before adding an audio-anchor API, two independent AR
developers must identify the same engine-neutral contract, including coordinate
space, update rate, pose-loss/relocalization behavior, listener semantics, mobile
platform, spatial renderer ownership, and the proof that a generic SDK boundary is
better than an engine adapter. Until then, AR is an LBE/tooling research extension,
not a runtime promise.

### 3.6 Livestreaming: an honourable but real control-plane fit

Livestreaming is not Orpheus network audio. It is, however, a meaningful
professional-audio workflow: a local app may provide prepared playback, cue/bed
handling, media integrity, diagnostics, and deterministic recovery while a separate
production stack carries the encoded stream to an ingest service/CDN.

NMOS is relevant where a livestream is produced inside a professional IP-media
facility. The NMOS model describes professional networked-media control separately
from audio/video transport, and Aurora already implements discovery, connection
management, channel mapping, compatibility, and clock observation [3], [11]. This
makes a credible joint path for a streaming control room or event facility:

| Concern | Orpheus host | Aurora | External streaming stack |
| --- | --- | --- | --- |
| Local playout/cue | Owns playback, local stop/panic, media/session recovery | Observes approved endpoint state | None |
| Facility route | Reports only host readiness/capability | Plans/stages/activates/verifies real NMOS routes | Encoder receives its configured input |
| Stream encode/distribution | Never owns encoder or CDN behavior | Never substitutes for stream transport | Encoder, packager, ingest, CDN, viewer telemetry |
| Incident response | Truthful local diagnostics; host-defined safe state | Audited topology/clock/route evidence | Platform-specific stream health and failover |

**Recommendation:** keep livestreaming an honourable secondary LBE/broadcast
opportunity. The first study can reuse the LBE fixture with an encoder-shaped
simulated receiver. Do not add RTMP/SRT/WebRTC/HLS, CDN APIs, chat, ad insertion,
viewer metrics, or stream redundancy to either core merely because a local audio
node participates in a livestream.

### 3.7 Explicit game-audio non-goals

- No consumer-game NMOS discovery or registration requirement.
- No MCP, HTTP, WebSocket, MQTT, or Aurora client on the audio thread.
- No Aurora-driven gameplay/event/state system, voice manager, spatializer, or
  game object model.
- No inference that a multichannel Orpheus route is ST 2110, AES67, Dante, or a
  production IP-media stream.
- No live facility write or safety-critical claim until both the Aurora adapter and
  the actual endpoint/device have their own verified acceptance evidence.

---

## 4. IoT opportunities

### 4.1 The right IoT frame

“IoT” is too broad to be a capability. Aurora can plausibly serve **managed audio
edge systems**—an Orpheus-based appliance, unattended installation node, sound
system endpoint, or controlled test rig—when an operator needs discovery,
preflight, audited intent, and explicit verification around an external system.

This is distinct from an arbitrary consumer sensor fleet. MQTT is a lightweight,
brokered publish/subscribe transport designed for constrained M2M/IoT environments;
it has its own session, QoS, authentication, retention, and topic governance
concerns [8]. Adding MQTT does not automatically supply Aurora's route lifecycle,
nor does Aurora's route lifecycle make it a general device-management system.

### 4.2 Immediate IoT-adjacent fit: observe, diagnose, recover

An Orpheus appliance can already expose host-owned bounded status from public driver,
transport, media-integrity, and telemetry contracts. Aurora can already present
observed topology/health and retain an append-only audit around its external
orchestration actions [2], [3].

A safe integration can therefore support:

- appliance/device inventory **owned by the host deployment**;
- reported local readiness, underrun/drop counts, selected format/channel shape,
  media resolution state, and versioned application configuration identity;
- read-only Aurora health/audit views that correlate a local audio failure with a
  facility route/clock observation; and
- manual, host-owned recovery procedures triggered from a verified operator runbook.

It must not convert telemetry into an unauthenticated command channel or claim
remote repair merely because an endpoint reports health.

### 4.3 Candidate device-control pathways

#### I-01 — NMOS IS-12 for professional media appliances

IS-12 is the AMWA NMOS Device Control & Monitoring Protocol. Its official
specification covers exposure/consumption of NMOS control models and includes
commands, device-model exploration, subscriptions, and IS-04 interactions [7]. It
is the closest future Aurora path for a professional Orpheus appliance installed in
an NMOS facility.

**Recommendation:** Aurora should complete its separately scoped IS-12 research and
first-device evidence before any Orpheus linkage. If a real appliance needs it, build
an external adapter that exposes a tiny, versioned, allowlisted control model—never
the SDK's arbitrary public object graph.

#### I-02 — MQTT gateway for non-NMOS edge deployments

MQTT may be appropriate for an installation/IoT gateway that forwards device health
and accepts *pre-authorized*, high-level host intents. It should be a separate
Aurora adapter/integration, not a dependency of Orpheus core [8].

**Recommendation:** only explore after I-01 or a named non-NMOS customer proves
that MQTT solves a deployment need which IS-12/Ember+/existing host APIs cannot.
The design must specify broker trust, TLS/authentication, topic namespace,
allowlisted operations, idempotency, QoS/retained-message behavior, offline
behavior, replay protection, and audit correlation before any write is allowed.

#### I-03 — Facility profile for Orpheus-based appliances

A later profile could define the *semantic* state that any Aurora adapter reads:

- application/build/public-contract versions;
- driver capability and selected format/buffer/channel state;
- configured endpoint role (not a physical route assertion);
- media/session verification status;
- bounded realtime health counters and last-update sequence; and
- a small allowlisted command vocabulary such as “request controlled stop” or
  “request session validation,” always subject to host policy.

This is a profile specification, not an Orpheus dependency. It must use stable
versioning, bounded fields, privacy/redaction rules, and capability negotiation.

### 4.4 IoT security and operating gates

Aurora currently defers IS-10 OAuth2/TLS and generic device control. Its Phase 4
Lawo candidate allows only numeric, path-allowlisted writes with dry-run, bounds,
exact re-read verification, and audit; it is still bench-gated [5], [6]. Those
facts rule out a broad “Aurora controls IoT devices” claim today.

No future IoT write path should proceed without all of the following:

1. authenticated and encrypted transport appropriate to the deployment;
2. explicit device identity, capability version, operator/service authorization,
   and allowlisted commands;
3. dry-run/plan semantics where an action can be meaningfully planned;
4. bounded deadlines, idempotency/replay behavior, and a declared offline policy;
5. device re-read or equivalent independent verification for state-changing writes;
6. append-only audit with correlation IDs linking Aurora intent to host result;
7. fail-safe behavior that never blocks Orpheus panic/stop or waits in the audio
   callback; and
8. hardware/bench acceptance for the named device and protocol—no simulated success
   promoted as field support.

---

## 5. Ownership model and proposed seams

### 5.1 Repository ownership

| Artifact | Owning repository | Why |
| --- | --- | --- |
| Realtime transport/routing/telemetry contract | Orpheus SDK | It must satisfy audio-thread correctness, installed package, and device evidence rules |
| Host application policy, credentials, session selection, presentation | Orpheus consumer application | Only the deployed application knows its operational/safety policy |
| External endpoint descriptor and adapter mapping | New, separately versioned integration package/repository | Prevents Aurora/NMOS dependencies from entering audio core; permits independent release cadence |
| Route discovery, compatibility, plan/stage/activate/verify, audit | Aurora | This is Aurora's tested control-plane model |
| NMOS IS-04/05/08/09/11, Ember+, IS-12, or MQTT protocol clients | Aurora adapter layer | Protocol/version/security surface belongs below Aurora's control service |
| Game-engine plug-in | Separate engine integration | An engine owns editor/runtime lifecycle and version matrix |

### 5.2 Message model

```
Aurora plan / external facility state
              │
              ▼
  integration adapter (network/control thread only)
              │  versioned request/result; bounded fields
              ▼
  Orpheus host application policy/dispatcher
              │  one control boundary
              ▼
     Orpheus public transport/routing/driver contracts
              │
              ▼
        audio callback — never networked or blocked
```

The adapter is intentionally the only cross-repository dependency point. It can be
removed without changing audio behavior, and a failed/partitioned adapter cannot
silently produce or mutate audio state.

### 5.3 One contract before code

Before any proof-of-concept, write an `AuroraOrpheusEndpoint/v1` schema with:

- endpoint/application identity and schema version;
- public SDK version and declared host capability set;
- non-sensitive device/configuration summary;
- media/session/readiness status enums, never arbitrary local paths;
- monotonic telemetry sequence/drop semantics; and
- typed intent/result records with correlation ID, accepted/refused reason, deadline,
  and completed/verified state.

The schema must not contain raw PCM, callback pointers, opaque private transport
objects, user credentials, arbitrary command strings, or an implicit authorization
bypass.

---

## 6. Recommended sequence

| Order | Research or branch | Goal | Exit criterion |
| --- | --- | --- | --- |
| R0 | Preserve boundaries | No code; retain this record and ORP147/148 gates | Owners agree Aurora is control plane, Orpheus is audio infrastructure |
| R1 | Read-only three-zone LBE simulation | Determine whether shared status/topology language handles local failure and external facility context honestly | Three public-only Orpheus fixtures + Aurora dry-run/audit; one missing-asset, one route-change, and one panic case; zero audio-thread network work |
| R2 | Endpoint schema review | Make the adapter data durable and safe | Versioned schema, redaction, failure, and ownership rules reviewed by both projects |
| R3 | Aurora IS-12 / security maturity | Establish credible professional device-control preconditions | Named device, protocol scope, security model, bench acceptance in Aurora; no Orpheus change required |
| R4 | One controlled appliance integration | Prove one allowlisted intent/result round trip | Hardware evidence, re-read verification, audit correlation, panic/stop independence |
| R5 | MQTT gateway discovery | Assess non-NMOS IoT value separately | Two real non-NMOS deployments and a complete broker/security/offline contract |
| R6 | Game/facility integration package | Support virtual-production or attraction environment, not consumer runtime | Named owner, engine/facility smoke path, installed Orpheus package and real Aurora adapter evidence |

**Do not skip R1/R2.** They determine whether the integration is genuinely useful
or merely a diagram connecting two interesting repositories.

---

## 7. Recommendations

1. **Make location-based entertainment the first joint-market research target.**
   It exercises multi-zone audio, external route/clock state, operator recovery,
   and IoT-adjacent endpoint health without turning either core into show control.
2. **Treat AR as local runtime first, venue integration second.** Orpheus can earn
   AR relevance through assets/tooling and later neutral local contracts; Aurora may
   coordinate a venue, never an AR frame loop.
3. **Keep livestreaming as an honourable secondary facility flow.** It validates
   the same Orpheus local-playout + Aurora route-control split without claiming an
   encoder, CDN, or network-audio stack.
4. **Treat IoT as an appliance-control opportunity, not a generic platform claim.**
   Begin with trusted, professional audio endpoints and explicit host policy.
5. **Fund the read-only three-zone LBE simulation before protocol work.** This is
   the highest-value, lowest-risk answer to whether the systems need each other.
6. **Do not put network clients in Orpheus core.** Aurora/MQTT/NMOS/IS-12 belong in
   external adapters and host control threads.
7. **Do not reopen Aurora write scope through this integration.** Aurora's current
   Lawo work must complete its own bench gate; IS-12/MQTT must earn their own
   security/device evidence.
8. **If R1 succeeds, create a dedicated integration plan rather than editing either
   core opportunistically.** It should name one endpoint, one deployment, one
   protocol path, one test fixture, and one verification record.

---

## 8. References

[1] Orpheus SDK, “ORP148 — Game-Audio Developer Opportunity Research,” repository
documentation, Jul. 2026. Local source:
`docs/orp/ORP148 Game-Audio Developer Opportunity Research.md`. [Accessed: Jul. 14,
2026].

[2] Orpheus SDK, “ORP147 — SDK Customer-Fit Gap Register and Incremental Build
Guide,” repository documentation, Jul. 2026. Local source:
`docs/orp/ORP147 SDK Customer-Fit Gap Register and Incremental Build Guide.md`.
[Accessed: Jul. 14, 2026].

[3] Aurora, “README,” repository root, Jul. 2026. Local source:
`~/dev/nmos/README.md`. [Accessed: Jul. 14, 2026].

[4] Orpheus SDK, “ORP143 — Reliability and Adoption Sprint Completion and
Child-App Handoff,” repository documentation, Jul. 2026. Local source:
`docs/orp/ORP143 Reliability and Adoption Sprint Completion and Child-App Handoff.md`.
[Accessed: Jul. 14, 2026].

[5] Aurora, “NMS012 Aurora Product Design Brief,” repository documentation, May
2026. Local source: `~/dev/nmos/docs/nms/NMS012 Aurora Product Design Brief.md`.
[Accessed: Jul. 14, 2026].

[6] Aurora, “NMS013 Aurora Phase 4 Plan,” repository documentation, Jul. 2026.
Local source: `~/dev/nmos/docs/nms/NMS013 Aurora Phase 4 Plan.md`. [Accessed:
Jul. 14, 2026].

[7] Advanced Media Workflow Association, “IS-12: NMOS Control Protocol,” 2026.
[Online]. Available: https://specs.amwa.tv/is-12/ [Accessed: Jul. 14, 2026].

[8] OASIS, “MQTT Version 5.0,” *OASIS Standard*, Mar. 2019. [Online]. Available:
https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html [Accessed: Jul. 14,
2026].

[9] Q-SYS, “Movie Park Germany has ultimate control with Q-SYS,” *Themed
Entertainment Case Study*, 2025. [Online]. Available:
https://www.qsys.com/resources/case-studies/movie-park/ [Accessed: Jul. 14, 2026].

[10] Google, “Working with Anchors,” *ARCore documentation*, 2026. [Online].
Available: https://developers.google.com/ar/develop/anchors [Accessed: Jul. 14, 2026].

[11] Advanced Media Workflow Association, “NMOS Technical Overview,” 2026.
[Online]. Available:
https://specs.amwa.tv/nmos/main/docs/Technical_Overview.html [Accessed: Jul. 14, 2026].
