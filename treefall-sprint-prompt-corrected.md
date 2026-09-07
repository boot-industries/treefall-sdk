# Orpheus SDK — Treefall Sprint Planning and Implementation

You are working on the Orpheus SDK. Use only the Whitebox checkout:

`/Users/nesbitt/dev/orpheus-sdk`

All repository, Git, build, test, and documentation commands must execute against that checkout. Do not use or modify another checkout.

## Operating rules

1. Read and follow:
   - `/Users/nesbitt/dev/orpheus-sdk/AGENTS.md`;
   - the repository's existing conventions; and
   - all required source documents below.

2. Read the current sprint records in this order before planning:
   1. ORP255 — parent strategy and sprint decomposition;
   2. ORP256 — Public Offline Seam and Codec Preflight;
   3. ORP257 — High-Concurrency Control Ingress;
   4. ORP177 — Treefall compatibility strategy.

3. Treat historical ORP records as evidence:
   - do not rewrite, erase, or reinterpret historical claims;
   - if a record needs an addendum, append a clearly labeled implementation or verification addendum;
   - reconcile contradictions explicitly; do not silently choose an interpretation;
   - distinguish document-recorded evidence from checks executed in this run.

4. Do not claim work, test results, synchronization, or evidence that was not directly observed.

5. Do not perform unrestricted textual replacement of `orpheus`. Historical ORP identifiers, persisted paths, plugin identity, design-token identifiers, and application-specific identifiers require deliberate treatment.

6. Use syntax-aware and symbol-aware tooling for C++ and C API changes. Before modifying an exported interface, inspect all in-repository references and callers. Do not rely on unrestricted global search-and-replace.

7. Do not modify downstream repositories or source-of-truth repositories, including:
   - FourTrack;
   - Clip Composer;
   - FreqFinder; and
   - ShmUI source repositories.

   Model downstream requirements with SDK-owned fixtures only.

8. ORP records under `/Users/nesbitt/dev/orpheus-sdk/docs/orp/` are local-only working records. They are intentionally ignored and must never be staged, committed, pushed, or used as GitHub synchronization payloads. Preserve the current local ORP256 and ORP257 records.

9. GitHub must remain free of `docs/orp/` records. Code, tests, package metadata, generated artifacts, and other non-ORP repository changes may still be committed normally.

## Required source documents

Read all of these before producing the plan:

1. `/Users/nesbitt/dev/orpheus-sdk/AGENTS.md`
2. `/Users/nesbitt/dev/orpheus-sdk/docs/orp/ORP255 Strategic Architecture, Competitive Posture, and Expansion Vectors.md`
3. `/Users/nesbitt/dev/orpheus-sdk/docs/orp/ORP256 Public Offline Seam and Codec Preflight.md`
4. `/Users/nesbitt/dev/orpheus-sdk/docs/orp/ORP257 High-Concurrency Control Ingress.md`
5. `/Users/nesbitt/dev/orpheus-sdk/docs/orp/ORP177 Orpheus SDK Rebrand.md`

ORP255 §8.1 and §8.2 define the strategic sprint objectives. ORP256 and ORP257 are the current detailed sprint planning contracts. Existing ShmUI segmented-meter, host-neutral metering, realtime telemetry, package, and ABI behavior is prior completed work and must be preserved, but it is not a deliverable of this sprint.

Reconcile ORP255's summary language with the detailed current ORP256 and ORP257 records. Preserve historical evidence. Do not reopen completed adjacent work merely because the current sprint records identify it as a compatibility constraint.

## Sprint scope

In scope:

- ORP255 §8.1 / ORP256 — Public Offline Seam and Codec Preflight;
- ORP255 §8.2 / ORP257 — High-Concurrency Control Ingress; and
- ORP177 — Treefall SDK rebrand under its compatibility-transition strategy.

Explicitly out of scope:

- ORP255 §8.3;
- ORP255 §8.4;
- Windows/WASAPI hardware promotion;
- Linux ALSA provider;
- `treefall-lint`;
- ROS 2 prototype;
- Windows or Linux platform-support promotion;
- downstream application changes;
- ShmUI source-of-truth changes;
- reopening completed segmented-meter or host-neutral metering implementation; and
- any Cloudflare, Wrangler, Pages, tunnel, or domain operation.

## Phase 1 — Read-only planning pass

This phase is strictly read-only with respect to the implementation checkout.

Do not:

- edit tracked files;
- edit or regenerate local ORP records;
- create or switch to an implementation branch;
- create or modify generated artifacts;
- commit;
- push;
- reset, rebase, stash, discard, or overwrite unexpected work;
- fast-forward or rewrite a branch; or
- implement code.

Reading ignored local ORP256 and ORP257 records is allowed. A clean tracked worktree may still contain those ignored local records.

### 1. Verify repository state

First verify the Whitebox checkout:

- current branch;
- tracked working-tree status;
- ignored local ORP-record status separately;
- upstream branch;
- synchronization with `origin/main`; and
- whether `origin/main` contains any `docs/orp/` paths.

Fetching remote refs is allowed for an accurate comparison. Do not discard unexpected work.

If tracked files are uncommitted or the checkout diverges from its upstream, report the exact state and do not conceal, reset, stash, or overwrite it. Do not classify ignored local ORP records as tracked-worktree divergence.

### 2. Audit before planning

Do not assume ORP256, ORP257, ORP255 §8.1, ORP255 §8.2, or ORP177 is unimplemented.

Determine:

- what is already implemented;
- what adjacent completed implementation must remain unchanged;
- what existing tests and fixtures already cover;
- what the current ORP256/ORP257 records require;
- what ORP records describe as completed versus merely planned;
- what remains open; and
- which requirements are incompatible, ambiguous, or already satisfied.

### 3. Produce the planning deliverable

Return a detailed plan with these sections, in this order:

#### A. Repository and source-document status

Report:

- checkout branch and tracked synchronization state;
- tracked working-tree state;
- local ignored ORP-record state;
- whether GitHub contains ORP records;
- documents read;
- relevant existing implementation and verification evidence; and
- the distinction between current observed evidence and historical ORP claims.

#### B. Scope ledger

For every ORP255 §8.1 / ORP256, ORP255 §8.2 / ORP257, and ORP177 requirement, classify it as:

- complete;
- partially complete;
- missing;
- blocked; or
- intentionally out of scope.

Cite exact files, symbols, tests, fixtures, and document sections. Treat existing ShmUI, routing-meter, telemetry, transport, settlement, ABI, and package behavior as adjacent preservation constraints rather than new sprint deliverables.

#### C. ORP reconciliation

Provide a table reconciling:

- ORP255 §8.1 and §8.2 summary requirements;
- ORP256 offline/preflight constraints;
- ORP257 MPSC/settlement constraints;
- ORP177 compatibility-transition strategy; and
- the implementation interpretation you will use.

Call out every conflict or ambiguity explicitly. Preserve historical serial-identity facts only as historical context; do not expand the sprint to revisit completed adjacent implementation.

#### D. Proposed implementation

For each required change, identify:

- exact files;
- exact symbols or targets;
- public API impact;
- caller/reference migration;
- tests and fixtures;
- package/install/export metadata;
- generated artifacts;
- local-only ORP addenda, if any;
- dependencies; and
- ordering constraints.

Do not propose speculative abstractions or unrelated cleanup. Do not stage or commit any `/docs/orp/` file.

#### E. Treefall naming and compatibility matrix

Cover each applicable surface:

- display name;
- repository identity;
- package identity;
- CMake project name;
- CMake package/config names;
- export names;
- public target names;
- compatibility targets;
- include paths;
- C++ namespace;
- C ABI names;
- C ABI macros;
- exported symbols;
- install paths;
- CPack/release artifact names;
- examples and tests;
- CI and release artifact globs;
- SBOM/provenance identifiers; and
- compatibility behavior for existing Orpheus consumers.

Use ORP177's documented compatibility-transition strategy by default. Do not silently choose a hard cutover. Do not rename ORP identifiers, local historical paths, design-token identifiers, bundle IDs, plugin manufacturer codes, or persisted application paths merely because the product display name becomes Treefall.

#### F. Complete migration map

Provide a caller/reference map for every exported interface affected.

Use symbol-aware references for C++ and C API changes. Include:

- definitions;
- declarations;
- implementations;
- re-exports;
- installed headers;
- package metadata;
- tests;
- examples;
- generated files;
- documentation references; and
- CI/release references.

#### G. Dependency and generated-artifact plan

Identify:

- dependency changes, if any;
- lockfile changes, if any;
- pinned FetchContent changes, if any;
- generated files;
- authoritative generators;
- required regeneration commands; and
- files that must not be hand-edited.

Treat the governed ShmUI package as an upstream-owned synchronized mirror. Do not hand-edit imported ShmUI files. Do not regenerate ShmUI artifacts unless a deliberate upstream import update is part of the approved scope.

#### H. Verification matrix

For every planned change, provide:

- command;
- working directory;
- prerequisites;
- expected evidence; and
- failure interpretation.

Include, as applicable:

- configured build;
- offline render behavior;
- deterministic hashes for 256, 512, 1024, and 2048 sample buffers;
- capability/preflight tests;
- MPSC stress tests with eight producers;
- 96 kHz / 64-frame audio consumption;
- TSAN;
- realtime harness;
- installed clean-prefix `cmake_find_package`;
- `realtime_static_audit`;
- `docs_path_audit`;
- ShmUI manifest check only when the governed package boundary is affected;
- pinned non-OpenGL ShmUI package consumer when applicable; and
- full configured CTest where practical.

Do not claim cross-platform determinism from a single macOS run. Do not claim race freedom from a non-TSan Debug run. Do not claim package synchronization from a manifest check alone.

#### I. Branch and commit plan

Use focused commits with messages in this format:

`type(scope): imperative description`

Plan commits so each logical implementation unit is independently reviewable and verifiable.

Rules:

- implementation changes occur only on `feat/treefall-sprint-8-1-8-2-rebrand`;
- do not commit implementation changes directly to `main`;
- do not push `main` for the implementation sprint;
- never include files under `docs/orp/` in a commit;
- local ORP addenda remain ignored working records; and
- push only the implementation feature branch after verification.

#### J. Risks, rollback, and unresolved decisions

Include:

- ABI/API risks;
- compatibility risks;
- generated-file risks;
- local-only documentation and remote-checkout risks;
- rollback boundaries;
- unresolved decisions requiring approval; and
- evidence prerequisites that cannot be obtained locally.

Explicitly distinguish:

- local ORP-record handling;
- GitHub branch synchronization; and
- implementation rollback.

#### K. Explicit out-of-scope confirmation

Confirm that ORP255 §8.3 and §8.4 will remain untouched, including:

- no Windows/WASAPI promotion;
- no Linux ALSA provider;
- no `treefall-lint`;
- no ROS 2 prototype;
- no Windows/Linux support promotion;
- no downstream repository changes;
- no ShmUI source-of-truth changes;
- no reopening completed adjacent metering work; and
- no Cloudflare/Wrangler/Pages/tunnel/domain operations.

End Phase 1 with exactly:

`PLAN COMPLETE — awaiting explicit approval before implementation.`

Do not begin Phase 2 in the same turn.

## Contract constraints

### ORP256 — Public Offline Seam and Codec Preflight

Plan and implement the missing portions of:

- a clean-prefix installed-package offline-render consumer under `tests/cmake/`;
- use of only documented public `Orpheus::` targets;
- deterministic golden render hashes for 256, 512, 1024, and 2048 sample buffers;
- cross-platform determinism where supported;
- public preflight capability APIs for supported file formats, sample rates, and bit depths;
- stale offline-renderer example updates or retirement; and
- preservation of realtime callback constraints.

Preserve these contract constraints:

- public installed consumers must not use private headers or source-only targets;
- capability queries must distinguish supported, unsupported, unavailable, invalid, and runtime-failure outcomes according to existing SDK error conventions;
- offline rendering remains outside the realtime callback boundary;
- no synchronous file/network I/O, logging, allocation, or unbounded work enters an audio callback; and
- existing governed ShmUI package, routing-meter, telemetry, and ABI behavior remains unchanged.

Required evidence:

- installed `cmake_find_package` fixture passes;
- offline output and golden hashes are deterministic;
- public capability queries are truthful before allocation or playback;
- stale examples are updated or retired with registrations and links removed;
- `realtime_static_audit` remains green; and
- installed public headers, exports, package metadata, and release evidence are consumer-usable.

### ORP257 — High-Concurrency Control Ingress

Plan and implement the missing portions of:

- a bounded, lock-free MPSC transport-command ingress;
- simultaneous UI, MIDI, OSC, and automation producers;
- no external producer mutex requirement;
- observable bounded saturation/drop counters;
- no heap allocation, blocking wait, OS I/O, or logging on the consumer/audio path;
- preservation of ORP252 tagged start outcomes;
- preservation of active-voice reconciliation; and
- correct ordering, overflow, failure, and settlement outcomes.

Preserve these contract constraints:

- queue capacity and overflow behavior are explicit and bounded;
- producers receive truthful admission/drop outcomes without a host mutex;
- saturating counters do not wrap;
- command ordering and settlement identity remain deterministic under concurrent producers;
- existing governed ShmUI package, schema-3 telemetry, routing-meter, transport, settlement, and ABI behavior remains unchanged; and
- any C++ ABI change is documented and all in-repository C++ consumers are rebuilt while stable C ABI 1.0 remains governed.

Required evidence:

- TSAN stress coverage with eight concurrent producer threads;
- audio consumer configured at 96 kHz and 64 frames;
- zero data races;
- zero deadlocks;
- zero consumer-side heap allocations;
- `realtime_harness_test` confirms memory-hook invariance and zero OS I/O; and
- existing transport, routing, telemetry, tagged-settlement, and active-voice tests remain valid.

### ORP177 — Treefall rebrand

Use the identity established by ORP177:

- product identity: `Treefall SDK`;
- public domain: `treefall.dev`; and
- compatibility-transition strategy by default.

Audit and migrate every applicable in-scope surface:

- visible SDK and README branding;
- CMake project/package/export metadata;
- public target names and compatibility targets;
- installed config/version files;
- include paths;
- namespaces;
- C ABI names and macros;
- exported symbols;
- install directories;
- CPack artifacts;
- examples;
- tests;
- release evidence;
- SBOM/provenance;
- CI artifact globs;
- package documentation; and
- dependency documentation.

Preserve unless ORP177 explicitly changes them:

- ORP identifiers and local historical paths;
- session formats;
- media formats;
- plugin manufacturer codes;
- plugin bundle identifiers;
- application-support paths;
- ShmUI design-token names;
- generated contract identifiers;
- realtime behavior;
- routing behavior;
- transport behavior;
- DSP behavior;
- downstream repositories; and
- local SDK paths.

## Phase 2 — Implementation after explicit approval

Begin only after explicit approval of the Phase 1 plan.

Before editing:

1. Re-verify the Whitebox tracked worktree is clean.
2. Confirm the local ORP256 and ORP257 records exist but remain ignored.
3. Fetch `origin/main`.
4. Fast-forward local `main` only if it is safely fast-forwardable.
5. Do not reset, rebase, discard, stash, or overwrite unexpected tracked work.
6. Create and use this feature branch:

   `feat/treefall-sprint-8-1-8-2-rebrand`

7. Do not commit directly to `main`.
8. Do not push to `main` for implementation changes.
9. If the feature branch already exists, do not overwrite or delete it; report the state and stop for instruction.
10. Never stage or commit files under `docs/orp/`.

### Implementation requirements

- Keep dependencies pinned and reproducible.
- Update `pnpm-lock.yaml` whenever `package.json` changes require it.
- Verify frozen-lockfile installation when applicable.
- Preserve pinned CMake FetchContent dependencies.
- Do not add dependencies merely to simplify implementation.
- Verify clean-prefix installation and consumer compilation.
- Verify package targets, config files, install paths, and generated metadata.
- If the governed ShmUI package boundary is affected:
  - use the manifest sync/check process;
  - run the pinned non-OpenGL package consumer; and
  - do not hand-edit imported package files.

### Local ORP documentation requirements

When implementation evidence is available, append clearly labeled addenda to the local-only ORP records as appropriate:

- ORP177;
- ORP255;
- ORP256; and
- ORP257.

Do not overwrite historical evidence. Do not stage, commit, or push these records. Addenda must contain:

- implementation decisions;
- compatibility consequences;
- changed contracts;
- exact observed verification evidence;
- remaining limitations; and
- explicit §8.3/§8.4 exclusions.

Update tracked changelog and release metadata only where repository conventions require it; keep those changes separate from local-only ORP records.

### Verification order

Run narrow behavioral checks first, then the relevant repository gates:

1. configured build;
2. offline render and capability checks;
3. MPSC/realtime/TSAN checks;
4. installed `cmake_find_package`;
5. `realtime_static_audit`;
6. `docs_path_audit`;
7. ShmUI manifest check when the governed package boundary is affected;
8. pinned non-OpenGL ShmUI package consumer when applicable; and
9. full configured CTest where practical.

Do not report a check as passing unless its output was directly observed. Do not report local-only ORP files as GitHub-synchronized artifacts.

### Delivery requirements

Before completion:

- push only the feature branch;
- leave Whitebox checked out on that feature branch;
- leave the tracked worktree clean;
- leave ORP256 and ORP257 local and ignored;
- commit each logical implementation unit;
- report the branch name;
- report every commit;
- report exact commands and observed results;
- report dependency changes;
- report every compatibility-surface change; and
- confirm ORP255 §8.3 and §8.4 were not implemented.

Never push `docs/orp/` records to GitHub.
