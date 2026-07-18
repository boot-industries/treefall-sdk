# ORP155 - Bounded Per-Clip DSP and Output Telemetry

**Status:** implemented and release-qualified  
**Created:** 2026-07-18  
**Release:** Orpheus SDK `v0.6.2`  
**Downstream:** OCC164 Tranche 7

## 1. Delivery boundary

SDK `0.6.2` adds the reusable realtime contracts required before Clip Composer
may expose operator-rated clip processing:

- a public semantic `ClipDspProgram`;
- a prepared, fixed-capacity `ClipDspProcessor`;
- per-voice transport ownership and live metadata replacement;
- schema-versioned physical-output meter telemetry.

The SDK does not own Clip Composer persistence, UI, bulk commands, offline
render publication, or grid geometry. Those remain application contracts.

## 2. DSP contract

The processor order is fixed:

1. gate;
2. four independent EQ bands;
3. compressor;
4. stereo width;
5. limiter.

`validateClipDspProgram()` rejects non-finite or out-of-range values for the
concrete sample rate and channel count. `ClipDspProcessor::prepare()` is
failure-atomic: rejection preserves the previously prepared program and state.
Coefficient calculation occurs in `prepare()` on the control thread.
`processFrame()` uses only fixed arrays and scalar state; it performs no
allocation, locking, I/O, or coefficient calculation.

The all-disabled default is exactly transparent. Width is defined only for the
first stereo pair and leaves mono/additional lanes unchanged. Dynamics link
the configured source channels so image balance does not wander independently.

## 3. Transport integration

`ClipMetadata::dsp` is stored with each registered clip. Start-context creation
prepares a processor before posting the start command. Every active voice owns
its own processor state, so overlapping voices never share gate, EQ,
compressor, or limiter history.

A live `updateClipMetadata()` validates and prepares the replacement before
queue admission. Rejection changes neither registry metadata nor active voice
state. Acceptance publishes the prepared processor through the existing
single-producer command ring and replaces each matching voice at the audio
boundary. Voice-slot compaction copies the complete processor state with the
voice.

The DSP chain runs on the decoded source frame before the existing clip gain,
pan, source-channel policy, and routing stages. This preserves the established
transport gain/pan semantics while making the five DSP stages deterministic.

## 4. Output telemetry

`RealtimeTelemetrySnapshot` schema 2 carries `output_count` plus one
`AudioMeter` per configured physical output lane. Transport publishes the
routing matrix's post-master, post-protection lane meters in the same coherent
snapshot as group and master meters. Unconfigured and out-of-range lanes remain
silent.

## 5. Evidence

Release qualification evidence:

- `clip_dsp_test`: default transparency, validation rollback, EQ state, width,
  limiter-last order, fixed trivially-copyable state, and guarded full-chain
  realtime allocation behavior;
- `clip_controls_test`: metadata round-trip, failure atomicity, start-time DSP,
  and live active-voice replacement;
- `realtime_harness_test`: the enabled five-stage chain remains inside the
  existing file-backed no-I/O/no-allocation transport gate where the platform
  exposes process-I/O counters;
- `routing_matrix_test`: isolated physical-output meter publication;
- all 152 configured SDK tests passed after the version-contract correction;
- installed `find_package(OrpheusSDK 0.6.2 EXACT)` and runtime consumers passed,
  including the public `ClipDspProcessor` program.

## 6. Downstream adoption

Clip Composer must pin the immutable `v0.6.2` tag before adopting these APIs.
App work then owns staged session migration, Clip Edit controls, attribute-lock
and bulk/undo integration, bounded offline analysis/render publication, and
operator-facing diagnostics. The OCC164 Tranche 8 show playlist remains out of
scope.
