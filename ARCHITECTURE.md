# Architecture Overview

Orpheus is organised around a compact C++20 core library that exposes ABI
negotiation utilities and lightweight session serialisation helpers. Adapters
consume the library to integrate with host environments while smoke tests ensure
round-trip correctness.

## Core Library (`src/` + `include/`)

* `orpheus::AbiVersion` – Describes the SDK ABI and exposes negotiation helpers.
* `orpheus::SessionState` – Represents a minimal event log that can be
  serialised to and from a newline-delimited text format.
* The library is header-driven with a static archive target (`orpheus_core`).

## Adapters (`adapters/`)

* **REAPER (`reaper_orpheus`)** – Builds a shared library that exports extension
  metadata. A tiny panel helper renders the current ABI version and ready status
  for display inside the host UI.
* **Minhost (`orpheus_minhost`)** – Provides a CLI that can toggle transport
  playback, serialise the resulting session events, and render a click track to
  disk via a simple WAV writer.

Both adapters link against `orpheus_core`, inherit the global warning policy, and
are exercised in CI.

## Testing (`tests/`)

GoogleTest powers smoke coverage:

* ABI negotiation – Validates that major version mismatches gracefully fall back
  to the compiled ABI and that minor versions cap to supported values.
* Session round-trip – Ensures serialization retains ordering and ignores
  malformed input lines.

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
