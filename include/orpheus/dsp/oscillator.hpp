#pragma once

/**
 * @file oscillator.hpp
 * @brief High-fidelity audio oscillator supporting multiple waveforms and unison.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <type_traits>

#include "orpheus/export.h"

namespace orpheus::dsp {

namespace detail {

constexpr double wrap_to_pi(double angle) noexcept {
  const double two_pi = 2.0 * std::numbers::pi_v<double>;
  while (angle > std::numbers::pi_v<double>) {
    angle -= two_pi;
  }
  while (angle < -std::numbers::pi_v<double>) {
    angle += two_pi;
  }
  return angle;
}

constexpr double sine_taylor(double angle) noexcept {
  const double x = wrap_to_pi(angle);
  const double x2 = x * x;
  const double x3 = x * x2;
  const double x5 = x3 * x2;
  const double x7 = x5 * x2;
  const double x9 = x7 * x2;
  return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0) + (x9 / 362880.0);
}

template <std::size_t Size> constexpr std::array<double, Size> make_sine_table() noexcept {
  std::array<double, Size> table{};
  for (std::size_t i = 0; i < Size; ++i) {
    const double phase =
        (static_cast<double>(i) / static_cast<double>(Size)) * 2.0 * std::numbers::pi_v<double>;
    table[i] = sine_taylor(phase);
  }
  return table;
}

} // namespace detail

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

class ORPHEUS_API AtomicDouble {
public:
  AtomicDouble() noexcept {
    storage_.store(std::bit_cast<std::uint64_t>(0.0), std::memory_order_relaxed);
  }

  explicit AtomicDouble(double value) noexcept {
    storage_.store(std::bit_cast<std::uint64_t>(value), std::memory_order_relaxed);
  }

  AtomicDouble(const AtomicDouble&) = delete;
  AtomicDouble& operator=(const AtomicDouble&) = delete;
  AtomicDouble(AtomicDouble&&) = delete;
  AtomicDouble& operator=(AtomicDouble&&) = delete;

  void store(double value, std::memory_order order = std::memory_order_relaxed) noexcept {
    storage_.store(std::bit_cast<std::uint64_t>(value), order);
  }

  [[nodiscard]] double load(std::memory_order order = std::memory_order_relaxed) const noexcept {
    const std::uint64_t bits = storage_.load(order);
    return std::bit_cast<double>(bits);
  }

  AtomicDouble& operator=(double value) noexcept {
    store(value);
    return *this;
  }

  operator double() const noexcept {
    return load();
  }

private:
  std::atomic<std::uint64_t> storage_;
};

class ORPHEUS_API AtomicBool {
public:
  AtomicBool() noexcept : storage_(0U) {}

  explicit AtomicBool(bool value) noexcept : storage_(value ? 1U : 0U) {}

  AtomicBool(const AtomicBool&) = delete;
  AtomicBool& operator=(const AtomicBool&) = delete;
  AtomicBool(AtomicBool&&) = delete;
  AtomicBool& operator=(AtomicBool&&) = delete;

  void store(bool value, std::memory_order order = std::memory_order_seq_cst) noexcept {
    storage_.store(value ? 1U : 0U, order);
  }

  [[nodiscard]] bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return storage_.load(order) != 0U;
  }

  bool exchange(bool desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    return storage_.exchange(desired ? 1U : 0U, order) != 0U;
  }

  bool compare_exchange_strong(bool& expected, bool desired,
                               std::memory_order success = std::memory_order_seq_cst,
                               std::memory_order failure = std::memory_order_seq_cst) noexcept {
    std::uint8_t expected_value = expected ? 1U : 0U;
    const bool result =
        storage_.compare_exchange_strong(expected_value, desired ? 1U : 0U, success, failure);
    expected = expected_value != 0U;
    return result;
  }

  bool compare_exchange_weak(bool& expected, bool desired,
                             std::memory_order success = std::memory_order_seq_cst,
                             std::memory_order failure = std::memory_order_seq_cst) noexcept {
    std::uint8_t expected_value = expected ? 1U : 0U;
    const bool result =
        storage_.compare_exchange_weak(expected_value, desired ? 1U : 0U, success, failure);
    expected = expected_value != 0U;
    return result;
  }

  AtomicBool& operator=(bool value) noexcept {
    store(value);
    return *this;
  }

  operator bool() const noexcept {
    return load();
  }

private:
  std::atomic<std::uint8_t> storage_;
};

template <typename Enum> class ORPHEUS_API AtomicEnum {
  static_assert(std::is_enum_v<Enum>, "AtomicEnum requires an enum type");

public:
  using underlying_type = std::underlying_type_t<Enum>;

  // Default-construct the atomic storage at run time to avoid MSVC warnings
  // about constexpr initialization of std::atomic.
  AtomicEnum() noexcept : storage_(static_cast<underlying_type>(Enum{})) {}

  explicit AtomicEnum(Enum value) noexcept : storage_(static_cast<underlying_type>(value)) {}

  AtomicEnum(const AtomicEnum&) = delete;
  AtomicEnum& operator=(const AtomicEnum&) = delete;
  AtomicEnum(AtomicEnum&&) = delete;
  AtomicEnum& operator=(AtomicEnum&&) = delete;

  void store(Enum value, std::memory_order order = std::memory_order_seq_cst) noexcept {
    storage_.store(static_cast<underlying_type>(value), order);
  }

  [[nodiscard]] Enum load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    return static_cast<Enum>(storage_.load(order));
  }

  Enum exchange(Enum desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    return static_cast<Enum>(storage_.exchange(static_cast<underlying_type>(desired), order));
  }

  bool compare_exchange_strong(Enum& expected, Enum desired,
                               std::memory_order success = std::memory_order_seq_cst,
                               std::memory_order failure = std::memory_order_seq_cst) noexcept {
    auto expected_value = static_cast<underlying_type>(expected);
    const bool result = storage_.compare_exchange_strong(
        expected_value, static_cast<underlying_type>(desired), success, failure);
    expected = static_cast<Enum>(expected_value);
    return result;
  }

  bool compare_exchange_weak(Enum& expected, Enum desired,
                             std::memory_order success = std::memory_order_seq_cst,
                             std::memory_order failure = std::memory_order_seq_cst) noexcept {
    auto expected_value = static_cast<underlying_type>(expected);
    const bool result = storage_.compare_exchange_weak(
        expected_value, static_cast<underlying_type>(desired), success, failure);
    expected = static_cast<Enum>(expected_value);
    return result;
  }

  AtomicEnum& operator=(Enum value) noexcept {
    store(value);
    return *this;
  }

  operator Enum() const noexcept {
    return load();
  }

private:
  std::atomic<underlying_type> storage_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

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
 */
class ORPHEUS_API Oscillator {
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

  [[nodiscard]] double render_voice(VoiceState& voice, Waveform waveform, double phase_increment,
                                    double pulse_width, double sub_increment,
                                    double& sub_mix) noexcept;

  void apply_phase_sync_if_needed() noexcept;

  [[nodiscard]] static double poly_blep(double t, double dt) noexcept;

  [[nodiscard]] static double wrap_phase(double phase) noexcept;

  [[nodiscard]] static double clamp(double value, double min, double max) noexcept;

  [[nodiscard]] double voice_detune(std::size_t voice_index) const noexcept;

  [[nodiscard]] static double detune_factor(double spread_cents, std::size_t voices,
                                            std::size_t voice_index) noexcept;

  static void advance_phase(double& phase, double increment) noexcept;

  template <typename T> static constexpr T lerp(T a, T b, T alpha) noexcept {
    return a + (b - a) * alpha;
  }

  static constexpr std::size_t kSineTableSize = 2048;

  // Inline variable to avoid ODR violations across translation units.
  inline static constexpr std::array<double, kSineTableSize> kSineTable =
      detail::make_sine_table<kSineTableSize>();

  [[nodiscard]] static double sine_from_table(double phase) noexcept;

  static constexpr std::size_t kVoiceAlignment = 64;
  alignas(kVoiceAlignment) std::array<VoiceState, kMaxVoices> voices_{};

  inline static constexpr double kDefaultSampleRate = 48'000.0;
  inline static constexpr double kDefaultFrequency = 440.0;
  inline static constexpr double kDefaultPulseWidth = 0.5;
  inline static constexpr double kDefaultDetuneCents = 12.0;

  AtomicDouble sample_rate_;
  AtomicDouble frequency_;
  AtomicDouble pulse_width_;
  AtomicDouble detune_cents_;
  std::atomic<std::size_t> voice_count_{1};
  AtomicBool sub_oscillator_{false};
  AtomicBool lfo_mode_{false};
  AtomicEnum<Waveform> waveform_{Waveform::Sine};
  AtomicDouble fm_depth_;
  AtomicBool phase_sync_pending_{false};
  AtomicDouble requested_phase_;
};

} // namespace orpheus::dsp
