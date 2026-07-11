// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

// Shared musical-timing constants and conversions.
//
// These helpers replace ad-hoc `60.0 / tempo_bpm` literals and per-file
// `kBeatsPerBar` definitions that were duplicated across the render ABI,
// track renderer, and host adapters. Keeping them in one place ensures the
// tempo math stays bit-identical everywhere (a determinism requirement).

namespace orpheus {

// Beats per bar assumed by the SDK's tempo model (4/4).
inline constexpr int kBeatsPerBar = 4;

// Seconds in one minute; tempo is expressed in beats per minute.
inline constexpr double kSecondsPerMinute = 60.0;

// Seconds elapsed per beat at the given tempo (beats per minute).
// Precondition: tempo_bpm > 0.
constexpr double secondsPerBeat(double tempo_bpm) {
  return kSecondsPerMinute / tempo_bpm;
}

// Fractional sample count spanning one beat at the given tempo and sample rate.
// Precondition: tempo_bpm > 0.
constexpr double samplesPerBeat(double tempo_bpm, double sample_rate_hz) {
  return sample_rate_hz * secondsPerBeat(tempo_bpm);
}

} // namespace orpheus
