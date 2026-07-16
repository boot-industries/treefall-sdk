<!-- SPDX-License-Identifier: MIT -->

# ORP153 — Clip Composer Routing State Snapshot Adoption Handoff

**Document type:** Downstream adoption handoff  
**Owning implementation team:** Clip Composer  
**Issuing repository:** Orpheus SDK  
**Status:** Dependency-blocked; schedule after the SDK routing-state contract ships  
**Date:** 2026-07-16  
**Related SDK direction:** [[ORP147 SDK Customer-Fit Gap Register and Incremental Build Guide]]

---

## 1. Purpose

This document hands one child-application task to the Clip Composer team: remove
its shadow copy of SDK routing gain, mute, and solo state after Orpheus publishes
a coherent routing-state query contract.

This is downstream adoption work. Orpheus owns the prerequisite public API,
realtime publication model, package fixture, and SDK contract tests. Clip
Composer owns its SDK pin, app-side migration, UI projection, and application
verification.

Do not begin the migration against the current SDK API. `IRoutingMatrix` does
not yet provide the required coherent configured-state query.

---

## 2. Current downstream state

Clip Composer currently reaches the public routing matrix through
`ITransportController::getRoutingMatrix()`, but it cannot read all configured
group controls from that interface:

- `IRoutingMatrix::isGroupMuted()` returns effective mute state;
- no public query returns a group's configured solo flag; and
- no public query returns a group's configured gain in decibels.

The application therefore maintains three four-element shadow arrays in
`Source/Audio/AudioEngine.h`:

- `m_groupMuteCache`;
- `m_groupSoloCache`; and
- `m_groupGainDbCache`.

`AudioEngine::setGroupMute()`, `setGroupSolo()`, `setGroupGain()`, and
`applyGroupMixProfile()` update these arrays after SDK writes. The routing
inspector and operational snapshots then read the cached values through
`isGroupMuted()`, `isGroupSoloed()`, and `getGroupGainDb()`.

Observed consumers include:

- the routing-inspector projection in `Source/MainComponent.cpp`; and
- operational snapshot construction in `Source/MainComponent.cpp`.

The cache is currently necessary, but it creates two authorities for the same
routing controls. A change applied by scene recall, transport rehydration, or a
future SDK control path can leave the application projection stale.

---

## 3. SDK prerequisite

The Orpheus team must ship and package a public routing-state contract before
this handoff becomes actionable. The final SDK symbol names are intentionally
not prescribed here, but the contract must provide one coherent control-thread
value containing, for every configured group:

- group index or stable group identity;
- configured gain in decibels;
- configured mute flag;
- configured solo flag;
- output start and width, when those fields remain SDK-owned; and
- a revision or equivalent coherence marker.

The contract must distinguish a group's configured mute flag from effective
mute caused by another group's solo state. It must not require Clip Composer to
call the allocating scene-capture API on every UI poll.

For rollback-safe profile application, the SDK must also provide either:

1. one validated batch mutation that publishes all group controls atomically at
   a render boundary; or
2. a documented snapshot-restore transaction whose failure semantics prevent a
   partially applied live routing configuration.

The SDK prerequisite is complete only when all of the following are true:

- the contract is declared in installed public headers;
- `Orpheus::routing` and `Orpheus::transport` package consumers can use it
  without a private header or concrete-controller downcast;
- concurrent control/render behavior is covered under ThreadSanitizer;
- an SDK test proves coherent gain/mute/solo readback after mutation and
  rollback; and
- the SDK release or candidate version containing the contract is identified.

---

## 4. Clip Composer implementation handoff

After the SDK prerequisite ships, perform a clean application cutover:

1. Advance the Clip Composer SDK pin in a dedicated child-app change.
2. Add one `AudioEngine` routing-state query that projects the SDK value into the
   application-facing inspector shape.
3. Read one SDK snapshot per inspector/operational-snapshot refresh rather than
   issuing independent per-field reads.
4. Make `isGroupMuted()`, `isGroupSoloed()`, and `getGroupGainDb()` read from that
   SDK-derived projection.
5. Delete `m_groupMuteCache`, `m_groupSoloCache`, and `m_groupGainDbCache`.
6. Remove cache mutation and cache rollback from `setGroupMute()`,
   `setGroupSolo()`, `setGroupGain()`, and `applyGroupMixProfile()`.
7. Apply group profiles through the SDK's validated batch or documented
   transaction contract. Do not rebuild an app-local partial-rollback scheme.
8. Refresh routing state after transport recreation, device reconfiguration,
   scene recall, and profile application.
9. Keep all SDK routing reads and mutations on Clip Composer's existing
   message/control-thread ownership path.
10. Retain only application-owned presentation state locally.

The cutover must leave no compatibility cache, fallback read path, or private
SDK access in the application.

---

## 5. State that remains application-owned

This handoff does not move presentation or product policy into Orpheus. Clip
Composer continues to own:

- group display names and operator-facing output labels;
- routing-inspector rows, polling cadence, and JUCE view models;
- device-selection and dialog state;
- Cue/PFL workflow policy;
- session persistence and undo policy outside the SDK routing contract; and
- operator error presentation.

`m_groupOutputProfile` and `m_cueOutputBus` remain local unless a separately
approved SDK contract explicitly assumes ownership of those concepts.

---

## 6. Required application tests

Add or update observable application tests for these contracts:

1. **SDK readback:** accepted gain, mute, and solo writes are returned through
   the SDK-derived `AudioEngine` query.
2. **No shadow authority:** a routing change made through another supported SDK
   path is visible without calling the matching `AudioEngine::setGroup*()`
   wrapper first.
3. **Configured versus effective mute:** soloing one group does not overwrite
   another group's configured mute flag in the inspector projection.
4. **Atomic profile application:** a valid four-group profile appears as one
   coherent state; a rejected profile preserves the prior SDK and inspector
   state.
5. **Transport recreation:** device reconfiguration and transport replacement
   rehydrate the inspector from the new SDK controller rather than stale app
   memory.
6. **Scene/recall integration:** any supported SDK scene or routing restore is
   reflected in the next application snapshot.
7. **Audio behavior:** preserve the existing
   `PlaybackDispatcherTest.GroupBusesRouteMuteAndSoloIndependently` content
   assertions.
8. **Public consumption:** the application continues to build without private
   Orpheus headers or concrete transport downcasts.

Recommended focused verification after the child-app change:

```bash
cmake --build build-spoton-debug --target clip_composer_tests --parallel
ctest --test-dir build-spoton-debug --output-on-failure \
  -R 'GroupBusesRouteMuteAndSoloIndependently|Routing|DeviceSwap|SdkAdoption'
```

Run the repository's normal full Clip Composer suite and application smoke path
before advancing its SDK pin.

---

## 7. Acceptance criteria

The handoff is complete when:

- the Clip Composer repository resolves the identified SDK version;
- all routing inspector gain/mute/solo values originate from one coherent SDK
  state value;
- the three app shadow arrays and every write/rollback site for them are gone;
- profile failure cannot expose a partially applied group state;
- transport recreation and supported SDK restore paths cannot leave stale
  inspector state;
- focused routing, device-reconfiguration, SDK-adoption, full application, and
  smoke checks pass; and
- the child-app change records its tested SDK revision.

No Clip Composer source change, SDK-pin change, build result, or runtime result
is claimed by this document.

---

## 8. Non-goals

- Redesigning the routing inspector.
- Moving group names, Cue/PFL policy, or device UI into the SDK.
- Adding a general plugin or DSP graph.
- Changing the application's audio-thread ownership model.
- Prescribing the final SDK C++ symbol names before the Orpheus routing contract
  is reviewed.

---

## 9. Dispatch state

**Pass to:** Clip Composer team.  
**Start condition:** Orpheus publishes the prerequisite coherent routing-state
and rollback-safe batch contract.  
**Current action:** retain this handoff in the Clip Composer backlog; do not
implement against the current SDK surface.
