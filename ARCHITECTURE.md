<!-- SPDX-License-Identifier: MIT -->
# Architecture Overview

Orpheus is organised around a compact C++20 core library that exposes ABI
negotiation utilities and lightweight session serialisation helpers. Adapters
consume the library to integrate with host environments while smoke tests ensure
round-trip correctness.

## Core Library (`src/` + `include/`)

* `orpheus::AbiVersion` – Describes the SDK ABI and exposes negotiation helpers.
* `orpheus::core::SessionGraph` – Owns an in-memory project graph made of
  **tracks** (ordered containers for musical events) and **clips** (named spans of
  beats). The graph tracks tempo, transport state, and session metadata so hosts
  can query or mutate the arrangement in one place.
* `orpheus::core::session_json` – A helper namespace that serialises
  `SessionGraph` instances to and from the canonical JSON format used on disk.
  It also exposes filesystem helpers for loading/saving sessions and for
  generating filenames for rendered click tracks.
* The library is header-driven with a static archive target (`orpheus_core`).

## Adapters (`adapters/`)

* **REAPER (`reaper_orpheus`)** – Builds a shared library that exports extension
  metadata. A tiny panel helper renders the current ABI version and ready status
  for display inside the host UI.
* **Minhost (`orpheus_minhost`)** – Provides a CLI that imports session JSON into
  a `SessionGraph`, uses the public ABI tables to populate host-visible session
  handles, and then either renders a click track via the render ABI or runs a
  transport simulation that prints beat ticks to stdout.

Both adapters link against `orpheus_core`, inherit the global warning policy, and
are exercised in CI.

## Testing (`tests/` + `tools/`)

GoogleTest powers smoke coverage:

* ABI smoke (`tests/abi_smoke.cpp`) – Validates that major version mismatches
  gracefully fall back to the compiled ABI and that clip/grid helpers bind
  correctly.
* Session JSON round-trip (`tests/session_roundtrip.cpp`) – Loads fixtures into a
  `SessionGraph`, serialises them back to JSON, and ensures tracks, clips, and
  tempo survive losslessly.

Additional tooling mirrors the tests: `tools/conformance/json_roundtrip.cpp`
performs a full-file round-trip comparison to guard against accidental schema
drift.

The tests are run automatically through `ctest` and the GitHub Actions workflow.

## Tooling

* **CMake Superbuild** – Options expose fine-grained control over building the
  REAPER adapter, the minhost CLI, and the test suite.
* **Sanitisers** – AddressSanitizer and UBSan are injected for Debug builds on
  non-MSVC toolchains by default.
* **Formatting/Static Analysis** – `.clang-format` and `.clang-tidy` enforce code
  consistency.

## Legacy Content

Historic non-Orpheus assets remain quarantined beneath
`backup/non_orpheus_20250926/`. They are intentionally isolated from the active
build to simplify maintenance while still providing reference material.
