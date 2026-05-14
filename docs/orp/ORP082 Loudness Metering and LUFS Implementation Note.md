---
related:
  - audio_processing_requires_determinism
  - deterministic_testing_enables_audio_reproducibility
  - audio_plugin_architecture_requires_thread_safety
---

# ORP082 Loudness Metering and LUFS Implementation Note

## Summary
- Added reusable LUFS metering support to the Orpheus routing layer for downstream apps such as `freqfinder`.
- Extended public `AudioMeter` data with `short_term_lufs` and `integrated_lufs`.
- Kept existing peak, RMS, true-peak, clipping, and clip-count behavior intact while making `MeteringMode::LUFS` real instead of stubbed.

## Implementation
- Added `include/orpheus/loudness_meter.h` with a lightweight BS.1770-style loudness meter.
- Updated routing channel/group/master metering state to maintain loudness history and expose short-term/integrated LUFS values.
- Reset loudness state on routing resets and initialize loudness meters with the active sample rate.

## Validation
- `cd /Users/chrislyons/dev/orpheus-sdk && /opt/homebrew/bin/cmake -S . -B build`
- `cd /Users/chrislyons/dev/orpheus-sdk && /opt/homebrew/bin/cmake --build build --target loudness_meter_test routing_matrix_test --parallel 4`
- `cd /Users/chrislyons/dev/orpheus-sdk && /opt/homebrew/bin/ctest --test-dir build --output-on-failure -R '^(loudness_meter_test|routing_matrix_test)$'`

## Coverage Added
- Loudness reset returns the meter to silence.
- Louder input produces higher short-term and integrated loudness.
- Sustained signal populates both short-term and integrated LUFS.
- Routing-matrix LUFS mode reports the new fields through `getMasterMeter()`.
