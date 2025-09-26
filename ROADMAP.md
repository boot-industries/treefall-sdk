# Roadmap

The Orpheus roadmap captures the near-term milestones for evolving the SDK and
its adapters.

## Milestone M1 – Foundations

* ✅ Quarantine legacy, non-Orpheus code and establish a clean repository.
* ✅ Create core ABI/session primitives with a modern CMake superbuild.
* ✅ Provide Hello World adapters (REAPER panel, minhost click renderer).
* ✅ Enable smoke testing, sanitizers, and cross-platform CI.

## Milestone M2 – Feature Expansion

* Extend the core library with richer session models (tempo, markers, media).
* Flesh out the REAPER adapter with interactive UI surfaces and message routing.
* Add audio rendering scenarios to the minhost (streamed audio, bus routing).
* Introduce integration tests that exercise host ↔ adapter handshakes.

## Milestone M3 – Production Readiness

* Harden ABI compatibility guarantees and publish migration guidelines.
* Ship official SDK packaging (binary + headers) for supported platforms.
* Expand CI with packaging, code coverage, and static analysis gates.
* Document extension points and authoring guides for partner teams.
