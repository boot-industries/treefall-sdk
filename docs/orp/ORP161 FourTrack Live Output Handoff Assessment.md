<!-- SPDX-License-Identifier: MIT -->

# ORP161 — FourTrack Live Output Handoff Assessment

**Document type:** SDK dependency assessment  
**Status:** Plan required; no live-output implementation started  
**Date:** 2026-07-19  
**Inputs:** FourTrack FTR020, FTR050, FTR051, and FTR052

---

## Decision

FTR020 is complete in the SDK. FTR052 is not implemented and is a multi-sprint
public/realtime API programme, not a safe follow-on patch. FourTrack cannot begin
FTR051 until the requested live-output primitives are designed, implemented,
release-qualified, and then adopted through an SDK release pin.

No FourTrack source, pin, fallback, output adapter, credentials, virtual-device
bundle, or network work was changed by this assessment.

## FTR020 status: delivered

The current SDK `AudioDriverConfig` exposes separate `input_device_id` and
`output_device_id` fields; no `device_id` compatibility alias remains. CoreAudio
resolves non-empty values as stable device UIDs, validates direction, and returns
`SessionGraphError::InvalidParameter` for unknown or incompatible explicit
endpoints rather than silently using a default. Empty direction fields retain
directional defaults. The CoreAudio test suite contains explicit endpoint,
default, incompatibility, aggregate-lifecycle, and capture coverage.

This is the contract delivered by ORP155/ORP156. FourTrack can satisfy the FTR020
application handoff by mapping its persisted input/output UIDs into those two
fields when it constructs its driver configuration.

## FTR052 status: absent, with one required reconciliation

No `LiveAudioBlockView`, `ILiveAudioFanout`, stream drain/live-edge API, prepared
streaming converter, adaptive clock bridge, audio-driver runtime event surface,
or deterministic live-output fixture exists in the current SDK tree. The current
`IAudioDriver` exposes configuration, capabilities, latency, and capture-failure
telemetry only; it does not expose FTR052's selected-route snapshot, maximum
callback capacity, ordered runtime events, or capacity-exceeded transition.

FTR052 ORP-LO-001–005, ORP-LO-007, and ORP-LO-008 therefore remain blocking and
unstarted. FTR051 correctly treats them as release prerequisites rather than app
work.

FTR052 ORP-LO-006 was written against SDK v0.5.3 and must be reconciled before
implementation:

- it names the removed `AudioDriverConfig.device_id` compatibility field;
- it prohibits a private aggregate for distinct input/output devices; while
  ORP155 intentionally creates and owns a private aggregate when a one-driver
  CoreAudio duplex configuration selects different physical endpoints; and
- it asks for selected-route/max-frame/event contracts that ORP155 did not add.

A new plan must preserve the released directional-ID contract and explicitly
decide whether FTR052's output-only/multiple-driver use case is separate from the
existing split-device duplex aggregate behaviour. It must not restore
`device_id`, weaken fail-closed endpoint selection, or silently replace an
explicit route.

## Required plan sequence

1. **Contract reconciliation:** FourTrack product owner resolves FTR051's
   mirror-heard-master versus separate-program-master tap semantic. SDK and
   FourTrack agree the updated ORP-LO-006 topology/capability contract against
   the released ORP155 directional route semantics.
2. **Fan-out foundation:** deliver ORP-LO-001–003 and ORP-LO-008 as a standalone
   C++20 public package with fixed-capacity allocation, stream isolation,
   lock-free status, deterministic fixtures, realtime audit, and consumer test.
3. **Clock primitives:** deliver ORP-LO-004 fixed streaming conversion and
   ORP-LO-005 adaptive bridge separately, with deterministic long-run drift and
   discontinuity coverage before any CoreAudio virtual-device adoption claim.
4. **Driver lifecycle:** deliver the remaining ORP-LO-006 capability delta and
   ORP-LO-007 whole-block capacity/event contract across dummy and CoreAudio
   backends, retaining ORP155's fail-closed UID routing.
5. **Release/adoption:** package, document, and release the APIs; then FourTrack
   updates its pin and performs its ASan/UBSan/TSan and product integration gates.

Each phase changes public contracts and/or callback behaviour. Plan approval is
required before implementation so the immutable FourTrack/SDK ownership boundary,
tap semantics, aggregate policy, API shape, and release acceptance criteria are
settled together.
