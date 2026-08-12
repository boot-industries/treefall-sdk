// SPDX-License-Identifier: MIT
//
// ORP127 T8 (G6) — Polyphase sample-rate converter.
//
// Verifies pitch correctness (a sine keeps its frequency across a rate change),
// pass-through behavior for equal rates, and determinism (bit-identical output
// for identical input). Frequency is measured by zero-crossing counting — no
// FFT dependency, fully deterministic.

#include <orpheus/directional_sample_rate_converter.h>
#include <orpheus/polyphase_resampler.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

// MSVC's <cmath> does not define M_PI without _USE_MATH_DEFINES; guard it so the
// Windows build resolves the constant. Matches the codebase's M_PI_2 guard.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace orpheus;

namespace {

std::vector<float> makeSine(uint32_t rate, double freq, double seconds, uint16_t ch = 1) {
  const int64_t n = static_cast<int64_t>(seconds * rate);
  std::vector<float> out(static_cast<size_t>(n) * ch);
  for (int64_t i = 0; i < n; ++i) {
    float s = static_cast<float>(
        std::sin(2.0 * M_PI * freq * static_cast<double>(i) / static_cast<double>(rate)));
    for (uint16_t c = 0; c < ch; ++c)
      out[static_cast<size_t>(i) * ch + c] = s;
  }
  return out;
}

// Measure fundamental frequency via zero-crossing rate on channel 0. Skips a
// startup guard region to avoid the FIR transient.
double measureFreq(const std::vector<float>& interleaved, uint16_t ch, uint32_t rate,
                   size_t guardFrames) {
  const size_t frames = interleaved.size() / ch;
  if (frames <= guardFrames + 2)
    return 0.0;
  int crossings = 0;
  size_t firstCross = 0, lastCross = 0;
  bool haveFirst = false;
  for (size_t i = guardFrames + 1; i < frames; ++i) {
    float prev = interleaved[(i - 1) * ch];
    float cur = interleaved[i * ch];
    if (prev < 0.0f && cur >= 0.0f) { // rising zero crossing
      if (!haveFirst) {
        firstCross = i;
        haveFirst = true;
      }
      lastCross = i;
      ++crossings;
    }
  }
  if (crossings < 2)
    return 0.0;
  // (crossings - 1) full periods span (lastCross - firstCross) frames.
  double periods = static_cast<double>(crossings - 1);
  double spanFrames = static_cast<double>(lastCross - firstCross);
  return periods * static_cast<double>(rate) / spanFrames;
}

using audio_utils::DirectionalSampleRateConverter;
using audio_utils::DirectionalSrcConfig;
using audio_utils::DirectionalSrcStatus;

DirectionalSrcConfig converterConfig(uint32_t input_rate, uint32_t output_rate,
                                     uint16_t channels = 1, uint32_t prime_input_frames = 0) {
  DirectionalSrcConfig config;
  config.input_rate = input_rate;
  config.output_rate = output_rate;
  config.channels = channels;
  config.max_input_frames = 1024;
  config.max_output_frames = 1024;
  config.fifo_capacity_frames = 8192;
  config.prime_input_frames = prime_input_frames;
  config.allow_input_rate_correction = true;
  return config;
}

void pushPlanar(DirectionalSampleRateConverter& converter, const std::vector<float>& input,
                uint16_t channels, size_t offset, uint32_t frames) {
  if (frames > 1024) {
    for (uint32_t chunk_offset = 0; chunk_offset < frames;) {
      const uint32_t chunk_frames = std::min<uint32_t>(1024, frames - chunk_offset);
      pushPlanar(converter, input, channels, offset + chunk_offset, chunk_frames);
      chunk_offset += chunk_frames;
    }
    return;
  }
  std::vector<float> planar(static_cast<size_t>(frames) * channels);
  for (uint32_t frame = 0; frame < frames; ++frame) {
    for (uint16_t channel = 0; channel < channels; ++channel) {
      planar[static_cast<size_t>(channel) * frames + frame] =
          input[(offset + frame) * channels + channel];
    }
  }
  std::vector<const float*> planar_planes(channels);
  for (uint16_t channel = 0; channel < channels; ++channel) {
    planar_planes[channel] = planar.data() + static_cast<size_t>(channel) * frames;
  }
  const auto transfer = converter.pushInput(planar_planes.data(), channels, frames);
  ASSERT_EQ(transfer.status, DirectionalSrcStatus::Ok);
  ASSERT_EQ(transfer.input_frames_consumed, frames);
}

std::vector<float> renderPlanar(DirectionalSampleRateConverter& converter, uint16_t channels,
                                uint32_t frames) {
  std::vector<float> planar(static_cast<size_t>(frames) * channels, 0.0F);
  std::vector<float*> planes(channels);
  for (uint32_t offset = 0; offset < frames;) {
    const uint32_t count = std::min<uint32_t>(1024, frames - offset);
    for (uint16_t channel = 0; channel < channels; ++channel) {
      planes[channel] = planar.data() + static_cast<size_t>(channel) * frames + offset;
    }
    const auto transfer = converter.renderOutput(planes.data(), channels, count);
    EXPECT_EQ(transfer.status, DirectionalSrcStatus::Ok);
    EXPECT_EQ(transfer.output_frames_produced, count);
    offset += count;
  }
  std::vector<float> interleaved(static_cast<size_t>(frames) * channels);
  for (uint32_t frame = 0; frame < frames; ++frame) {
    for (uint16_t channel = 0; channel < channels; ++channel) {
      interleaved[static_cast<size_t>(frame) * channels + channel] =
          planar[static_cast<size_t>(channel) * frames + frame];
    }
  }
  return interleaved;
}

bool renderChunked(DirectionalSampleRateConverter& converter, uint16_t channels, uint32_t frames,
                   uint32_t chunk_frames, std::vector<float>& interleaved,
                   uint32_t& input_consumed) {
  interleaved.assign(static_cast<size_t>(frames) * channels, 0.0F);
  std::vector<float> planar(static_cast<size_t>(chunk_frames) * channels);
  std::vector<float*> planes(channels);
  input_consumed = 0;
  for (uint32_t offset = 0; offset < frames;) {
    const uint32_t count = std::min(chunk_frames, frames - offset);
    for (uint16_t channel = 0; channel < channels; ++channel) {
      planes[channel] = planar.data() + static_cast<size_t>(channel) * chunk_frames;
    }
    const auto transfer = converter.renderOutput(planes.data(), channels, count);
    if (transfer.status != DirectionalSrcStatus::Ok || transfer.output_frames_produced != count) {
      return false;
    }
    input_consumed += transfer.input_frames_consumed;
    for (uint32_t frame = 0; frame < count; ++frame) {
      for (uint16_t channel = 0; channel < channels; ++channel) {
        interleaved[static_cast<size_t>(offset + frame) * channels + channel] =
            planes[channel][frame];
      }
    }
    offset += count;
  }
  return true;
}

} // namespace

// 44.1 kHz sine in a 48 kHz engine must keep its frequency (correct pitch).
TEST(PolyphaseResamplerTest, UpsampleKeepsFrequency) {
  const uint32_t inRate = 44100, outRate = 48000;
  const double freq = 1000.0;
  auto in = makeSine(inRate, freq, 1.0, 1);

  PolyphaseResampler rs(inRate, outRate, 1);
  std::vector<float> out, chunk;
  // Feed in blocks to exercise streaming.
  const size_t block = 1024;
  for (size_t i = 0; i < in.size(); i += block) {
    size_t n = std::min(block, in.size() - i);
    rs.process(in.data() + i, n, chunk);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }

  double measured = measureFreq(out, 1, outRate, /*guard*/ 2000);
  EXPECT_NEAR(measured, freq, 2.0) << "Upsampled sine should keep 1000 Hz, got " << measured;

  // Output length should be ~ inFrames * outRate/inRate.
  double expectedFrames = static_cast<double>(in.size()) * outRate / inRate;
  EXPECT_NEAR(static_cast<double>(out.size()), expectedFrames, block * 2);
}

// 48 kHz sine downsampled to 44.1 kHz must keep its frequency.
TEST(PolyphaseResamplerTest, DownsampleKeepsFrequency) {
  const uint32_t inRate = 48000, outRate = 44100;
  const double freq = 1000.0;
  auto in = makeSine(inRate, freq, 1.0, 1);

  PolyphaseResampler rs(inRate, outRate, 1);
  std::vector<float> out, chunk;
  const size_t block = 1024;
  for (size_t i = 0; i < in.size(); i += block) {
    size_t n = std::min(block, in.size() - i);
    rs.process(in.data() + i, n, chunk);
    out.insert(out.end(), chunk.begin(), chunk.end());
  }

  double measured = measureFreq(out, 1, outRate, 2000);
  EXPECT_NEAR(measured, freq, 2.0) << "Downsampled sine should keep 1000 Hz, got " << measured;
}

// Equal rates => exact pass-through.
TEST(PolyphaseResamplerTest, EqualRatesArePassthrough) {
  PolyphaseResampler rs(48000, 48000, 2);
  EXPECT_TRUE(rs.isPassthrough());

  std::vector<float> in = {0.1f, -0.1f, 0.2f, -0.2f, 0.3f, -0.3f};
  std::vector<float> out;
  size_t frames = rs.process(in.data(), 3, out);
  EXPECT_EQ(frames, 3u);
  ASSERT_EQ(out.size(), in.size());
  for (size_t i = 0; i < in.size(); ++i)
    EXPECT_FLOAT_EQ(out[i], in[i]);
}

// Determinism: identical input -> bit-identical output across two instances.
TEST(PolyphaseResamplerTest, DeterministicOutput) {
  const uint32_t inRate = 44100, outRate = 48000;
  auto in = makeSine(inRate, 440.0, 0.5, 2);

  auto run = [&]() {
    PolyphaseResampler rs(inRate, outRate, 2);
    std::vector<float> out, chunk;
    const size_t block = 777; // odd block size to stress phase continuity
    for (size_t i = 0; i < in.size() / 2; i += block) {
      size_t n = std::min(block, in.size() / 2 - i);
      rs.process(in.data() + i * 2, n, chunk);
      out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return out;
  };

  std::vector<float> a = run();
  std::vector<float> b = run();
  ASSERT_EQ(a.size(), b.size());
  for (size_t i = 0; i < a.size(); ++i) {
    ASSERT_EQ(std::bit_cast<uint32_t>(a[i]), std::bit_cast<uint32_t>(b[i]))
        << "Non-deterministic output at index " << i;
  }
}

// Stereo channels stay independent (a hard-panned signal doesn't bleed).
TEST(PolyphaseResamplerTest, StereoChannelsIndependent) {
  const uint32_t inRate = 44100, outRate = 48000;
  const int64_t n = inRate / 2;
  std::vector<float> in(static_cast<size_t>(n) * 2);
  for (int64_t i = 0; i < n; ++i) {
    in[static_cast<size_t>(i) * 2 + 0] =
        static_cast<float>(std::sin(2.0 * M_PI * 500.0 * i / inRate)); // L: 500 Hz
    in[static_cast<size_t>(i) * 2 + 1] = 0.0f;                         // R: silent
  }

  PolyphaseResampler rs(inRate, outRate, 2);
  std::vector<float> out;
  rs.process(in.data(), static_cast<size_t>(n), out);

  // R channel must remain (near) silent.
  double rEnergy = 0.0;
  const size_t frames = out.size() / 2;
  for (size_t i = 0; i < frames; ++i)
    rEnergy += std::abs(static_cast<double>(out[i * 2 + 1]));
  EXPECT_LT(rEnergy / std::max<size_t>(1, frames), 1e-3)
      << "Silent right channel leaked energy from the left";
}

TEST(DirectionalSampleRateConverterTest, PreparationAndTransferSemantics) {
  DirectionalSampleRateConverter converter;
  uint32_t required = 123;
  EXPECT_EQ(converter.requiredInputFrames(1, required), DirectionalSrcStatus::NotPrepared);
  EXPECT_EQ(required, 0u);

  auto unsupported = converterConfig(32000, 48000);
  EXPECT_EQ(converter.prepare(unsupported), SessionGraphError::NotSupported);

  auto config = converterConfig(16000, 48000, 1, 4);
  ASSERT_EQ(converter.prepare(config), SessionGraphError::OK);
  EXPECT_FALSE(converter.isPrimed());
  EXPECT_EQ(converter.latencyOutputFrames(), 393u);

  float input_value = 1.0F;
  const float* input_plane = &input_value;
  EXPECT_EQ(converter.pushInput(&input_plane, 2, 1).status, DirectionalSrcStatus::InvalidArgument);
  EXPECT_EQ(converter.pushInput(nullptr, 1, 0).status, DirectionalSrcStatus::Ok);

  float output_storage[8];
  float* output_plane = output_storage;
  std::fill(std::begin(output_storage), std::end(output_storage), 9.0F);
  EXPECT_EQ(converter.renderOutput(&output_plane, 1, 1).status,
            DirectionalSrcStatus::InputUnderflow);
  EXPECT_TRUE(std::all_of(std::begin(output_storage), std::end(output_storage),
                          [](float sample) { return sample == 9.0F; }));

  std::vector<float> input(140, 0.25F);
  pushPlanar(converter, input, 1, 0, static_cast<uint32_t>(input.size()));
  EXPECT_TRUE(converter.isPrimed());
  const auto output = renderPlanar(converter, 1, 1);
  EXPECT_EQ(output.size(), 1u);
  converter.reset();
  EXPECT_FALSE(converter.isPrimed());
  EXPECT_EQ(converter.bufferedInputFrames(), 0u);
}

TEST(DirectionalSampleRateConverterTest, RequiredInputAndNominalFrameRatio) {
  DirectionalSampleRateConverter converter;
  auto config = converterConfig(48000, 16000);
  ASSERT_EQ(converter.prepare(config), SessionGraphError::OK);

  std::vector<float> input(4096);
  for (size_t index = 0; index < input.size(); ++index) {
    input[index] = static_cast<float>(index % 31) / 31.0F;
  }
  pushPlanar(converter, input, 1, 0, static_cast<uint32_t>(input.size()));
  ASSERT_TRUE(converter.isPrimed());

  uint32_t required = 0;
  ASSERT_EQ(converter.requiredInputFrames(1024, required), DirectionalSrcStatus::Ok);
  EXPECT_EQ(required, 0u);
  const auto first = renderPlanar(converter, 1, 1024);
  EXPECT_EQ(first.size(), 1024u);
  EXPECT_EQ(converter.bufferedInputFrames(), 4096u - 3072u);

  ASSERT_EQ(converter.requiredInputFrames(200, required), DirectionalSrcStatus::Ok);
  EXPECT_EQ(required, 0u);
  const auto second = renderPlanar(converter, 1, 200);
  EXPECT_EQ(second.size(), 200u);
  EXPECT_EQ(converter.bufferedInputFrames(), 4096u - 3072u - 600u);
}

TEST(DirectionalSampleRateConverterTest, SinePitchPassbandAndDownsampleRejection) {
  {
    DirectionalSampleRateConverter converter;
    ASSERT_EQ(converter.prepare(converterConfig(16000, 48000)), SessionGraphError::OK);
    const auto input = makeSine(16000, 1000.0, 0.125);
    pushPlanar(converter, input, 1, 0, static_cast<uint32_t>(input.size()));
    const auto output = renderPlanar(converter, 1, 5000);
    EXPECT_NEAR(measureFreq(output, 1, 48000, 500), 1000.0, 5.0);
    float peak = 0.0F;
    for (size_t index = 500; index < output.size(); ++index) {
      peak = std::max(peak, std::abs(output[index]));
    }
    EXPECT_GT(peak, 0.85F);
  }

  {
    DirectionalSampleRateConverter converter;
    ASSERT_EQ(converter.prepare(converterConfig(48000, 16000)), SessionGraphError::OK);
    const auto input = makeSine(48000, 1000.0, 0.125);
    pushPlanar(converter, input, 1, 0, static_cast<uint32_t>(input.size()));
    const auto output = renderPlanar(converter, 1, 1800);
    EXPECT_NEAR(measureFreq(output, 1, 16000, 200), 1000.0, 5.0);

    converter.reset();
    const auto stopband = makeSine(48000, 10000.0, 0.125);
    pushPlanar(converter, stopband, 1, 0, static_cast<uint32_t>(stopband.size()));
    const auto rejected = renderPlanar(converter, 1, 1800);
    double energy = 0.0;
    for (size_t index = 200; index < rejected.size(); ++index) {
      energy += static_cast<double>(rejected[index]) * rejected[index];
    }
    const double rms = std::sqrt(energy / static_cast<double>(rejected.size() - 200));
    EXPECT_LT(rms, 0.08);
  }
}

TEST(DirectionalSampleRateConverterTest, CorrectionAndChunkInvariance) {
  const auto input = makeSine(16000, 440.0, 0.125);

  DirectionalSampleRateConverter nominal;
  DirectionalSampleRateConverter corrected;
  ASSERT_EQ(nominal.prepare(converterConfig(16000, 48000)), SessionGraphError::OK);
  ASSERT_EQ(corrected.prepare(converterConfig(16000, 48000)), SessionGraphError::OK);
  EXPECT_FALSE(corrected.setInputRateCorrectionPpm(1001));
  EXPECT_TRUE(corrected.setInputRateCorrectionPpm(1000));
  pushPlanar(nominal, input, 1, 0, static_cast<uint32_t>(input.size()));
  pushPlanar(corrected, input, 1, 0, static_cast<uint32_t>(input.size()));

  std::vector<float> nominal_output;
  std::vector<float> corrected_output;
  uint32_t nominal_consumed = 0;
  uint32_t corrected_consumed = 0;
  ASSERT_TRUE(renderChunked(nominal, 1, 3000, 1024, nominal_output, nominal_consumed));
  ASSERT_TRUE(renderChunked(corrected, 1, 3000, 1024, corrected_output, corrected_consumed));
  EXPECT_GT(corrected_consumed, nominal_consumed);

  DirectionalSampleRateConverter one_chunk;
  DirectionalSampleRateConverter many_chunks;
  ASSERT_EQ(one_chunk.prepare(converterConfig(16000, 48000)), SessionGraphError::OK);
  ASSERT_EQ(many_chunks.prepare(converterConfig(16000, 48000)), SessionGraphError::OK);
  pushPlanar(one_chunk, input, 1, 0, static_cast<uint32_t>(input.size()));
  for (size_t offset = 0; offset < input.size(); offset += 137) {
    const uint32_t frames = static_cast<uint32_t>(std::min<size_t>(137, input.size() - offset));
    pushPlanar(many_chunks, input, 1, offset, frames);
  }
  std::vector<float> one_output;
  std::vector<float> many_output;
  uint32_t one_consumed = 0;
  uint32_t many_consumed = 0;
  ASSERT_TRUE(renderChunked(one_chunk, 1, 3000, 1024, one_output, one_consumed));
  ASSERT_TRUE(renderChunked(many_chunks, 1, 3000, 257, many_output, many_consumed));
  ASSERT_EQ(many_output.size(), one_output.size());
  for (size_t index = 0; index < one_output.size(); ++index) {
    EXPECT_FLOAT_EQ(one_output[index], many_output[index]);
  }
}

TEST(DirectionalSampleRateConverterTest, StereoChannelsRemainIndependentAndOverflowIsTerminal) {
  DirectionalSampleRateConverter converter;
  ASSERT_EQ(converter.prepare(converterConfig(48000, 16000, 2)), SessionGraphError::OK);
  std::vector<float> input(4096 * 2, 0.0F);
  for (size_t frame = 0; frame < 4096; ++frame) {
    input[frame * 2] = static_cast<float>(std::sin(2.0 * M_PI * 500.0 * frame / 48000.0));
  }
  pushPlanar(converter, input, 2, 0, 4096);
  const auto output = renderPlanar(converter, 2, 1000);
  double right_energy = 0.0;
  for (size_t frame = 0; frame < output.size() / 2; ++frame) {
    right_energy += std::abs(static_cast<double>(output[frame * 2 + 1]));
  }
  EXPECT_LT(right_energy / 1000.0, 1e-4);

  DirectionalSampleRateConverter overflow_converter;
  ASSERT_EQ(overflow_converter.prepare(converterConfig(48000, 16000, 1)), SessionGraphError::OK);
  std::vector<float> full_fifo(8192, 0.5F);
  pushPlanar(overflow_converter, full_fifo, 1, 0, 8192);
  std::vector<const float*> planes = {full_fifo.data()};
  const auto overflow = overflow_converter.pushInput(planes.data(), 1, 1);
  EXPECT_EQ(overflow.status, DirectionalSrcStatus::InputOverflow);
  EXPECT_EQ(overflow.input_frames_consumed, 0u);
}
