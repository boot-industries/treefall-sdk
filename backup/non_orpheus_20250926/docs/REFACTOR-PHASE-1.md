# Phase 1 Refactor Notes

## Summary by Directory
### reaper-plugins/
- Simplified roundtrip latency helper and removed unused headers.
- Replaced nested loops with `std::inner_product`.
- Cached ping size in loopback simulation and gated test-only `<cstdio>` include.

## Micro Benchmarks
- `find_ping_offset` (64 sample ping, 50ms delay, 1000 runs) avg **166.7µs → 161.9µs**.

## Phase 2 Follow-Ups
- Normalize WDL include paths across plug-ins.
- Explore broader header de-duplication.
