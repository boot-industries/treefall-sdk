# ORP160 Master Tape Varispeed Primitive

**Status:** Draft implementation — FourTrack FTR063 contract accepted; release and merge require separate user review.

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

## Remaining release evidence

This draft does not yet satisfy the release gate. Before it can be merged or
published, ORP160 must add and pass: measurable endpoint passband/image tests,
chunk-invariance and drain fixtures, duplex reciprocal-slew contract coverage,
callback allocation/static-audit proof, clean-prefix package fixtures, and
44.1/48 kHz M2 timing plus user listening review. FourTrack must not consume this
branch or any unreleased SDK SHA.
