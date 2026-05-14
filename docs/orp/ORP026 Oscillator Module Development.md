---
source: standard-notes
sn_filename: "ORP026 Oscillator Module Development-e7dc514c.txt"
prefix: orp
original_format: lexical
imported: 2026-05-01
status: archive
related:
  - audio_processing_requires_determinism
  - cpp20_for_audio_dsp
  - deterministic-audio-processing
  - cmake-googletest-ci
---

# Oscillator Module Development

**Context:** I'm working on the Orpheus SDK, a modern C++20 audio processing framework with a host-neutral core. The SDK follows these principles:

- Cross-platform (Windows/macOS/Linux) via CMake 3.20+
- C++20 standard with sanitizers (ASan/UBSan) enabled
- GoogleTest for unit testing
- Strict code quality (clang-format, clang-tidy compliance)
- Modular, header-focused design for easy integration

**Task:** Design and implement a production-ready oscillator module (`orpheus::dsp::Oscillator`) with the following specifications:


## Core Requirements

**Wave Shapes** (all alias-suppressed where applicable):

- Sine (pure analytical)
- Triangle (band-limited or polyBLEP)
- Sawtooth (band-limited or polyBLEP)
- Square/Pulse (band-limited, variable pulse width 0-100%)
- White Noise
- Pink Noise (optional but preferred)

**API Design:**

- Sample-rate agnostic initialization
- Per-sample `process()` method returning `float` or `double`
- Thread-safe parameter updates (frequency, phase, waveform, pulse width)
- Phase reset capability
- Unison/detune support for richer sounds (2-8 voices)

**Modern C++ Practices (2025-2026):**

- Use `std::span`, `std::optional`, and C++20 concepts where appropriate
- Constexpr evaluation for lookup table generation
- SIMD-friendly memory layout (consider `std::hardware_destructive_interference_size`)
- Move semantics and perfect forwarding
- No raw pointers; prefer smart pointers or references
- Modules support (if feasible for C++20)

**Performance:**

- Minimize branching in the audio thread
- Use wavetable synthesis for complex waveforms with linear/cubic interpolation
- Optimize for cache coherency
- Consider SIMD intrinsics or auto-vectorization hints

**Project Integration:**


```




```

**Testing Requirements:**

- Frequency accuracy validation (compare expected vs actual zero-crossings)
- DC offset verification (should be minimal)
- Harmonic content analysis for each waveform
- Thread-safety tests
- Performance benchmarks (samples/second throughput)

**Documentation:**

- Doxygen-style comments for all public APIs
- Include usage example in header
- Brief ALGORITHMS.md explaining band-limiting approach

**Constraints:**

- Header-only preferred, but separate .cpp if implementation is substantial
- No external dependencies beyond standard library
- Compatible with existing CMake structure
- Must pass `-Werror -Wall -Wextra -Wpedantic`

**Bonus Features** (if elegant to include):

- FM modulation input
- Hard/soft sync
- Sub-oscillator (octave down)
- Built-in LFO mode (0.01 Hz - 20 Hz range)

**Code Style:** Follow the Orpheus conventions (inferred from README: modular, portable, automation-friendly). Assume `.clang-format` uses a modern style (likely LLVM or Google variant).

Deliver a complete, production-ready implementation with elegant topology—favoring clarity and maintainability while achieving professional audio quality. The oscillator should feel like a reference implementation that other developers would study.
