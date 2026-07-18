<!-- SPDX-License-Identifier: MIT -->

# ORP153 — Clip Composer Routing State Snapshot Adoption Handoff

**Document type:** Downstream adoption handoff  
**Owning implementation team:** Clip Composer  
**Issuing repository:** Orpheus SDK  
**Status:** SDK prerequisite and Clip Composer clean cutover delivered
**Date:** 2026-07-16  
**Related SDK direction:** [[ORP147 SDK Customer-Fit Gap Register and Incremental Build Guide]]
**SDK release:** `v0.6.0`

---

## 1. Purpose

This document hands one child-application task to the Clip Composer team: remove
its shadow copy of SDK routing gain, mute, and solo state by adopting the
coherent routing-state contract shipped in Orpheus SDK `v0.6.0`.

This is downstream adoption work. Orpheus owns the prerequisite public API,
realtime publication model, package fixture, and SDK contract tests. Clip
Composer owns its SDK pin, app-side migration, UI projection, and application
verification.

The prerequisite is now available through installed public headers. Clip
Composer can perform the clean cutover in §4.

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

Orpheus SDK `v0.6.0` ships the public routing-state contract required by this
handoff. `IRoutingMatrix::getRoutingControlSnapshot()` returns one coherent,
fixed-capacity control-thread value containing, for every configured group:

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

### 3.1 Delivered SDK contract

- `RoutingControlSnapshot` is a fixed-capacity, standard-layout,
  trivially-copyable public value with schema, group count, and monotonic
  revision.
- Each `RoutingGroupControlState` distinguishes configured mute from effective
  solo-derived mute and carries configured gain, solo, and output route.
- `IRoutingMatrix::applyGroupControlSnapshot()` validates the entire group set
  before mutation. Rejection preserves every prior field and the revision.
- Accepted batches publish to the render thread at one buffer boundary. The
  render thread copies a bounded value without waiting, allocation, locking, or
  I/O and retains the prior complete value if control publication overlaps its
  boundary.
- Installed `Orpheus::routing` and `Orpheus::transport` package fixtures compile,
  write, and read the contract through public interfaces.
- Routing unit coverage proves configured/effective projection, coherent batch
  publication, rejection rollback, preset recall, and concurrent render/query
  behavior. The concurrent contract passes under ThreadSanitizer.

### 3.2 Repository-wide CI baseline

The merged `main` run
[`29562306450`](https://github.com/chrislyons/orpheus-sdk/actions/runs/29562306450)
is red for repository-wide baseline defects outside ORP153:

- C++ lint reports existing clang-format violations across unrelated audio
  driver, scene, and channel-format files.
- Windows Debug and Release fail in `src/core/session/json_io.cpp` because the
  Windows `max` macro collides with `std::numeric_limits<...>::max()`.
- Ubuntu Release fails the ShmUI no-OpenGL consumer while compiling the shared
  UI package.

The ORP153 routing/package changes were verified separately before merge:
the routing and package targets passed locally, and the PR's macOS jobs passed.
These baseline failures remain documented rather than being folded into the
routing or Clip Composer adoption scope.

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

The SDK delivery claims above are verified in the Orpheus repository. Clip
Composer source, pin, build, and runtime results remain downstream work until
the application completes this handoff.

### 7.1 Downstream completion evidence

Clip Composer adopted SDK commit `2017741b40132c4bc27872e30b7b08019c1006a3`.
`AudioEngine` now reads one SDK `RoutingControlSnapshot`, applies group profiles
with `applyGroupControlSnapshot()`, rehydrates routing across transport
replacement, and keeps only group labels and device topology as app-owned
presentation state. The three routing shadow arrays and their rollback paths
are removed.

Focused routing, device-swap, scene-recall, external-mutation, configured versus
effective mute, atomic rollback, and playback behavior tests pass. The Clip
Composer full Debug suite passes with the CPU benchmark skipped by its existing
configuration.

---

## 8. Non-goals

- Redesigning the routing inspector.
- Moving group names, Cue/PFL policy, or device UI into the SDK.
- Adding a general plugin or DSP graph.
- Changing the application's audio-thread ownership model.
- Extending the contract beyond the reviewed fixed-capacity group-control
  surface.

---

## 9. Dispatch state

**Pass to:** Complete.
**Start condition:** satisfied by Orpheus SDK `v0.6.0`.
**Current action:** none; the downstream clean cutover and verification are
recorded in Clip Composer OCC169 and `PROGRESS.md`.
