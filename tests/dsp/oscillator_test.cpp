#include <gtest/gtest.h>

#include "orpheus/dsp/oscillator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <numbers>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

namespace {
constexpr std::size_t kTestBlock = 48000;
constexpr double kSampleRate = 48000.0;

std::vector<float> render_buffer(orpheus::dsp::Oscillator& osc, std::size_t frames) {
  std::vector<float> buffer(frames);
  osc.process(buffer);
  return buffer;
}

double measure_frequency(const std::vector<float>& buffer, double sample_rate) {
  std::size_t zero_crossings = 0;
  for (std::size_t i = 1; i < buffer.size(); ++i) {
    if (buffer[i - 1] <= 0.0f && buffer[i] > 0.0f) {
      ++zero_crossings;
    }
  }
  if (zero_crossings < 2) {
    return 0.0;
  }
  const double periods = static_cast<double>(zero_crossings);
  const double duration = static_cast<double>(buffer.size() - 1) / sample_rate;
  return periods / duration;
}

double dc_offset(const std::vector<float>& buffer) {
  const double sum = std::accumulate(buffer.begin(), buffer.end(), 0.0);
  return sum / static_cast<double>(buffer.size());
}

double magnitude_at(const std::vector<float>& buffer, std::size_t harmonic) {
  const double fundamental = 440.0;
  const double frequency = fundamental * static_cast<double>(harmonic);
  const double sample_rate = kSampleRate;
  double real = 0.0;
  double imag = 0.0;
  const double two_pi = 2.0 * std::numbers::pi;
  for (std::size_t n = 0; n < buffer.size(); ++n) {
    const double phase = two_pi * frequency * static_cast<double>(n) / sample_rate;
    real += buffer[n] * std::cos(phase);
    imag -= buffer[n] * std::sin(phase);
  }
  const double magnitude = std::sqrt(real * real + imag * imag) /
                           static_cast<double>(buffer.size());
  return magnitude;
}

}  // namespace

TEST(OscillatorTest, FrequencyAccuracySine) {
  orpheus::dsp::Oscillator osc{kSampleRate};
  osc.set_waveform(orpheus::dsp::Waveform::Sine);
  osc.set_frequency(440.0);

  auto buffer = render_buffer(osc, kTestBlock);
  const double measured = measure_frequency(buffer, kSampleRate);
  EXPECT_NEAR(measured, 440.0, 0.2);
}

TEST(OscillatorTest, DCBalanceAcrossWaveforms) {
  orpheus::dsp::Oscillator osc{kSampleRate};
  osc.set_unison_voice_count(1);

  const std::array<orpheus::dsp::Waveform, 4> waveforms = {
      orpheus::dsp::Waveform::Sine, orpheus::dsp::Waveform::Saw,
      orpheus::dsp::Waveform::Square, orpheus::dsp::Waveform::Triangle};

  for (auto waveform : waveforms) {
    osc.set_waveform(waveform);
    osc.set_phase(0.0);
    auto buffer = render_buffer(osc, kTestBlock);
    EXPECT_LT(std::abs(dc_offset(buffer)), 0.01) << "Waveform " << static_cast<int>(waveform);
  }
}

TEST(OscillatorTest, HarmonicContentRespectsExpectations) {
  orpheus::dsp::Oscillator osc{kSampleRate};
  osc.set_frequency(440.0);

  osc.set_waveform(orpheus::dsp::Waveform::Sine);
  auto sine = render_buffer(osc, kTestBlock);
  const double sine_fundamental = magnitude_at(sine, 1);
  const double sine_h2 = magnitude_at(sine, 2);
  EXPECT_GT(20.0 * std::log10(sine_fundamental / std::max(1e-8, sine_h2)), 60.0);

  osc.set_waveform(orpheus::dsp::Waveform::Square);
  auto square = render_buffer(osc, kTestBlock);
  const double square_h1 = magnitude_at(square, 1);
  const double square_h2 = magnitude_at(square, 2);
  EXPECT_GT(20.0 * std::log10(square_h1 / std::max(1e-8, square_h2)), 30.0);

  osc.set_waveform(orpheus::dsp::Waveform::Saw);
  auto saw = render_buffer(osc, kTestBlock);
  const double saw_h1 = magnitude_at(saw, 1);
  const double saw_h10 = magnitude_at(saw, 10);
  EXPECT_GT(20.0 * std::log10(saw_h1 / std::max(1e-8, saw_h10)), 20.0);
}

TEST(OscillatorTest, ThreadSafeParameterUpdates) {
  orpheus::dsp::Oscillator osc{kSampleRate};
  osc.set_waveform(orpheus::dsp::Waveform::Saw);
  osc.set_unison_voice_count(4);
  std::atomic<bool> running{true};

  auto controller = std::async(std::launch::async, [&]() {
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> freq(110.0, 880.0);
    std::uniform_real_distribution<double> pw(0.1, 0.9);
    for (int i = 0; i < 1000; ++i) {
      osc.set_frequency(freq(rng));
      osc.set_pulse_width(pw(rng));
      osc.set_waveform(static_cast<orpheus::dsp::Waveform>(i % 4));
    }
    running.store(false);
  });

  std::vector<float> buffer(256);
  while (running.load()) {
    osc.process(buffer);
  }
  controller.wait();
  SUCCEED();
}

TEST(OscillatorTest, ProcessesEfficiently) {
  orpheus::dsp::Oscillator osc{kSampleRate};
  osc.set_waveform(orpheus::dsp::Waveform::Saw);
  osc.set_unison_voice_count(8);
  std::vector<float> buffer(1'000'000);

  const auto start = std::chrono::steady_clock::now();
  osc.process(buffer);
  const auto end = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(end - start).count();
  const double throughput = static_cast<double>(buffer.size()) / elapsed;

  EXPECT_GT(throughput, 1'000'000.0);
}

