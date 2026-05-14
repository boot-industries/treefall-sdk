---
source: standard-notes
sn_filename: "ORP020 Sprint Plan-04188a0e.txt"
prefix: orp
original_format: lexical
imported: 2026-05-01
status: archive
related:
  - cpp20_for_audio_dsp
  - cmake-googletest-ci
  - security_by_default_enables_production_confidence
---

⸻



Codex Cloud — PR 00: fix Windows exports, link wiring, and unresolved *_imp** symbols



Title: build(core): stabilize C ABI exports/imports on Windows and link all consumers



Intent

	•	Resolve LNK2019 __imp_orpheus_* errors by making the core’s C ABIs truly C-visible and producing a proper import library that minhost, tests, and adapters link against. ABI must remain C, versioned, and table-based.  ￼



Changes

	•	Add include/orpheus/export.h with ORPHEUS_API macro (__declspec(dllexport) when building DLL, __declspec(dllimport) when consuming; empty on Unix).

	•	Ensure all factory symbols (e.g., orpheus_session_abi_v1, orpheus_clipgrid_abi_v1, orpheus_render_abi_v1, orpheus_negotiate_abi) are declared extern "C" in headers and defined in a single TU in each core lib.

	•	In CMake:

	•	Build shared libs for each ABI target (e.g., orpheus_session, orpheus_clipgrid, orpheus_render) with WINDOWS_EXPORT_ALL_SYMBOLS OFF and explicit exports via headers (not link hacks).

	•	Emit an import library on Windows and link it into minhost, tests, and REAPER adapter targets with target_link_libraries(...).

	•	Add ORPHEUS_BUILDING_DLL to core targets; add ORPHEUS_USING_DLL to dependents on Windows.

	•	Add a tests/abi_link/ smoke that dlopen/LoadLibrary + resolves each factory.



Acceptance

	•	cmake -DORPHEUS_BUILD_SHARED=ON yields orpheus_*.(dll|so|dylib) + import libs on Windows; minhost/tests link cleanly (no LNK2019).

	•	On all OSes: ctest -R abi_link passes and prints resolved function pointers for session/clipgrid/render v1 tables.

	•	Symbols verified unmangled (e.g., dumpbin /exports shows orpheus_session_abi_v1).



Run



cmake -S . -B build -DORPHEUS_BUILD_SHARED=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build --config RelWithDebInfo -j

ctest --test-dir build --output-on-failure



(Why C ABI + negotiation? That’s our long-term stability contract and adapter boundary.  ￼)



⸻




# **PR 01: repo rebrand & host-neutral defaults**





**Title:** repo: rebrand to Orpheus SDK and set host-neutral defaults (adapter builds opt-in)



**Intent**



- Remove any impression that this is a “REAPER project.” Default build = core library + tests + minhost. All adapters OFF by default.
- Keep a REAPER adapter only as an optional compatibility module, clearly labeled and isolated.





**Changes**



- README: new positioning (“Orpheus SDK — host-neutral core + adapters”). Remove REAPER-centric phrasing.
- CMake:
- - New option ORPHEUS_ENABLE_ADAPTER_REAPER (default OFF).
- Top-level ENABLE_ADAPTERS section with per-adapter toggles; only minhost ON by default.
- Tree hygiene:
- - Move/confirm REAPER code under adapters/reaper/ only.
- Rename any reaper_* file names outside that folder.
- Namespaces/identifiers: ensure anything outside adapters/reaper/ uses orpheus::... and neutral terms (Session, ClipGrid, Render).
- Docs: add docs/ADAPTERS.md describing adapters as thin shims; REAPER listed as one of many.





**Acceptance**



- cmake -S . -B build && cmake --build build produces core + tests + minhost only.
- adapters/reaper/ not built unless -DORPHEUS_ENABLE_ADAPTER_REAPER=ON.
- README/ADAPTERS.md show host-neutral branding.






---






# **PR 02: licensing & provenance hardening (REUSE/SPDX, third-party)**





**Title:** legal: add LICENSES/THIRD_PARTY and REUSE/SPDX headers; vendor notices for adapters



**Intent**



- Respect upstream licenses (e.g., REAPER SDK, WDL if present) and make provenance explicit without centering branding.
- Adopt REUSE compliance to reduce legal ambiguity.





**Changes**



- Add LICENSE for Orpheus (choose your main license).
- Add LICENSES/ directory with unmodified texts for any third-party components in the repo.
- Add THIRD_PARTY.md mapping folders → license, version, upstream URL.
- Add file headers with SPDX tags where appropriate; add .reuse/ config.
- adapters/reaper/ gets a NOTICE explaining it’s an optional compatibility layer; restate upstream license references there.





**Acceptance**



- reuse lint passes locally (or minimal reported issues documented).
- CI job legal runs reuse lint and fails on regressions.






---






# **PR 03: unify C ABI negotiation & version tables (host-neutral contract)**





**Title:** core(abi): single negotiation pattern with stable v1 tables + caps



**Intent**



- One idiomatic negotiation entry per subsystem (session/clipgrid/render), returning stable v1 tables with major/minor/caps. This is the adapter boundary.





**Changes**



- include/orpheus/abi_version.h (macros, ORPHEUS_ABI_EXPECT(major), cap bits).
- For each subsystem expose orpheus_*_abi_v1(want_major, &got_major, &got_minor) -> const *_api_v1*.
- GTests for upgrade/downgrade, caps presence.





**Acceptance**



- Negotiation returns v1 tables across OSes; tests pass in CI.






---






# **PR 04: SessionGraph invariants + deterministic JSON/filenames**





**Title:** core(session): enforce clip invariants + JSON round-trip determinism + filename rules



**Intent**



- Lock down track/clip rules, tempo/transport metadata, and byte-stable session JSON + deterministic render filename scheme.





**Changes**



- Enforce min clip length, sorted items, session range tracking.
- Stable-order JSON writer; {project}_{stem}_{sr}k_{bd}b filename rule.
- Golden fixtures + round-trip tests.





**Acceptance**



- Round-trip tests show zero diff; filename tests pass.






---






# **PR 05: minimal multitrack render path (beyond click)**





**Title:** core(render): implement render_tracks() v1 (mono/stereo PCM WAV)



**Intent**



- Deliver a basic multitrack export so Orpheus stands on its own without any host.





**Changes**



- Implement render_tracks() honoring sample rate, bit depth, dither; simple stereo map [0,1].
- Reuse WAV writer used by click path; deterministic output naming.
- Tests: compare RMS/correlation against reference stems.





**Acceptance**



- minhost render --session s.json --tracks "DX,MUS,SFX" --out out/ produces WAVs; tests pass on all platforms.






---






# **PR 06: **


# **minhost**


# ** CLI UX and QA ergonomics**





**Title:** tool(minhost): subcommands {load, render-click, render-tracks, simulate-transport}



**Intent**



- Make host-independent workflows smooth; stable exit codes; nice diagnostics.





**Changes**



- Add subcommands/flags: --session,--spec,--tracks,--range,--sr,--bd.
- Pretty negotiation/caps printout; JSON error summary on --json.
- tools/minhost/README.md with examples.





**Acceptance**



- minhost --help lists commands; CI smoke runs across OSes.






---






# **PR 07: sovereign demo host (JUCE) to prove independence**





**Title:** host(demo): minimal JUCE app loading Orpheus via ABI (open, trigger, render)



**Intent**



- Demonstrate Orpheus without any third-party DAW. Menu: Open Session → Trigger ClipGrid scene → Render WAV.





**Changes**



- Tiny JUCE app with AudioDeviceManager; uses LoadLibrary/dlopen to fetch Orpheus tables.
- No branding beyond Orpheus; clean about box and demo disclaimer.





**Acceptance**



- App runs on Win/macOS; menu flow executes; renders to disk.






---






# **PR 08: isolate the REAPER adapter (opt-in, neutral wording, local notices)**





**Title:** adapter(reaper): isolate under opt-in flag, neutralize wording, add NOTICE



**Intent**



- Keep a compatibility adapter while de-emphasizing it. Clear opt-in build, neutral UI strings (“Orpheus Adapter”), and explicit licensing notes inside the adapter folder.





**Changes**



- Build only when -DORPHEUS_ENABLE_ADAPTER_REAPER=ON.
- Panel/action strings: “Orpheus Adapter: …” (no product co-branding).
- adapters/reaper/NOTICE describes scope and license mapping; README explains how it uses the public SDK APIs.
- No core logic lives in the adapter; it stays a thin shim.





**Acceptance**



- Default builds exclude this target; turning it on builds and runs with unchanged core.






---






# **PR 09: conformance fixtures + sanitizers + warnings matrix**





**Title:** ci(conformance): golden fixtures, ASan/UBSan on posix, MSVC /W4, artifact diffs



**Intent**



- Catch regressions early and document determinism.





**Changes**



- tools/conformance/ with JSON/WAV goldens + diff tooling.
- CI matrix runs unit + conformance + sanitizers (posix) and strict warnings (MSVC).
- Upload diff reports as artifacts.





**Acceptance**



- CI gates on these checks; artifacts appear on failure.






---






# **PR 10: SDK packaging (install/export + CPack ZIP) sans adapters by default**





**Title:** packaging(cmake): install headers/libs + OrpheusSDKConfig.cmake + CPack



**Intent**



- Ship an SDK bundle that is purely Orpheus core by default; adapters can ship separately.





**Changes**



- install(TARGETS ...), export config, install(DIRECTORY include/orpheus ...).
- cpack config for platform ZIPs; exclude adapters unless explicitly turned on.





**Acceptance**



- cmake --install build --prefix out/sdk && cpack yields a clean SDK bundle.






---






# **PR 11: ClipGrid v1 (slots/scenes, quantize, commit-to-arrangement)**





**Title:** core(clipgrid): v1 engine + quant windows + commit to linear arrangement



**Intent**



- Ship first non-linear engine piece, independent of any host.





**Changes**



- API for set_time_sig, set_quant, trigger/stop, commit_to_arrangement.
- Tests for launch timing (p99 jitter budget), commit correctness.





**Acceptance**



- tests/clipgrid_quant_test and commit tests pass.






---






# **PR 12: generic “MarkerSets” + “Playlist Lanes” (neutral terms), adapter hooks**





**Title:** core+adapter: add neutral MarkerSets/Playlist Lanes models + reaper hooks



**Intent**



- Introduce neutral, cross-host features (no brand terms). The REAPER adapter merely persists/recalls them via namespaced chunks, but semantics live in core.





**Changes**



- Core models: MarkerSet (scoped recall), PlaylistLane (take lanes/comping).
- Serialization in core session JSON; validation + undoable ops in core.
- In adapters/reaper/: namespaced chunk I/O and simple UI to reflect core state.





**Acceptance**



- Core tests cover create/list/recall/switch operations; adapter round-trips data when enabled.






---






# **PR 13: error codes, logging hooks, and telemetry opt-in (off by default)**





**Title:** core(common): unify error enums + logger callback + disabled telemetry hook



**Intent**



- Consistent errors and optional host-provided logging. Telemetry is opt-in and inert by default.





**Changes**



- include/orpheus/errors.h with enums + stringizer.
- orpheus_set_logger(callback, user_data); no PII media in any path.





**Acceptance**



- Errors stringify in CLI/demo; builds unchanged when logger unset.






---






# **PR 14: OTIO reconform skeleton (import/diff/plan)**





**Title:** core(otio): ReconformPlan JSON models + import/diff stubs + round-trip



**Intent**



- Stand up editorial reconform contracts; no heavy apply yet.





**Changes**



- ReconformPlan types + deterministic JSON.
- Tests for insert/delete/retime diff cases; fixture loader.





**Acceptance**



- Plans serialize deterministically; tests pass; demo host can preview planned ops.






---






# **PR 15: ADM entity graph skeleton + thinning policy**





**Title:** core(adm): models for programme/content/bed/object + writer stub + thinning



**Intent**



- Prepare for immersive deliverables; keep it minimal but correct.





**Changes**



- Entities + envelope types; JSON debug dump; thinning switch to reduce points while preserving semantics.





**Acceptance**



- Unit test builds a simple 5.1 bed + one object; thinning toggles validated.






---






## **Notes on execution order**





- **Branding & defaults first (PR 01–02)** so every subsequent artifact presents Orpheus as a host-neutral SDK, with adapters explicitly optional.
- **Contracts & determinism (PR 03–06)** establish a solid, testable core and CLI.
- **Sovereignty proof (PR 07)** demonstrates independence early.
- **Adapter isolation (PR 08)** keeps compatibility without brand bleed.
- **Quality gates & packaging (PR 09–10)** make regressions hard and distribution clean.
- **Feature momentum (PR 11–15)** advances non-linear, editorial, and immersive capabilities in neutral terms.
