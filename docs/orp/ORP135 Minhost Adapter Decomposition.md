# ORP135 Minhost Adapter Decomposition

**Status:** Design / Hand-off — approved for implementation, not yet started
**Author:** Cleanup sprint, 2026-07-10
**Scope:** `adapters/minhost/` — split the monolithic `main.cpp` into modules
**Severity:** Medium — maintainability; violates the ≤300 LOC adapter guideline
**Related:** ORP133/134 (realtime-safety hardening), the `adapters/shared`
`Orpheus::adapters_common` library introduced in the same cleanup sprint

---

## Context

`adapters/minhost/main.cpp` is a **single 1,532-line translation unit** — a
minimal standalone CLI host used for testing and offline rendering of Orpheus
sessions. `CLAUDE.md` sets a **≤300 LOC guideline for adapters**; minhost is 5×
over. The file mixes at least five responsibilities in one namespace:

- CLI argument parsing (global options + per-command flag parsing)
- JSON error/summary serialization
- ABI negotiation (now partly extracted — see below)
- Session preparation from a loaded `SessionGraph`
- Five command handlers: `load`, `render-tracks`, `render-click`,
  `simulate-transport`, plus dispatch in `Run()`/`main()`

This makes the adapter hard to navigate, hard to test in isolation, and a poor
reference for anyone writing a new host adapter.

**Why a design doc rather than an ad-hoc edit:** the split touches the one file
that the unity-include test (`tests/json_minhost_bridge_tests.cpp`,
`#include "../adapters/minhost/main.cpp"`) compiles directly, so the module
boundaries and the test's include strategy must be decided deliberately.

## Current structure (map)

Line anchors as of this doc (`adapters/minhost/main.cpp`, one `namespace minhost`):

| Concern | Symbols | Approx. lines |
|---|---|---|
| Error/summary I/O | `ErrorInfo`, `PrintJsonError`, `PrintError`, `JsonEscape`, `FormatNumber`, `FormatBeats` | 39–208, 464–491 |
| Scalar parsing | `ParseDouble`, `ParseUint32`, `ParseUint16`, `ParseRangeArgument`, `SplitCommaSeparated` | 97–164, 598–617 |
| ABI | `AbiContext`, `PrintNegotiationSummary`, `NegotiateApis`, `PrintAbiJson` | 208–259, 492–515 |
| Session prep | `SessionLoadOptions`, `SessionContext`, `PrepareSession`, `PrintSessionSummary`, `MergeSessionOptions`, `ClipIntersectsRange`, `ParseSessionCommonArg` | 260–463, 516–536, 618–659 |
| Help text | `PrintGlobalHelp` + 4 per-command help fns | 537–597 |
| `load` | `LoadCommandOptions`, `ParseLoadCommand`, `RunLoadCommand` | 660–768 |
| `render-tracks` | `RenderTracksCommandOptions`, `ParseRenderTracksCommand`, `RunRenderTracksCommand` | 769–918 |
| `render-click` | `ClickSpecOverrides`, `ParseClickSpecOverrides`, `RenderClickCommandOptions`, `Merge…`, `Parse…`, `BeatsToBars`, `ComputeBarsFromRange`, `RunRenderClickCommand` | 919–1220 |
| `simulate-transport` | `SimulateTransportCommandOptions`, `Parse…`, `RunTransportSimulation`, `Run…` | 1221–1320 |
| Dispatch | `ParsedCommand`, `ParseArguments`, `Run`, `main` | 1321–1532 |

### Already extracted (do not re-do)
- **ABI negotiation** null+version check is now `orpheus::adapters::NegotiateAbi<>`
  in `adapters/shared/abi_negotiation.h`. `NegotiateApis` already calls it.
- **Tempo math** (`secondsPerBeat`) is now `include/orpheus/music_timing.h`.
- **SessionGuard** lives in `adapters/shared/session_guard.h`.
- Both are reachable via the `Orpheus::adapters_common` INTERFACE target the
  minhost executable now links.

## Proposed target layout

Keep a single `namespace minhost`; split by concern into a small set of TUs
under `adapters/minhost/`. Each command handler owns its own options struct,
parser, and runner:

```
adapters/minhost/
  main.cpp                 # thin: main() + Run() dispatch only  (~120 LOC)
  cli_common.{h,cpp}       # ErrorInfo, JSON I/O, scalar parsers, help text
  session_prep.{h,cpp}     # SessionLoadOptions/Context, PrepareSession, summaries
  abi_context.{h,cpp}      # AbiContext, PrintNegotiationSummary, NegotiateApis, PrintAbiJson
  commands/
    load.{h,cpp}
    render_tracks.{h,cpp}
    render_click.{h,cpp}
    simulate_transport.{h,cpp}
```

Cross-cutting duplication to collapse **while** splitting (each appears 3–4×
across the parse functions today):

- **Flag-with-value parsing** (`--sr`, `--bd`, `--session`, `--tracks`,
  `--range`): extract `bool ParseValuedFlag(args, index, flag, handler, error)`
  into `cli_common`. Removes ~120 LOC of near-identical blocks.
- **JSON field extraction** in `ParseClickSpecOverrides` (8 near-identical
  `find(...) + type check + assign` blocks): extract
  `template<typename T> bool ExtractJsonField(obj, key, T& out)`.

## Build / test changes

- `adapters/minhost/CMakeLists.txt`: list the new sources; the target already
  links `Orpheus::core` + `Orpheus::adapters_common`.
- **Unity-include test:** `tests/json_minhost_bridge_tests.cpp` currently
  `#include`s `main.cpp` to reach `minhost::` internals. After the split it must
  instead compile against the new TUs. Two options — pick during implementation:
  1. Preferred: make an `orpheus_minhost_lib` OBJECT/STATIC library of the
     non-`main()` sources; both `orpheus_minhost` and `orpheus_tests` link it.
     The test then includes the relevant headers instead of `main.cpp`.
  2. Minimal: point the test's `#include` at the specific new TU(s) it exercises
     and add their dir to the test include path (already includes
     `adapters/shared`).

## Constraints

- **Behaviour-preserving.** No change to CLI surface, flags, exit codes, JSON
  output shape, or rendered bytes. minhost output is used by determinism tests.
- Formatting: `clang-format` (CI-enforced, husky pre-commit).
- Each new TU should land comfortably under the 300 LOC guideline.

## Verification

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
- `ctest --test-dir build --output-on-failure` — full suite must stay green
  (baseline: 134/134). `json_minhost_bridge` is the direct guard.
- Smoke parity on a fixture session — output must be byte-identical to
  pre-split for each command:
  - `orpheus_minhost load <session.json>`
  - `orpheus_minhost render-tracks <session.json> --out /tmp/rt.wav`
  - `orpheus_minhost render-click --tempo 120 --bars 4 --out /tmp/rc.wav`
  - `orpheus_minhost simulate-transport <session.json>`
- Debug build runs ASan+UBSan — must stay clean.

## Hand-off

Suitable for a build/adapter-focused implementation agent or session. Land on a
branch off `main` (`refactor/orp-minhost-split`) → PR. Do **not** commit to
`main` directly.
