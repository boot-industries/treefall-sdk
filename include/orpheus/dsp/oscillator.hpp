#pragma once

/**
 * @file oscillator.hpp
 * @brief High-fidelity audio oscillator supporting multiple waveforms and unison.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

namespace orpheus::dsp {

/**
 * @brief Oscillator waveforms.
 */
enum class Waveform : std::uint8_t {
  /** Pure sine wave. */
  Sine = 0,
  /** Triangle wave generated via band-limited integration. */
  Triangle,
  /** Sawtooth wave with polyBLEP band-limiting. */
  Saw,
  /** Square wave with polyBLEP band-limiting. */
  Square,
  /** Variable pulse wave with polyBLEP band-limiting. */
  Pulse,
  /** White noise using a high-quality random generator. */
  WhiteNoise,
  /** Pink noise using a Paul Kellet filter. */
  PinkNoise
};

/**
 * @brief Modern, production-ready oscillator supporting real-time parameter automation.
 *
 * The oscillator is sample-rate agnostic and safe for real-time audio threads. Parameters
 * can be updated concurrently from control threads without locking. The class exposes both
 * scalar and span-based processing helpers.
 *
 * ### Usage Example
 * @code
 * orpheus::dsp::Oscillator osc;
 * osc.set_sample_rate(48000.0);
 * osc.set_waveform(orpheus::dsp::Waveform::Saw);
 * osc.set_frequency(220.0);
 * std::array<float, 512> buffer{};
 * osc.process(buffer);
 * @endcode
 */
class Oscillator {
 public:
  static constexpr std::size_t kMaxVoices = 8;

  /**
   * @brief Constructs an oscillator with a default sample rate of 48 kHz.
   */
  Oscillator();

  /**
   * @brief Constructs an oscillator with a custom sample rate.
   * @param sample_rate The initial sample rate in Hz.
   */
  explicit Oscillator(double sample_rate);

  /// @name Configuration
  ///@{
  /**
   * @brief Sets the processing sample rate.
   * @param sample_rate The new sample rate in Hz.
   */
  void set_sample_rate(double sample_rate) noexcept;

  /**
   * @brief Retrieves the current sample rate.
   */
  [[nodiscard]] double sample_rate() const noexcept;

  /**
   * @brief Sets the oscillator's fundamental frequency.
   * @param frequency_hz Frequency in Hertz. Values below 0.01 Hz are clamped.
   */
  void set_frequency(double frequency_hz) noexcept;

  /**
   * @brief Returns the current oscillator frequency in Hertz.
   */
  [[nodiscard]] double frequency() const noexcept;

  /**
   * @brief Selects the active waveform.
   * @param waveform The waveform to render.
   */
  void set_waveform(Waveform waveform) noexcept;

  /**
   * @brief Returns the currently active waveform.
   */
  [[nodiscard]] Waveform waveform() const noexcept;

  /**
   * @brief Sets the oscillator phase for all voices.
   * @param phase Phase in [0, 1) normalized turns.
   */
  void set_phase(double phase) noexcept;

  /**
   * @brief Resets all voices to phase zero.
   */
  void reset_phase() noexcept;

  /**
   * @brief Queries the phase of a specific voice.
   * @param voice Zero-based voice index.
   */
  [[nodiscard]] double phase(std::size_t voice = 0) const noexcept;

  /**
   * @brief Sets the pulse width for pulse-based waveforms.
   * @param width Duty cycle in the range [0, 1].
   */
  void set_pulse_width(double width) noexcept;

  /**
   * @brief Returns the current pulse width.
   */
  [[nodiscard]] double pulse_width() const noexcept;

  /**
   * @brief Sets the number of active unison voices.
   * @param voices Voice count in the range [1, kMaxVoices].
   */
  void set_unison_voice_count(std::size_t voices) noexcept;

  /**
   * @brief Returns the number of active unison voices.
   */
  [[nodiscard]] std::size_t unison_voice_count() const noexcept;

  /**
   * @brief Configures symmetric detune spread across unison voices.
   * @param cents Total spread in cents.
   */
  void set_unison_detune_cents(double cents) noexcept;

  /**
   * @brief Returns the configured detune spread in cents.
   */
  [[nodiscard]] double unison_detune_cents() const noexcept;

  /**
   * @brief Enables a built-in sub-oscillator one octave below.
   * @param enabled True to enable, false to disable.
   */
  void enable_sub_oscillator(bool enabled) noexcept;

  /**
   * @brief Reports whether the sub-oscillator is enabled.
   */
  [[nodiscard]] bool sub_oscillator_enabled() const noexcept;

  /**
   * @brief Enables a low-frequency-oscillator scaling mode.
   * @param enabled True for LFO scaling (0.5x frequency), false otherwise.
   */
  void set_lfo_mode(bool enabled) noexcept;

  /**
   * @brief Returns whether LFO mode is active.
   */
  [[nodiscard]] bool lfo_mode() const noexcept;

  /**
   * @brief Sets bipolar frequency modulation depth.
   * @param depth_ratio Depth as a ratio of the base frequency.
   */
  void set_frequency_modulation_depth(double depth_ratio) noexcept;

  /**
   * @brief Returns the modulation depth ratio.
   */
  [[nodiscard]] double frequency_modulation_depth() const noexcept;
  ///@}

  /// @name Processing
  ///@{
  /**
   * @brief Processes a single sample.
   * @param fm_input Optional normalized FM input in [-1, 1].
   * @return The rendered sample.
   */
  [[nodiscard]] float process(float fm_input = 0.0f) noexcept;

  /**
   * @brief Processes a span of samples in-place.
   * @param output Destination buffer.
   * @param fm_input Optional normalized FM input for every sample.
   */
  void process(std::span<float> output, float fm_input = 0.0f) noexcept;
  ///@}

 private:
  struct PinkState {
    double b0{};
    double b1{};
    double b2{};
    double b3{};
  };

  struct VoiceState {
    double phase{0.0};
    double integrator{0.0};
    double sub_phase{0.0};
    double dc{0.0};
    PinkState pink{};
  };

  [[nodiscard]] double render_voice(VoiceState& voice, Waveform waveform,
                                    double phase_increment, double pulse_width,
                                    double sub_increment, double& sub_mix) noexcept;

  void apply_phase_sync_if_needed() noexcept;

  [[nodiscard]] static double poly_blep(double t, double dt) noexcept;
  [[nodiscard]] static double wrap_phase(double phase) noexcept;
  [[nodiscard]] static double clamp(double value, double min, double max) noexcept;
  [[nodiscard]] double voice_detune(std::size_t voice_index) const noexcept;
  [[nodiscard]] static double detune_factor(double spread_cents, std::size_t voices,
                                            std::size_t voice_index) noexcept;
  static void advance_phase(double& phase, double increment) noexcept;

  template <typename T>
  static constexpr T lerp(T a, T b, T alpha) noexcept {
    return a + (b - a) * alpha;
  }

  static constexpr std::size_t kSineTableSize = 2048;
  static constexpr std::array<double, kSineTableSize> kSineTable = [] {
    std::array<double, kSineTableSize> table{};
    for (std::size_t i = 0; i < kSineTableSize; ++i) {
      table[i] =
          std::sin((static_cast<double>(i) / kSineTableSize) * 2.0 * std::numbers::pi);
    }
    return table;
  }();

  [[nodiscard]] static double sine_from_table(double phase) noexcept;

  static constexpr std::size_t kVoiceAlignment = 64;
  alignas(kVoiceAlignment) std::array<VoiceState, kMaxVoices> voices_{};

  std::atomic<double> sample_rate_{48000.0};
  std::atomic<double> frequency_{440.0};
  std::atomic<double> pulse_width_{0.5};
  std::atomic<double> detune_cents_{12.0};
  std::atomic<std::size_t> voice_count_{1};
  std::atomic<bool> sub_oscillator_{false};
  std::atomic<bool> lfo_mode_{false};
  std::atomic<Waveform> waveform_{Waveform::Sine};
  std::atomic<double> fm_depth_{0.0};
  std::atomic<bool> phase_sync_pending_{false};
  std::atomic<double> requested_phase_{0.0};
};

}  // namespace orpheus::dsp

