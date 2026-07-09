// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G2: time-domain primitives.
//
// The sample count is CANONICAL (the SDK rule: "64-bit sample counts, never
// float seconds"). Seconds, beats, and timecode are derived views computed on
// demand from an explicit sample rate / tempo — they are never stored, so a
// TimePoint cannot drift or double-round. The existing session graph's
// beats-only fields (start_beats/length_beats) stay untouched; these types
// are the additive sample-domain vocabulary new code should build on.
//
// All types are trivially copyable PODs, safe to embed in realtime messages.

#include <cstdint>

namespace orpheus {

/// Integer timecode (non-drop-frame). Derived view only.
struct Timecode {
  int32_t hours = 0;
  int32_t minutes = 0;
  int32_t seconds = 0;
  int32_t frames = 0; ///< At the fps supplied to TimePoint::toTimecode()

  friend constexpr bool operator==(const Timecode& a, const Timecode& b) {
    return a.hours == b.hours && a.minutes == b.minutes && a.seconds == b.seconds &&
           a.frames == b.frames;
  }
  friend constexpr bool operator!=(const Timecode& a, const Timecode& b) {
    return !(a == b);
  }
};

/// A point on a sample timeline. Canonical representation: int64 samples.
class TimePoint {
public:
  constexpr TimePoint() = default;

  // ---- construction ------------------------------------------------------

  static constexpr TimePoint fromSamples(int64_t samples) {
    return TimePoint(samples);
  }

  /// Derived-domain constructor; rounds to the nearest sample.
  static constexpr TimePoint fromSeconds(double seconds, uint32_t sampleRate) {
    return TimePoint(roundToSamples(seconds * static_cast<double>(sampleRate)));
  }

  /// Derived-domain constructor; rounds to the nearest sample.
  /// @param tempoBpm Beats per minute (> 0)
  static constexpr TimePoint fromBeats(double beats, double tempoBpm, uint32_t sampleRate) {
    const double seconds = tempoBpm > 0.0 ? (beats * 60.0 / tempoBpm) : 0.0;
    return fromSeconds(seconds, sampleRate);
  }

  // ---- canonical value ---------------------------------------------------

  constexpr int64_t samples() const {
    return m_samples;
  }

  // ---- derived views -----------------------------------------------------

  constexpr double toSeconds(uint32_t sampleRate) const {
    return sampleRate == 0 ? 0.0 : static_cast<double>(m_samples) / static_cast<double>(sampleRate);
  }

  constexpr double toBeats(double tempoBpm, uint32_t sampleRate) const {
    return toSeconds(sampleRate) * tempoBpm / 60.0;
  }

  /// Non-drop-frame timecode at an integer frame rate (24/25/30...).
  /// Integer math throughout; truncates within the final frame.
  constexpr Timecode toTimecode(uint32_t sampleRate, uint32_t framesPerSecond) const {
    Timecode tc;
    if (sampleRate == 0 || framesPerSecond == 0) {
      return tc;
    }
    const int64_t clamped = m_samples < 0 ? 0 : m_samples;
    const int64_t totalSeconds = clamped / static_cast<int64_t>(sampleRate);
    const int64_t remainderSamples = clamped % static_cast<int64_t>(sampleRate);
    tc.hours = static_cast<int32_t>(totalSeconds / 3600);
    tc.minutes = static_cast<int32_t>((totalSeconds / 60) % 60);
    tc.seconds = static_cast<int32_t>(totalSeconds % 60);
    tc.frames = static_cast<int32_t>((remainderSamples * static_cast<int64_t>(framesPerSecond)) /
                                     static_cast<int64_t>(sampleRate));
    return tc;
  }

  // ---- arithmetic / ordering --------------------------------------------

  friend constexpr TimePoint operator+(TimePoint p, int64_t deltaSamples) {
    return TimePoint(p.m_samples + deltaSamples);
  }
  friend constexpr TimePoint operator-(TimePoint p, int64_t deltaSamples) {
    return TimePoint(p.m_samples - deltaSamples);
  }
  /// Distance in samples.
  friend constexpr int64_t operator-(TimePoint a, TimePoint b) {
    return a.m_samples - b.m_samples;
  }

  friend constexpr bool operator==(TimePoint a, TimePoint b) {
    return a.m_samples == b.m_samples;
  }
  friend constexpr bool operator!=(TimePoint a, TimePoint b) {
    return a.m_samples != b.m_samples;
  }
  friend constexpr bool operator<(TimePoint a, TimePoint b) {
    return a.m_samples < b.m_samples;
  }
  friend constexpr bool operator>(TimePoint a, TimePoint b) {
    return a.m_samples > b.m_samples;
  }
  friend constexpr bool operator<=(TimePoint a, TimePoint b) {
    return a.m_samples <= b.m_samples;
  }
  friend constexpr bool operator>=(TimePoint a, TimePoint b) {
    return a.m_samples >= b.m_samples;
  }

private:
  constexpr explicit TimePoint(int64_t samples) : m_samples(samples) {}

  static constexpr int64_t roundToSamples(double value) {
    // constexpr-friendly round-half-away-from-zero (std::llround isn't
    // constexpr in C++20).
    return value >= 0.0 ? static_cast<int64_t>(value + 0.5) : -static_cast<int64_t>(-value + 0.5);
  }

  int64_t m_samples = 0;
};

/// Half-open sample range [start, end). Length is canonical alongside start;
/// end() is derived. Empty ranges (length 0) are valid.
class TimeRange {
public:
  constexpr TimeRange() = default;

  static constexpr TimeRange fromStartLength(TimePoint start, int64_t lengthSamples) {
    return TimeRange(start, lengthSamples < 0 ? 0 : lengthSamples);
  }

  static constexpr TimeRange fromStartEnd(TimePoint start, TimePoint end) {
    return fromStartLength(start, end - start);
  }

  constexpr TimePoint start() const {
    return m_start;
  }
  constexpr TimePoint end() const {
    return m_start + m_length;
  }
  constexpr int64_t length() const {
    return m_length;
  }
  constexpr bool empty() const {
    return m_length == 0;
  }

  /// Half-open containment: start() <= p < end().
  constexpr bool contains(TimePoint p) const {
    return p >= m_start && p < end();
  }

  constexpr bool overlaps(const TimeRange& other) const {
    return m_start < other.end() && other.m_start < end();
  }

  /// Intersection (empty range at the later start when disjoint).
  constexpr TimeRange intersect(const TimeRange& other) const {
    const TimePoint s = m_start > other.m_start ? m_start : other.m_start;
    const TimePoint e = end() < other.end() ? end() : other.end();
    return e > s ? fromStartEnd(s, e) : fromStartLength(s, 0);
  }

  friend constexpr bool operator==(const TimeRange& a, const TimeRange& b) {
    return a.m_start == b.m_start && a.m_length == b.m_length;
  }
  friend constexpr bool operator!=(const TimeRange& a, const TimeRange& b) {
    return !(a == b);
  }

private:
  constexpr TimeRange(TimePoint start, int64_t length) : m_start(start), m_length(length) {}

  TimePoint m_start;
  int64_t m_length = 0;
};

} // namespace orpheus
