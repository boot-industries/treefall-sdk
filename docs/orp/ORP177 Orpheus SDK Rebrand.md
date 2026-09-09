## ORP177 ORPHEUS SDK REBRAND

## Current decision and acceptance — 2026-09-09

**Status: SDK implementation delivered; qualification and rebrand handoff pending.**
The selected public identities are **Treefall SDK** and **Treefall Suite**.
Canonical repository: `https://github.com/chrislyons/treefall-sdk`;
homepage: `https://treefall.dev`. Existing history and the physical SDK checkout
are retained. ORP180 is the documentation/community/harness strategy authority;
this synchronization does not implement that strategy or establish domain delivery.

`ORP-SUITE-20260909-001` adopts published
`0238eac1721d820d16ba5390e0e4641391be1d59` across the three consumers.
The local FLAC encoder-rate correction is separate and unmerged; it does not
advance that source identity. No reconciliation or ORP180 strategy commits
are included in the adoption pin, and no push or merge is authorized.

Preserved compatibility: `third_party/orpheus-sdk`, OrpheusSDK, Orpheus::,
orpheus::, existing C ABI exports, ORPHEUS_* configuration, suite IDs/schema,
generated tokens, app names, bundle/plugin IDs, and persisted paths. Additive
Treefall APIs coexist with them; **no legacy removal is scheduled**.
All C++ consumers must rebuild matching headers/libraries for appended
`ITransportController::getCommandIngressTelemetry`; old binaries are not evidence.

Local source handoffs now update ShmUI SDK references, both app submodule URLs
and pins, FourTrack Swift provenance/0.6.0 contract, and app About credits.
Their build/runtime and remote-publication acceptance remains separately pending
until the execution ledger supplies actual results. FreqFinder retains its old
Help handler: the intended Treefall manual route failed DNS resolution and must
serve the actual manual before migration. Homepage branding is not domain
deployment evidence. FourTrack's independent fourtrack.audio site is unchanged.

SDK-owned unresolved gates, each independent:

| Gate | Existing evidence | Requalification and scope |
|---|---|---|
| Hosted macOS callback progress | Run 34359783933: one callback against >5; full CTest 81/82. Local test now waits for six callbacks with a two-second deadline and passed the full CoreAudio fixture. | Hosted rerun of the same six-callback assertion at the eventual published revision; blocks hosted macOS full-suite qualification |
| Linux syscall attribution | Selected TID 3294 in `RealtimeHarnessTest.ConcurrentIngressAt96k64DoesNoIo`: CPU-online open/read/close plus anonymous mmap. These occur in whole-thread tracing around `std::barrier`; the harness now uses allocation-free atomic phase coordination and the focused test passed locally in 298 ms. | Preserve raw trace/positive control and rerun exact unsanitized Linux syscall gate; local Docker daemon was unavailable. Blocks zero-I/O qualification until Linux trace passes |
| Local capture initialization | Closed locally on BuiltInMicrophoneDevice → BuiltInSpeakerDevice, 44.1 kHz, strict stereo: 172 callbacks/88,064 frames, non-silent input, zero render/FIFO/conversion failures, Healthy route. | Re-run at eventual published revision; other device routes remain separately unqualified |

Raw Linux evidence is preserved under
`/tmp/treefall-suite-sync.m8YGjQ/evidence/linux-consumer-syscalls`;
ORP257 records exact files and attribution. No assertions or syscall allowances
were weakened. Dummy/hosted/silent capture does not close these gates.
Windows hardware promotion, ALSA, treefall-lint, ROS 2, and public release remain
out of scope. Sources: PR #258 and run 34359783933 in the canonical repository.

## Historical strategy narrative

The recommendations and placeholder choices below are historical; the current
decision above supersedes them, without rewriting their original rationale.

A rebrand is technically feasible. The audio engine, realtime guarantees, session model, and DSP architecture do not need to change.

However, a **full technical rename is not cosmetic**. “Orpheus” is embedded in:

- public C++ include paths and namespaces;
- the stable C ABI;
- CMake package and target names;
- library, executable, install, and archive names;
- release metadata and provenance;
- four downstream applications;
- ShmUI’s SDK mirror;
- app bundle IDs, plugin identity, and persisted application paths;
- suite coordination and documentation.

The safest recommendation is:

1. **Immediate display rebrand:** change visible product/repository/docs branding while preserving technical identifiers.
2. **Optional technical migration:** introduce new package/API names during a deliberate major compatibility window.
3. **Do not rename design-token or historical document identifiers automatically.**

## Repository decision: retain and rename the existing repository

The selected repository strategy is **Option A**: rename the existing GitHub
repository rather than create a successor repository.

This preserves the repository's commit history, branches, tags, pull requests,
issues, releases, and existing provenance references. It also keeps the SDK as
the suite's established system of record. A new repository would not remove the
technical API migration work; it would additionally require repository
configuration, release history, permissions, workflow, and provenance migration.

The repository operation is:

1. Rename the existing GitHub repository to the new canonical slug.
2. Update the SDK checkout's `origin` URL.
3. Update the canonical SDK URLs in downstream `.gitmodules` files.
4. Keep downstream local paths such as `third_party/orpheus-sdk` unchanged during
   the first transition; changing a local path is optional and creates needless
   build and documentation churn.
5. Perform display, package, and technical API migrations separately according to
   the compatibility policy below.

The repository rename must not be confused with a source/API rename. Renaming
the remote repository preserves Git history but does not preserve old C++,
CMake, or C ABI names if those identifiers are subsequently changed.

A new repository is reserved for a legal or ownership transfer, a security
boundary, a deliberate public-history reset, or a decision to terminate the old
product. If a new repository becomes necessary, it must be created as a full
history mirror rather than by copying the source tree, and the old repository
must remain an explicit compatibility or archival boundary.

The repository rename should precede package/API changes. This lets URL and
source-of-truth changes land independently from the later compatibility event.

---

## Repository operation and technical migration are separate

The repository rename is low-risk and primarily affects URLs and governance.
The technical migration remains the compatibility-sensitive work described
below:

| Operation | Preserves old technical names? | Compatibility consequence |
|---|---:|---|
| Rename existing repository | Yes | Existing source and binaries remain unaffected |
| Display rebrand | Yes | Visible strings and release presentation change |
| Package/build rebrand | Optional | `find_package` and build consumers may need updates |
| Full source/API rebrand | No | C++ source and binary/C ABI compatibility break |

The first release should therefore be able to carry the repository/display
rename without requiring a simultaneous namespace, include-path, or C ABI
cutover.

---

## Selected repository and migration policy

The selected strategy is an in-place repository rebrand with staged technical
migration:

- **Repository:** rename the existing `chrislyons/orpheus-sdk`; do not create a
  parallel successor repository.
- **History:** preserve commit, tag, release, and provenance history.
- **Consumer URLs:** update FourTrack and Clip Composer submodule URLs.
- **Consumer paths:** initially preserve `third_party/orpheus-sdk`.
- **Display identity:** migrate when the new brand is selected.
- **Technical identity:** preserve first, then migrate only through the chosen
  hard-cutover or compatibility-transition policy.
- **Application identity:** preserve bundle IDs, plugin identity, and persisted
  paths unless a separate suite-wide product migration is approved.

## Current coupling

| Surface | Current identity | Impact |
|---|---|---|
| Repository | `chrislyons/orpheus-sdk` | Git remotes, submodule URLs, documentation links |
| CMake project | `project(orpheus VERSION 0.8.0)` | Version tooling and generated metadata |
| CMake package | `OrpheusSDK` | `find_package(OrpheusSDK)` consumers |
| CMake targets | `Orpheus::core`, `Orpheus::transport`, etc. | All package and embedded consumers |
| Native targets | `orpheus_core`, `orpheus_audio_io`, `orpheus_session`, etc. | Build scripts, tests, installed exports |
| Headers | `include/orpheus/...` | 37 tracked public headers and every include site |
| C++ namespace | `namespace orpheus` | SDK implementation and downstream source |
| C ABI | `orpheus_session_abi_v1`, `orpheus_status`, `ORPHEUS_API` | C callers and dynamic loaders |
| Build options | `ORPHEUS_*` and `ORP_*` | Configure commands, presets, CI, docs |
| Install layout | `share/orpheus`, `lib/cmake/OrpheusSDK` | Installed-package discovery |
| Release artifacts | `orpheus-sdk-*.zip` | CPack, CI artifact globs, SBOM/provenance |
| In-tree apps | Orpheus Wave Finder / demo host | Product names, bundle metadata |
| Shared packages | `occ-app-platform`, `shmui-juce` | SDK-owned app infrastructure and JUCE mirror |
| Suite governance | `orpheus-suite`, ORP records | Cross-repository coordination |

The primary evidence is in the root build file, which defines the project name, `ORPHEUS_*` options, target exports, install layout, CMake package name, and CPack artifact name: `CMakeLists.txt:4-36`, `CMakeLists.txt:303-347`.

The public C ABI is explicitly branded in both type names and exported function names: `include/orpheus/abi.h:14-140`, `include/orpheus/abi_version.h:6-18`, and `include/orpheus/export.h:4-14`.

The release evidence generator also hardcodes the current project, package, SBOM, and provenance identity: `tools/generate_release_evidence.py:26-31`, `tools/generate_release_evidence.py:67-123`.

## The important compatibility distinction

There are three separate rebrands:

### 1. Display rebrand

Change:

- README titles and prose;
- application product names;
- workflow names;
- repository description;
- release presentation;
- visible About dialogs and marketing references.

Preserve:

- `include/orpheus`;
- `namespace orpheus`;
- `OrpheusSDK`;
- `Orpheus::...`;
- `orpheus_*` ABI symbols;
- existing bundle IDs and plugin identifiers.

This is low risk. It does not require downstream recompilation except where visible app metadata is changed.

### 2. Package/build rebrand

Change:

- repository slug;
- `find_package(NewBrandSDK)`;
- installed config filenames;
- CMake export namespace;
- install directories;
- CPack artifact names;
- package metadata.

Potentially preserve:

- public header paths;
- C++ namespace;
- C ABI;
- old CMake target aliases.

This is manageable, but every package consumer must update its discovery logic. FreqFinder currently resolves `find_package(OrpheusSDK)` and requires `Orpheus::core` and `Orpheus::audio_utils`: `freqfinder/CMakeLists.txt:39-71`.

### 3. Full source/API rebrand

Change:

- `include/orpheus` to a new include root;
- `namespace orpheus` to a new namespace;
- `orpheus_*` C types and functions;
- `ORPHEUS_*` macros;
- CMake variables and targets;
- libraries, executables, package names, and install paths.

This is feasible but is a **source and binary compatibility break**.

Renaming a C++ namespace changes mangled symbols. Renaming `orpheus_session_abi_v1` and its related C ABI symbols breaks existing dynamic clients even if the struct layouts remain identical. The current README calls the C ABI stable 1.0, and `abi_version.h` defines major version 1. A no-compatibility full rename should therefore be treated as a new ABI generation, likely ABI major 2.

## Downstream work required

### FourTrack

FourTrack embeds the SDK through `third_party/orpheus-sdk`, adds it with `add_subdirectory`, and links targets such as:

- `Orpheus::core`;
- `Orpheus::audio_utils`;
- `Orpheus::audio_io`;
- `Orpheus::routing`;
- `Orpheus::transport`;
- `Orpheus::diagnostics`.

Its CMake integration is in `fourtrack/CMakeLists.txt:90-116`.

The submodule URL is also explicit in `fourtrack/.gitmodules`. The SDK repository URL can change independently of the local submodule directory, so renaming the local directory is optional and not technically necessary.

FourTrack also has runtime identity values that should not be changed casually:

- `com.orpheus.fourtrack`;
- `com.orpheus.eighttrack`;
- `Orpheus/PortableRecorder`.

Those affect macOS bundle identity, single-instance coordination, and persisted application-support paths.

### Clip Composer

Clip Composer has the same embedded SDK model and uses the `Orpheus::` targets. It also has a separate SDK submodule URL in `.gitmodules`.

Its application identity includes:

- `Orpheus Clip Composer`;
- `com.orpheus.clipcomposer`;
- `OrpheusClipComposer` preference folders;
- `Orpheus Remote Media I/O Helper`.

The SDK rebrand should not automatically rename those app identities unless this is a full family rebrand.

### FreqFinder

FreqFinder uses the installed/source SDK package and has many direct C++ references to:

- `#include <orpheus/...>`;
- `orpheus::...`;
- `Orpheus::...`.

Its plugin metadata is particularly sensitive:

```cmake
COMPANY_NAME "OrpheusSDK"
BUNDLE_ID "com.orpheussdk.freqfinder"
PLUGIN_MANUFACTURER_CODE Orph
```

Changing the plugin bundle ID or manufacturer code can cause hosts to treat the renamed plugin as a different plugin. For an SDK-only rebrand, keep plugin identity stable and change only the dependency/package branding.

### ShmUI

ShmUI is a separate repository whose JUCE source is authoritative. The SDK contains a flattened mirror under `packages/shmui-juce`.

The source-of-truth CMake currently defines:

- `orpheus_shmui_juce`;
- `Orpheus::shmui_juce`;
- install headers under `include/orpheus/shmui-juce`.

Those changes must originate in ShmUI, then flow through its sync process into the SDK. The SDK mirror must not be hand-edited.

The generated `OrpheusDesignTokens.swift` and related token manifests are a separate design-system contract. A technical SDK rebrand does not require renaming them. Renaming them would be a ShmUI/design-system contract migration with generated-artifact and downstream provenance consequences.

## Recommended migration strategy

### Phase 0 — Decide the naming boundary

Create a naming matrix before editing:

| Decision | Example placeholder |
|---|---|
| Display name | `NewBrand SDK` |
| Repository slug | `newbrand-sdk` |
| CMake package | `NewBrandSDK` |
| Export namespace | `NewBrand::` |
| Include root | `newbrand/` |
| C++ namespace | `newbrand` |
| C ABI prefix | `newbrand_*` |
| Macro prefix | `NEWBRAND_*` |
| Install prefix | `share/newbrand` |
| Compatibility policy | hard cutover / dual support |
| App bundle IDs | preserve or migrate |
| Plugin identity | preserve |
| Document prefix | preserve `ORP` |

The key decision is whether “rebrand” means **new visible identity** or **remove every technical occurrence of Orpheus**.

### Phase 1 — Establish compatibility policy

Choose one:

#### Hard cutover

- Rename all technical identifiers.
- Require every consumer to migrate.
- Bump the C ABI major.
- Publish a clear migration guide.
- Rebuild all downstream applications.

Simpler long term, but existing external users break immediately.

#### Compatibility transition

Add the new names while retaining old names temporarily:

- new CMake package config plus old config;
- new target namespace plus old aliases;
- new include paths plus forwarding headers;
- new C ABI symbols plus old exported entry points;
- migration warnings where practical.

This is safer for unknown external consumers, but doubles the supported surface and requires a planned removal release.

Because this is a public SDK with a stable C ABI and unknown external consumers, the compatibility transition is the safer release strategy.

### Phase 2 — Rename the SDK technical surface

Update, in dependency order:

1. public include directory and internal include references;
2. C++ namespaces and qualified names;
3. C ABI structs, enums, macros, and factory symbols;
4. `ORPHEUS_*` build options;
5. native CMake target names;
6. exported `NewBrand::` aliases;
7. package config and version files;
8. install directories and metadata;
9. CPack filenames;
10. tests, fixtures, ABI loaders, and examples;
11. release evidence and CI artifact globs.

This should use syntax-aware refactoring for C++ symbols rather than an unrestricted text replacement. A raw global replacement would incorrectly affect:

- historical ORP document IDs;
- design-token identifiers;
- archived package names;
- application-specific namespaces;
- words where `orpheus` is part of a persisted path or external identity.

### Phase 3 — Update shared packages and consumers

Recommended order:

1. SDK source and package.
2. ShmUI source-of-truth CMake and package contract, if its target names change.
3. SDK’s synchronized ShmUI mirror.
4. FourTrack embedded SDK integration.
5. Clip Composer embedded SDK integration.
6. FreqFinder installed/source package integration.
7. suite manifest and cross-repository coordination metadata.

Each repository should retain an independently reviewable commit. Consumer submodule pins remain SHA-based, but URLs, paths, package names, CMake targets, and generated provenance may change.

### Phase 4 — Release and migration evidence

The SDK already has tests specifically covering the important boundaries:

- installed `find_package` consumption;
- runtime package consumption;
- `add_subdirectory` consumption;
- ABI linking;
- ShmUI package consumption;
- suite manifest validation;
- version contract validation.

Relevant registration is in `tests/CMakeLists.txt:133-155`, `tests/CMakeLists.txt:200-242`, `tests/CMakeLists.txt:271-284`, and `tests/CMakeLists.txt:286-300`.

The rebrand release should prove:

- a clean SDK build on supported platforms;
- full SDK CTest;
- installed package discovery using the new package name;
- embedded `add_subdirectory` consumption;
- C ABI negotiation and dynamic loading;
- correct install paths and target manifests;
- correct CPack artifact names;
- correct SBOM/provenance package identity;
- ShmUI mirror freshness;
- FourTrack build and tests;
- Clip Composer build and tests;
- FreqFinder source-override and installed-package builds;
- unchanged plugin/bundle identity where compatibility is intended.

## What should remain unchanged

Unless the goal is a total suite rebrand, preserve these:

- ORP document numbers and `docs/orp/` paths;
- FTR, FRQ, SHM, and OCC historical document identifiers;
- session and media file formats;
- plugin manufacturer and bundle identifiers;
- application-support directories;
- ShmUI design-token names and generated contract identifiers;
- realtime, routing, transport, and DSP behavior.

The ORP documents are durable cross-reference identifiers, not merely display text. Renaming them would create unnecessary link churn and destroy historical continuity.

The archived TypeScript names under the old `@orpheus` scope are not an active published SDK surface. They should remain clearly marked as historical rather than being included in the active technical migration.

## Overall outlook

| Scope | Feasibility | Compatibility risk |
|---|---:|---:|
| Visible/docs rebrand | Very high | Low |
| Existing repository rename only | Very high | Low to medium |
| New package/build identity with old API | High | Medium |
| Full source/API rename | High technically | High |
| Full rename with no compatibility layer | High technically | Very high for existing users |
| Full suite rebrand including apps, plugins, ShmUI, tokens, and persisted paths | Feasible | Very high and requires product-level migration planning |

The main engineering risk is not the rename itself. It is accidentally presenting a breaking ABI/package migration as a cosmetic brand update. A staged rebrand keeps the audio contracts intact, preserves downstream applications, and leaves the option of a clean technical cutover when the new brand and compatibility policy are settled.

## Treefall public Page

The initial public web presence for the rebranded SDK is deployed as a
Cloudflare Pages project:

- **Product:** Treefall SDK
- **Cloudflare Pages project:** `treefall-sdk`
- **Pages deployment:** `https://1afbcf54.treefall-sdk.pages.dev`
- **Custom domain:** `https://treefall.dev`
- **Production branch:** `main`
- **Deployment content:** a static SDK landing page describing Treefall as a
  host-neutral C++20 audio SDK and linking to the existing source repository.

The `treefall.dev` apex domain is managed in the same Cloudflare account and
was attached to the Pages project through the Pages custom-domain API. Cloudflare
reported the domain as `initializing` with HTTP validation pending at setup time;
DNS delegation already points to Cloudflare nameservers. The Pages deployment
URL remains the operational fallback while certificate and domain validation
complete.