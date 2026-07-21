# ORP160 Master Tape Varispeed Primitive

**Status:** Released in Orpheus SDK `v0.6.3` (2026-07-21)

**Related:** FTR063, FTR062, ORP128/ORP129 (`ScrubResampler`), ORP127 (`PolyphaseResampler`)

## Scope

`PreparedTapeVarispeed` and `OfflineTapeVarispeed` are host-neutral interleaved
positive-rate tape processors for `0.5x...2.0x`. They preserve linked stereo
phase, move pitch and duration together, validate all public inputs, and use an
allocation-free prepared processing path. `ScrubResampler` remains signed,
linear, and monitor-only; it is not a fallback for this facility.

## Contract

The installed public API is `include/orpheus/tape_varispeed.h` in
`Orpheus::audio_utils`. `prepare()` owns allocation and fixed Kaiser-windowed
sinc-kernel preparation. `process()`, `drain()`, `reset()`, and accessors do
not allocate or acquire locks. Invalid configuration, rate, pointer, or prepared
capacity returns `InvalidParameter` without mutating processor state.

The caller owns source buffering. `TapeVarispeedProcessResult` reports exact
input consumption and output production so arbitrary block chunking can retain
unconsumed input. Unity with no active slew is a byte-identical copy path.

## Release evidence

PR [#222](https://github.com/chrislyons/orpheus-sdk/pull/222) passed the
four-case `tape_varispeed_test`, the clean-prefix public-package fixture, and
the static realtime audit before merge. The user completed the required manual
listening review and authorized the immutable release on 2026-07-21.

The release preserves the public `Orpheus::audio_utils` package route. FourTrack
may consume only the immutable `v0.6.3` tag, never the former feature-branch SHA.
