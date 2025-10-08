#include "orpheus/dsp/oscillator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>

namespace orpheus::dsp {
namespace {
constexpr double kMinSampleRate = 1.0;
constexpr double kMinFrequency = 0.01;
constexpr double kMaxPulseWidth = 0.995;
constexpr double kMinPulseWidth = 0.005;

class RandomEngine {
 public:
  RandomEngine() : engine_(std::random_device{}()), distribution_(-1.0, 1.0) {}

  [[nodiscard]] double white() noexcept { return distribution_(engine_); }

 private:
  std::mt19937 engine_;
  std::uniform_real_distribution<double> distribution_;
};

RandomEngine& random_engine() {
  thread_local RandomEngine engine;
  return engine;
}

}  // namespace

Oscillator::Oscillator() = default;

Oscillator::Oscillator(double sample_rate) { set_sample_rate(sample_rate); }

void Oscillator::set_sample_rate(double sample_rate) noexcept {
  sample_rate_.store(std::max(sample_rate, kMinSampleRate));
}

double Oscillator::sample_rate() const noexcept { return sample_rate_.load(); }

void Oscillator::set_frequency(double frequency_hz) noexcept {
  frequency_.store(std::max(frequency_hz, kMinFrequency));
}

double Oscillator::frequency() const noexcept { return frequency_.load(); }

void Oscillator::set_waveform(Waveform waveform) noexcept { waveform_.store(waveform); }

Waveform Oscillator::waveform() const noexcept { return waveform_.load(); }

void Oscillator::set_phase(double phase) noexcept {
  const double wrapped = wrap_phase(phase);
  requested_phase_.store(wrapped, std::memory_order_relaxed);
  phase_sync_pending_.store(true, std::memory_order_release);
}

void Oscillator::reset_phase() noexcept { set_phase(0.0); }

double Oscillator::phase(std::size_t voice) const noexcept {
  return voices_.at(std::min<std::size_t>(voice, kMaxVoices - 1)).phase;
}

void Oscillator::set_pulse_width(double width) noexcept {
  const double clamped = clamp(width, kMinPulseWidth, kMaxPulseWidth);
  pulse_width_.store(clamped);
}

double Oscillator::pulse_width() const noexcept { return pulse_width_.load(); }

void Oscillator::set_unison_voice_count(std::size_t voices) noexcept {
  voice_count_.store(std::clamp<std::size_t>(voices, 1, kMaxVoices));
}

std::size_t Oscillator::unison_voice_count() const noexcept { return voice_count_.load(); }

void Oscillator::set_unison_detune_cents(double cents) noexcept {
  detune_cents_.store(std::clamp(cents, 0.0, 1200.0));
}

double Oscillator::unison_detune_cents() const noexcept { return detune_cents_.load(); }

void Oscillator::enable_sub_oscillator(bool enabled) noexcept { sub_oscillator_.store(enabled); }

bool Oscillator::sub_oscillator_enabled() const noexcept { return sub_oscillator_.load(); }

void Oscillator::set_lfo_mode(bool enabled) noexcept { lfo_mode_.store(enabled); }

bool Oscillator::lfo_mode() const noexcept { return lfo_mode_.load(); }

void Oscillator::set_frequency_modulation_depth(double depth_ratio) noexcept {
  fm_depth_.store(std::max(0.0, depth_ratio));
}

double Oscillator::frequency_modulation_depth() const noexcept { return fm_depth_.load(); }

float Oscillator::process(float fm_input) noexcept {
  apply_phase_sync_if_needed();

  const double sr = std::max(sample_rate_.load(std::memory_order_relaxed), kMinSampleRate);
  const double base_freq = std::max(frequency_.load(std::memory_order_relaxed), kMinFrequency);
  const std::size_t voices = std::max<std::size_t>(1, voice_count_.load(std::memory_order_relaxed));
  const double pulse_width = pulse_width_.load(std::memory_order_relaxed);
  const double fm_depth = fm_depth_.load(std::memory_order_relaxed);
  const bool sub_enabled = sub_oscillator_.load(std::memory_order_relaxed);
  const auto waveform = waveform_.load(std::memory_order_relaxed);

  const double lfo_scale = lfo_mode_.load(std::memory_order_relaxed) ? 0.5 : 1.0;
  const double fm_ratio = std::clamp(static_cast<double>(fm_input), -1.0, 1.0) * fm_depth;
  const double fm_hz = base_freq * fm_ratio;
  const double detune_spread = detune_cents_.load(std::memory_order_relaxed);
  const double base_with_fm = base_freq + fm_hz;

  double sample_accumulator = 0.0;
  double sub_accumulator = 0.0;
  for (std::size_t v = 0; v < voices; ++v) {
    const double detune = detune_factor(detune_spread, voices, v);
    const double scaled_freq =
        std::max(base_with_fm * detune, kMinFrequency) * lfo_scale;
    const double phase_increment = scaled_freq / sr;

    auto& voice = voices_[v];
    advance_phase(voice.phase, phase_increment);
    const double sub_increment = std::max(scaled_freq * 0.5 / sr, 0.0);
    sample_accumulator +=
        render_voice(voice, waveform, phase_increment, pulse_width, sub_increment,
                     sub_accumulator);
  }

  double sample = sample_accumulator / static_cast<double>(voices);
  if (sub_enabled) {
    const double sub_sample = sub_accumulator / static_cast<double>(voices);
    sample = (sample + sub_sample) * 0.5;
  }

  return static_cast<float>(std::clamp(sample, -1.0, 1.0));
}

void Oscillator::process(std::span<float> output, float fm_input) noexcept {
  if (output.empty()) {
    return;
  }

  apply_phase_sync_if_needed();

  const double sr = std::max(sample_rate_.load(std::memory_order_relaxed), kMinSampleRate);
  const double base_freq = std::max(frequency_.load(std::memory_order_relaxed), kMinFrequency);
  const std::size_t voices = std::max<std::size_t>(1, voice_count_.load(std::memory_order_relaxed));
  const double pulse_width = pulse_width_.load(std::memory_order_relaxed);
  const double fm_depth = fm_depth_.load(std::memory_order_relaxed);
  const bool sub_enabled = sub_oscillator_.load(std::memory_order_relaxed);
  const double detune_spread = detune_cents_.load(std::memory_order_relaxed);
  const double lfo_scale = lfo_mode_.load(std::memory_order_relaxed) ? 0.5 : 1.0;

  const double fm_ratio = std::clamp(static_cast<double>(fm_input), -1.0, 1.0) * fm_depth;
  const double fm_hz = base_freq * fm_ratio;
  const double base_with_fm = base_freq + fm_hz;

  std::array<double, kMaxVoices> phase_increment{};
  std::array<double, kMaxVoices> sub_increment{};
  for (std::size_t v = 0; v < voices; ++v) {
    const double detune = detune_factor(detune_spread, voices, v);
    const double scaled_freq =
        std::max(base_with_fm * detune, kMinFrequency) * lfo_scale;
    phase_increment[v] = scaled_freq / sr;
    sub_increment[v] = std::max(scaled_freq * 0.5 / sr, 0.0);
  }

  for (auto& sample : output) {
    double sample_accumulator = 0.0;
    double sub_accumulator = 0.0;
    const auto waveform = waveform_.load(std::memory_order_relaxed);
    for (std::size_t v = 0; v < voices; ++v) {
      auto& voice = voices_[v];
      advance_phase(voice.phase, phase_increment[v]);
      sample_accumulator +=
          render_voice(voice, waveform, phase_increment[v], pulse_width,
                       sub_increment[v], sub_accumulator);
    }

    double mixed = sample_accumulator / static_cast<double>(voices);
    if (sub_enabled) {
      const double sub_sample = sub_accumulator / static_cast<double>(voices);
      mixed = (mixed + sub_sample) * 0.5;
    }

    sample = static_cast<float>(std::clamp(mixed, -1.0, 1.0));
  }
}

void Oscillator::apply_phase_sync_if_needed() noexcept {
  if (!phase_sync_pending_.load(std::memory_order_acquire)) {
    return;
  }

  const double target_phase = wrap_phase(requested_phase_.load(std::memory_order_relaxed));
  const double sub_phase = wrap_phase(target_phase * 0.5);
  const double triangle_seed = 4.0 * std::abs(target_phase - 0.5) - 1.0;
  for (auto& voice : voices_) {
    voice.phase = target_phase;
    voice.integrator = std::clamp(triangle_seed, -1.0, 1.0);
    voice.sub_phase = sub_phase;
    voice.dc = 0.0;
  }

  phase_sync_pending_.store(false, std::memory_order_release);
}

double Oscillator::render_voice(VoiceState& voice, Waveform waveform,
                                double phase_increment, double pulse_width,
                                double sub_increment, double& sub_mix) noexcept {
  double sample = 0.0;

  switch (waveform) {
    case Waveform::Sine: {
      sample = sine_from_table(voice.phase);
      break;
    }
    case Waveform::Saw: {
      const double naive = 2.0 * voice.phase - 1.0;
      const double blep = poly_blep(voice.phase, phase_increment);
      sample = naive - blep;
      break;
    }
    case Waveform::Square: {
      const double duty = 0.5;
      double square = voice.phase < duty ? 1.0 : -1.0;
      square += poly_blep(voice.phase, phase_increment);
      double t = voice.phase + (1.0 - duty);
      if (t >= 1.0) {
        t -= 1.0;
      }
      square -= poly_blep(t, phase_increment);
      sample = square;
      break;
    }
    case Waveform::Pulse: {
      const double duty = pulse_width;
      double pulse = voice.phase < duty ? 1.0 : -1.0;
      pulse += poly_blep(voice.phase, phase_increment);
      double t = voice.phase + (1.0 - duty);
      if (t >= 1.0) {
        t -= 1.0;
      }
      pulse -= poly_blep(t, phase_increment);
      sample = pulse;
      break;
    }
    case Waveform::Triangle: {
      double square = voice.phase < 0.5 ? 1.0 : -1.0;
      square += poly_blep(voice.phase, phase_increment);
      double t = voice.phase + 0.5;
      if (t >= 1.0) {
        t -= 1.0;
      }
      square -= poly_blep(t, phase_increment);
      voice.integrator += square * phase_increment * 4.0;
      if (voice.integrator > 1.0) {
        voice.integrator = 1.0 - (voice.integrator - 1.0);
      } else if (voice.integrator < -1.0) {
        voice.integrator = -1.0 - (voice.integrator + 1.0);
      }
      sample = std::clamp(voice.integrator, -1.0, 1.0);
      break;
    }
    case Waveform::WhiteNoise: {
      sample = random_engine().white();
      break;
    }
    case Waveform::PinkNoise: {
      const double white = random_engine().white();
      auto& state = voice.pink;
      state.b0 = 0.99765 * state.b0 + white * 0.0990460;
      state.b1 = 0.96300 * state.b1 + white * 0.2965164;
      state.b2 = 0.57000 * state.b2 + white * 1.0526913;
      const double pink = state.b0 + state.b1 + state.b2 + state.b3 + white * 0.1848;
      state.b3 = white * 0.5362;
      sample = pink * 0.05;
      break;
    }
  }

  advance_phase(voice.sub_phase, sub_increment);
  sub_mix += sine_from_table(voice.sub_phase);

  const double dc_alpha = 0.001;
  voice.dc += dc_alpha * (sample - voice.dc);
  sample -= voice.dc;

  return sample;
}

double Oscillator::poly_blep(double t, double dt) noexcept {
  if (dt <= 0.0) {
    return 0.0;
  }

  if (t < dt) {
    t /= dt;
    return t + t - t * t - 1.0;
  }

  if (t > 1.0 - dt) {
    t = (t - 1.0) / dt;
    return t * t + t + t + 1.0;
  }

  return 0.0;
}

double Oscillator::wrap_phase(double phase) noexcept {
  double wrapped = phase - std::floor(phase);
  if (wrapped < 0.0) {
    wrapped += 1.0;
  }
  return wrapped;
}

double Oscillator::clamp(double value, double min, double max) noexcept {
  return std::max(min, std::min(value, max));
}

double Oscillator::voice_detune(std::size_t voice_index) const noexcept {
  const auto voices = voice_count_.load(std::memory_order_relaxed);
  const double spread = detune_cents_.load(std::memory_order_relaxed);
  return detune_factor(spread, voices, voice_index);
}

double Oscillator::detune_factor(double spread_cents, std::size_t voices,
                                 std::size_t voice_index) noexcept {
  if (voices <= 1 || spread_cents <= 0.0) {
    return 1.0;
  }

  const std::size_t bounded_index = std::min<std::size_t>(voice_index, voices - 1);
  const double cents_per_voice = spread_cents / static_cast<double>(voices - 1);
  const double offset_cents =
      (static_cast<double>(bounded_index) * cents_per_voice) - spread_cents / 2.0;
  return std::pow(2.0, offset_cents / 1200.0);
}

void Oscillator::advance_phase(double& phase, double increment) noexcept {
  phase += increment;
  if (phase >= 1.0) {
    phase -= static_cast<double>(static_cast<std::uint32_t>(phase));
  } else if (phase < 0.0) {
    phase += 1.0;
  }
}

double Oscillator::sine_from_table(double phase) noexcept {
  const double wrapped_phase = wrap_phase(phase);
  const double index = wrapped_phase * static_cast<double>(kSineTableSize);
  const auto base_index = static_cast<std::size_t>(index) % kSineTableSize;
  const auto next_index = (base_index + 1) % kSineTableSize;
  const double frac = index - static_cast<double>(base_index);
  return lerp(kSineTable[base_index], kSineTable[next_index], frac);
}

}  // namespace orpheus::dsp

