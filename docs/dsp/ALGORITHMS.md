# Oscillator Band-Limiting Overview

The Orpheus oscillator relies on a lightweight, allocation-free signal chain designed for
real-time audio rendering. This document summarises the techniques used to suppress aliasing
and maintain spectral parity across waveforms.

## PolyBLEP Edge Correction

Saw, square, and variable pulse shapes use a polyBLEP (polynomial band-limited step)
compensation term. Each time the waveform introduces a discontinuity the oscillator blends
an analytic correction curve across the transition. This removes the infinite-bandwidth step
from the naïve shape while retaining the closed-form waveform definition. The implementation
follows the formulation outlined by Valimaki and Huovilainen (2007) and is evaluated per
voice without additional lookup tables.

### Triangle Waves via Integrated Square

Triangle rendering integrates the band-limited square response using a leaky integrator. By
recycling the already anti-aliased square, the resulting triangle maintains the expected 12 dB
per-octave roll-off while avoiding numerical drift.

## Noise Sources

White noise samples originate from a thread-local Mersenne Twister seeded via `std::random_device`.
Pink noise applies the Paul Kellet six-stage filter, producing a 3 dB/octave spectral slope with
minimal CPU overhead.

## Unison and Sub Oscillator

Detune factors are computed analytically by spreading voices evenly across the requested cent
range. Each voice maintains an independent phase accumulator, enabling deterministic
polyphony without heap allocations. The optional sub-oscillator tracks the primary voice at
half frequency using the shared sine wavetable for phase-coherent layering.

## SIMD and Cache Behaviour

Voice state resides in a fixed-size, cache-aligned array sized to 64 bytes, matching typical
destructive interference thresholds. This ensures voice data is colocated and friendly to CPU prefetchers. The sine wavetable is
constexpr-initialised and served via linear interpolation to reduce expensive transcendental
calls within tight loops.

