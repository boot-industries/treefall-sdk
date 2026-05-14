---
related:
  - ORP
  - audio_processing_requires_determinism
  - single-writer-audio-state
  - audio_plugin_architecture_requires_thread_safety
---

# Orpheus SDK Gain Staging Model

**ORP121 A-06** | Last Updated: 2026-01-17

## Overview

This document defines the gain staging architecture for the Orpheus audio backend. The design follows broadcast industry standards (ST2110, ITU-R BS.775) and provides ~1528 dB of internal headroom with 32-bit float processing.

## Signal Flow

```
Source File (0 dBFS normalized)
       │
       ▼
┌──────────────────────────────────────────────────────────────────┐
│ FORMAT HANDLING (ORP121 A-01)                                    │
│                                                                  │
│ Mono:        Duplicate to L/R (phantom center)                   │
│ Stereo:      Direct L→L, R→R mapping                             │
│ Multi-ch:    ITU-R BS.775-3 downmix to stereo                    │
│              L_out = L + 0.707*C + 0.707*Ls                      │
│              R_out = R + 0.707*C + 0.707*Rs                      │
└──────────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Clip Gain            │  User setting: -inf to +12 dB
│ (clip.gainLinear)    │  Applied in: TransportController::processAudio()
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Fade Gain            │  Envelope: 0.0 to 1.0
│ (calculateFadeGain)  │  Types: FadeIn, FadeOut, RestartCrossfade, StopFade
│                      │  Curves: Linear, EqualPower, SCurve, Logarithmic
└──────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────────────────────────┐
│ ROUTING MATRIX INPUT (ST2110-aligned)                            │
│                                                                  │
│ Each clip outputs to 2 independent channels (L/R)                │
│ Channel index: clip * 2 + 0 = L, clip * 2 + 1 = R                │
│ Total routing channels: MAX_ACTIVE_CLIPS * 2 = 64                │
└──────────────────────────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Channel Gain         │  User setting: -inf to +12 dB
│ (channel.gain)       │  Applied in: RoutingMatrix::processRouting()
│                      │  Per-channel, smoothed (GainSmoother)
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Pan Law              │  Position: -1.0 to +1.0
│ (constant power)     │  ORP121 C-04: Constant-power pan law
│                      │  L/R coefficients: sqrt((1-pan)/2), sqrt((1+pan)/2)
│                      │  Center: L=R=0.707 (-3 dB each)
│                      │  Hard L: L=1.0, R=0.0
│                      │  Hard R: L=0.0, R=1.0
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Stereo Group Buffers │  ORP121 A-02: True stereo imaging
│ (StereoGroupBuffer)  │  Each group has L/R channel pair
│                      │  Additive summing (no auto-compensation)
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Group Gain           │  User setting: -inf to +12 dB
│ (group.gain)         │  Applied per-group before output bus
│                      │  Per-group, smoothed (GainSmoother)
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Output Bus Routing   │  ORP121 A-03: Configurable output buses
│ (group.output_bus)   │  Bus 0 = channels 0-1, Bus 1 = 2-3, etc.
│                      │  Groups sum into their assigned bus
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Master Gain          │  User setting: -inf to +12 dB
│ (master.gain)        │  Final output level control
│                      │  Smoothed (GainSmoother)
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Soft Limiter         │  ORP121 C-02: Soft-knee tanh limiter
│                      │  Threshold: -2 dBFS (0.794 linear)
│                      │  Knee width: 0.3
│                      │  Ceiling: -0.001 dBFS (0.9999 linear)
│                      │  C1 continuous (no audible discontinuity)
└──────────────────────┘
       │
       ▼
Output (broadcast-safe: -1.0 to +1.0)
```

## Gain Ranges

| Stage | Min | Max | Unit | Notes |
|-------|-----|-----|------|-------|
| Clip Gain | -inf | +12 | dB | Per-clip, precomputed linear |
| Channel Gain | -inf | +12 | dB | Per-routing-channel |
| Group Gain | -inf | +12 | dB | Per-group |
| Master Gain | -inf | +12 | dB | Global |
| Pan | -1.0 | +1.0 | position | Constant-power law |

## Headroom Analysis

### Internal Processing

32-bit IEEE 754 float provides:
- Maximum value: ~3.4×10³⁸
- Minimum positive: ~1.2×10⁻³⁸
- Dynamic range: ~1528 dB

**Conclusion:** Internal headroom is effectively unlimited. Gain staging is about signal quality, not overflow prevention.

### Clipping Points

Clipping occurs only at:

1. **Soft Limiter** (intentional, C1 continuous)
   - Threshold: -2 dBFS
   - Gradual compression above threshold
   - Hard ceiling: -0.001 dBFS

2. **DAC Output** (hardware)
   - Must stay within ±1.0
   - Protected by soft limiter

## Recommended Gain Structure

For broadcast/live use:

| Stage | Recommended | Rationale |
|-------|-------------|-----------|
| Source files | -18 dBFS RMS, -6 dBFS peak | EBU R128 alignment |
| Clip gains | 0 dB (unity) | Preserve source normalization |
| Channel gains | 0 dB (unity) | Default, adjust as needed |
| Group gains | -6 to 0 dB | Submix balance |
| Master gain | -3 to 0 dB | Final trim before limiter |

This structure provides ~12-18 dB of headroom at each summing stage.

## Smoothing

All gain controls use `GainSmoother` for click-free transitions:
- Default smoothing time: 10ms (configurable)
- Lock-free atomic target updates
- Linear ramping to prevent discontinuities

**Exception:** Clip-level fades use dedicated fade curves (not smoothing) to prevent artifacts with rapid clip triggers.

## Thread Safety

| Operation | Thread | Mechanism |
|-----------|--------|-----------|
| Gain setting | UI | Atomic write to pending target |
| Gain smoothing | Audio | Per-sample ramping from GainSmoother |
| Meter reading | UI | Atomic read from peak/RMS values |

All gain operations are lock-free and broadcast-safe.

## References

- ORP121: Audio Backend Refactoring Master Plan
- ITU-R BS.775-3: Multi-channel stereophonic sound system
- ST2110-30: Professional Media Over Managed IP Networks - PCM Digital Audio
- EBU R128: Loudness normalisation and permitted maximum level
