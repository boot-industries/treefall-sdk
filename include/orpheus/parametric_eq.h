// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace orpheus {

/// Parametric EQ whose digital response matches the analog prototype of each
/// band. Every band is a trapezoidal state-variable filter (SVF) [2] whose
/// coefficients are fitted to the analog magnitude with the matched bilinear
/// method of [1]. Unlike a plain bilinear (cookbook) biquad, bell centres and
/// shelf midpoints stay on their frequency close to Nyquist.
///
/// [1] Y. Ivantsov, "On the ideal bilinear and biquadratic digital filter."
///     [Online]. Available: https://ivantsovy.com/research/paper1.pdf
/// [2] A. Simper, "Simultaneous solving of all outputs of linear SVF using
///     trapezoidal integration in state space form," Cytomic. [Online].
///     Available: https://cytomic.com/files/dsp/SvfLinearTrapAllOutputs.pdf

inline constexpr size_t kParametricEqMaxBands = 8;
inline constexpr size_t kParametricEqMaxChannels = 8;
/// A change applied with transitionTo() reaches its target in this time.
inline constexpr double kParametricEqTransitionSeconds = 0.010;
/// Highest Q accepted for a second-order LowShelf, HighShelf or TiltShelf.
inline constexpr float kParametricEqMaxShelfQ = 4.0f;

/// Band response families.
///
/// `frequencyHz` is the shelf midpoint for LowShelf and HighShelf (the
/// response is half the gain in dB there), the pivot for TiltShelf, and the
/// centre or cutoff for every other type. TiltShelf gives -gain/2 at DC and
/// +gain/2 at high frequencies.
enum class EqBandType : uint8_t {
  Bell,
  LowShelf,
  HighShelf,
  HighPass,
  LowPass,
  Notch,
  BandPass,
  AllPass,
  TiltShelf
};

/// Filter order. First (6 dB/oct) is accepted for LowShelf, HighShelf,
/// TiltShelf, HighPass and LowPass, and ignores `q`.
enum class EqBandOrder : uint8_t { First = 1, Second = 2 };

/// One band. `enabled` is the band bypass: a disabled band keeps every setting
/// and processes nothing.
///
/// Shelf `q` is the Q of the shelf's second-order section, as defined by the
/// Audio EQ Cookbook. 0.70710678 is the classic fixed shelf slope. A lower Q
/// widens the transition; a higher Q adds an overshoot below `frequencyHz`
/// and a dip above it. `gainDb` shapes only Bell, LowShelf, HighShelf and
/// TiltShelf, but is validated for every type.
struct EqBandSettings {
  bool enabled = false;
  EqBandType type = EqBandType::Bell;
  EqBandOrder order = EqBandOrder::Second;
  float frequencyHz = 1000.0f;
  float gainDb = 0.0f;
  float q = 0.70710678f;
  bool operator==(const EqBandSettings&) const = default;
};

/// Bands run in index order. `enabled` bypasses the whole EQ while every band
/// keeps its settings. The default settings are an exact identity.
struct ParametricEqSettings {
  bool enabled = false;
  std::array<EqBandSettings, kParametricEqMaxBands> bands{};
  bool operator==(const ParametricEqSettings&) const = default;
};

enum class ParametricEqValidationError : uint8_t {
  OK,
  InvalidSampleRate,
  InvalidChannelCount,
  InvalidBand,
  InvalidQuery
};

/// Validate every band, enabled or not, for a render rate in [8000, 384000]:
/// frequency in [20, 0.49 * sampleRate], gain in [-24, 24] dB, Q in [0.1, 20],
/// second-order shelf Q at most kParametricEqMaxShelfQ, and an order the type
/// accepts.
ParametricEqValidationError validateParametricEq(const ParametricEqSettings& settings,
                                                 double sampleRate) noexcept;

/// Control thread. Magnitude in dB of the full target cascade at each
/// frequency in [0, sampleRate / 2]. A bypassed EQ reports 0 dB everywhere.
/// The spans must have equal sizes and may overlap or alias (an in-place query
/// is allowed); on any error nothing is written.
ParametricEqValidationError parametricEqMagnitudeDb(const ParametricEqSettings& settings,
                                                    double sampleRate,
                                                    std::span<const float> frequenciesHz,
                                                    std::span<float> magnitudesDb) noexcept;

/// Prepared, fixed-capacity EQ for up to kParametricEqMaxChannels channels.
/// prepare() designs on the control thread and is failure-atomic.
/// transitionTo() adopts another prepared processor's settings while keeping
/// the filter state, ramping every band to its new response over
/// kParametricEqTransitionSeconds; it, processFrame() and active() are
/// allocation-free, lock-free and bounded.
class ParametricEqProcessor {
public:
  ParametricEqValidationError prepare(const ParametricEqSettings& settings, double sampleRate,
                                      size_t channelCount) noexcept;
  /// Precondition: `prepared` came from a successful prepare(). A different
  /// channel count or sample rate replaces this processor outright. A band
  /// whose pending ramp already heads to the requested response (including a
  /// bypass ramp-out) keeps that ramp rather than restarting it.
  void transitionTo(const ParametricEqProcessor& prepared) noexcept;
  void reset() noexcept;
  void processFrame(float* channels, size_t channelCount) noexcept;
  /// True while any band processes audio, including a band ramping out.
  bool active() const noexcept;

  const ParametricEqSettings& settings() const noexcept {
    return m_settings;
  }
  size_t channelCount() const noexcept {
    return m_channelCount;
  }

private:
  struct SvfParameters {
    float g = 1.0f;
    float k = 1.0f;
    float m0 = 1.0f;
    float m1x = 0.0f;
    float m2x = 0.0f;
    bool operator==(const SvfParameters&) const = default;
  };
  /// Per-sample SVF gains. The output mix is folded onto the input and the
  /// two integrator states, so the input-to-output path is one multiply-add.
  struct SvfCoefficients {
    float gt1 = 0.0f;
    float gk1 = 0.0f;
    float gt2 = 0.0f;
    float inputGain = 1.0f;
    float ic1Gain = 0.0f;
    float ic2Gain = 0.0f;
  };
  struct SvfState {
    float ic1 = 0.0f;
    float ic2 = 0.0f;
  };
  struct BandRuntime {
    SvfParameters target;
    SvfParameters current;
    SvfParameters step;
    SvfCoefficients coefficients;
    uint32_t framesRemaining = 0;
    bool enabled = false;
    bool running = false;
  };

  static SvfCoefficients deriveCoefficients(const SvfParameters& parameters) noexcept;
  void beginRamp(size_t band, const SvfParameters& target) noexcept;
  void advanceRamp(size_t band) noexcept;
  void stopBand(size_t band) noexcept;

  ParametricEqSettings m_settings{};
  std::array<BandRuntime, kParametricEqMaxBands> m_bands{};
  std::array<std::array<SvfState, kParametricEqMaxChannels>, kParametricEqMaxBands> m_state{};
  double m_sampleRate = 0.0;
  uint32_t m_transitionFrames = 0;
  size_t m_channelCount = 0;
};

} // namespace orpheus
