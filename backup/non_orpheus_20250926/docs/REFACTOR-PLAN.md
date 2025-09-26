# Phase 1 Refactor Plan
## Inventory
- Modules: sdk, reaper-plugins, scripts
- Hotspots: reaper-plugins/roundtrip_latency.cpp
- Smells: long nested loops, unused headers
## Proposed Changes
- No-risk: remove unused headers and cache vector sizes in roundtrip latency utility
- Low-risk: replace manual cross-correlation loops with std::inner_product
- Requires review: audit broader plug-ins for duplication and header bloat
## Expected Impact
- Build/compile: slightly reduced header includes and potential faster rebuilds
- Runtime: inner_product may improve cross-correlation speed
## Test/CI Considerations
- configure and build via cmake -S . -B build && cmake --build build
