// SPDX-License-Identifier: MIT
#include "render/render_tracks.h"

#include "support/fnv1a64.hpp"
#include "support/synth.hpp"
#include "support/wav_parse.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
namespace support = orpheus::tests::support;

namespace {

class ScratchDir {
 public:
  explicit ScratchDir(const std::string &name) {
    const fs::path base = fs::temp_directory_path();
    path_ = base / ("orpheus_render_tracks_basic_" + name);
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_);
  }

  ~ScratchDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path &path() const { return path_; }

 private:
  fs::path path_;
};

class FailureArtifactGuard {
 public:
  explicit FailureArtifactGuard(std::vector<fs::path> files)
      : files_(std::move(files)) {}

  ~FailureArtifactGuard() {
    if (!::testing::Test::HasFailure()) {
      return;
    }
    try {
      const fs::path artifact_dir = fs::current_path() / "tmp" / "render_failures";
      fs::create_directories(artifact_dir);
      for (const auto &file : files_) {
        if (file.empty() || !fs::exists(file)) {
          continue;
        }
        const fs::path target = artifact_dir / file.filename();
        std::error_code ec;
        fs::copy_file(file, target, fs::copy_options::overwrite_existing, ec);
        if (!ec) {
          std::cout << "Saved render artifact: "
                    << fs::absolute(target) << std::endl;
        }
      }
    } catch (const std::exception &ex) {
      std::cout << "Failed to stash render artifacts: " << ex.what() << std::endl;
    }
  }

 private:
  std::vector<fs::path> files_;
};

std::vector<std::int64_t> DecodeChannel(const support::ParsedWav &wav,
                                        std::size_t channel) {
  const std::size_t bytes_per_sample = (wav.bits_per_sample + 7u) / 8u;
  if (wav.channels == 0 || bytes_per_sample == 0) {
    return {};
  }
  const std::size_t frame_stride = wav.channels * bytes_per_sample;
  if (frame_stride == 0 || wav.data.size() % frame_stride != 0) {
    return {};
  }
  const std::size_t frame_count = wav.data.size() / frame_stride;
  std::vector<std::int64_t> samples(frame_count, 0);

  const auto RoundTiesToZero = [](double value) -> std::int64_t {
    const double floored = std::floor(value);
    const double fraction = value - floored;
    if (fraction > 0.5) {
      return static_cast<std::int64_t>(floored + 1.0);
    }
    if (fraction < 0.5) {
      return static_cast<std::int64_t>(floored);
    }
    if (value >= 0.0) {
      return static_cast<std::int64_t>(floored);
    }
    return static_cast<std::int64_t>(floored + 1.0);
  };

  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    const std::size_t offset = frame * frame_stride + channel * bytes_per_sample;
    if (wav.audio_format == 1u) {
      if (wav.bits_per_sample == 16u) {
        const std::int16_t value = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(wav.data[offset]) |
            (static_cast<std::uint16_t>(wav.data[offset + 1]) << 8));
        samples[frame] = static_cast<std::int64_t>(value);
      } else if (wav.bits_per_sample == 24u) {
        std::int32_t value = static_cast<std::int32_t>(wav.data[offset]) |
                             (static_cast<std::int32_t>(wav.data[offset + 1])
                              << 8) |
                             (static_cast<std::int32_t>(wav.data[offset + 2])
                              << 16);
        if (value & 0x00800000) {
          value |= ~0x00FFFFFF;
        }
        samples[frame] = static_cast<std::int64_t>(value);
      }
    } else if (wav.audio_format == 3u && wav.bits_per_sample == 32u) {
      float value = 0.0f;
      std::memcpy(&value, wav.data.data() + offset, sizeof(float));
      const double clamped = std::clamp(static_cast<double>(value), -1.0, 1.0);
      samples[frame] =
          RoundTiesToZero(clamped * static_cast<double>(1 << 15));
    }
  }

  return samples;
}

double ComputeRms(const std::vector<std::int64_t> &samples,
                  std::int64_t full_scale) {
  if (samples.empty() || full_scale == 0) {
    return 0.0;
  }
  const long double denom = static_cast<long double>(samples.size());
  const long double scale = static_cast<long double>(full_scale);
  long double sum = 0.0L;
  for (const auto sample : samples) {
    const long double normalized = static_cast<long double>(sample) / scale;
    sum += normalized * normalized;
  }
  return std::sqrt(static_cast<double>(sum / denom));
}

double ComputeCorrelation(const std::vector<std::int64_t> &lhs,
                          const std::vector<std::int64_t> &rhs) {
  if (lhs.size() != rhs.size() || lhs.empty()) {
    return 0.0;
  }
  const std::size_t count = lhs.size();
  long double mean_l = 0.0L;
  long double mean_r = 0.0L;
  for (std::size_t i = 0; i < count; ++i) {
    mean_l += static_cast<long double>(lhs[i]);
    mean_r += static_cast<long double>(rhs[i]);
  }
  mean_l /= static_cast<long double>(count);
  mean_r /= static_cast<long double>(count);

  long double sum_lr = 0.0L;
  long double sum_ll = 0.0L;
  long double sum_rr = 0.0L;
  for (std::size_t i = 0; i < count; ++i) {
    const long double l = static_cast<long double>(lhs[i]) - mean_l;
    const long double r = static_cast<long double>(rhs[i]) - mean_r;
    sum_lr += l * r;
    sum_ll += l * l;
    sum_rr += r * r;
  }
  if (sum_ll <= 0.0L || sum_rr <= 0.0L) {
    return 0.0;
  }
  return static_cast<double>(sum_lr / std::sqrt(sum_ll * sum_rr));
}

double ComputeMean(const std::vector<std::int64_t> &samples,
                   std::int64_t full_scale) {
  if (samples.empty() || full_scale == 0) {
    return 0.0;
  }
  long double sum = 0.0L;
  for (auto sample : samples) {
    sum += static_cast<long double>(sample);
  }
  const long double scale = static_cast<long double>(full_scale);
  return static_cast<double>((sum / static_cast<long double>(samples.size())) /
                             scale);
}

std::uint64_t HashChannelBytes(const support::ParsedWav &wav, std::size_t channel) {
  const std::size_t bytes_per_sample = (wav.bits_per_sample + 7u) / 8u;
  if (wav.channels == 0 || bytes_per_sample == 0) {
    return support::kFnv1a64Offset;
  }
  const std::size_t frame_stride = wav.channels * bytes_per_sample;
  if (frame_stride == 0 || wav.data.size() % frame_stride != 0) {
    return support::kFnv1a64Offset;
  }
  const std::size_t frame_count = wav.data.size() / frame_stride;
  std::uint64_t hash = support::kFnv1a64Offset;
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    const std::size_t offset = frame * frame_stride + channel * bytes_per_sample;
    hash = support::Fnv1a64(wav.data.data() + offset, bytes_per_sample, hash);
  }
  return hash;
}

struct RenderContext {
  orpheus::core::render::Session session;
  orpheus::core::render::TrackList tracks;
  orpheus::core::render::RenderSpec spec;
};

RenderContext MakeBaseContext(const std::string &test_name,
                              std::uint32_t sample_rate,
                              std::uint16_t bit_depth,
                              std::uint32_t channels) {
  (void)test_name;
  RenderContext ctx;
  ctx.session.name = "render_tracks_basic";
  ctx.session.tempo_bpm = 60.0;
  ctx.session.start_beats = 0.0;
  ctx.session.end_beats = 1.0;
  ctx.spec.output_directory = fs::path();
  ctx.spec.sample_rate_hz = sample_rate;
  ctx.spec.bit_depth_bits = bit_depth;
  ctx.spec.output_channels = channels;
  ctx.spec.dither = false;
  ctx.spec.dither_seed = 0x9e3779b97f4a7c15ull;
  return ctx;
}

}  // namespace

TEST(RenderTracksBasic, MonoSineHashAndMetrics) {
  const auto *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  ScratchDir scratch(test_info->name());

  auto ctx = MakeBaseContext(test_info->name(), 48000, 24, 1);
  ctx.spec.output_directory = scratch.path();

  orpheus::core::render::Clip clip;
  clip.start_beats = 0.0;
  clip.samples.push_back(
      support::GenerateSine(ctx.spec.sample_rate_hz, ctx.spec.sample_rate_hz,
                            440u, 0.5f));

  orpheus::core::render::Track track;
  track.name = "tone";
  track.clips.push_back(std::move(clip));
  ctx.tracks.push_back(std::move(track));

  const auto outputs =
      orpheus::core::render::render_tracks(ctx.session, ctx.tracks, ctx.spec);
  ASSERT_EQ(outputs.size(), 1u);
  FailureArtifactGuard guard(outputs);

  const support::ParsedWav wav = support::ReadWav(outputs.front());
  EXPECT_EQ(wav.audio_format, 1u);
  EXPECT_EQ(wav.sample_rate, ctx.spec.sample_rate_hz);
  EXPECT_EQ(wav.channels, ctx.spec.output_channels);
  EXPECT_EQ(wav.bits_per_sample, ctx.spec.bit_depth_bits);

  const std::uint64_t hash = support::Fnv1a64(wav.data);
  constexpr std::uint64_t kExpectedHash = 7792556221712049445ull;
  EXPECT_EQ(hash, kExpectedHash);

  const auto samples = DecodeChannel(wav, 0);
  const std::int64_t full_scale = 1ll << (wav.bits_per_sample - 1);
  const double rms = ComputeRms(samples, full_scale);
  constexpr double kExpectedRms = 0.35354217456110504;
  const double tolerance = 1.0 / static_cast<double>(full_scale);
  EXPECT_NEAR(rms, kExpectedRms, tolerance);
}

TEST(RenderTracksBasic, StereoSineImpulseHashesAndCorrelation) {
  const auto *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  ScratchDir scratch(test_info->name());

  auto ctx = MakeBaseContext(test_info->name(), 48000, 24, 2);
  ctx.spec.output_directory = scratch.path();

  orpheus::core::render::Clip clip;
  clip.start_beats = 0.0;
  clip.samples.push_back(
      support::GenerateSine(ctx.spec.sample_rate_hz, ctx.spec.sample_rate_hz,
                            440u, 0.5f));
  clip.samples.push_back(
      support::GenerateImpulse(ctx.spec.sample_rate_hz,
                               ctx.spec.sample_rate_hz / 2));

  orpheus::core::render::Track track;
  track.name = "stereo";
  track.clips.push_back(std::move(clip));
  ctx.tracks.push_back(std::move(track));

  const auto outputs =
      orpheus::core::render::render_tracks(ctx.session, ctx.tracks, ctx.spec);
  ASSERT_EQ(outputs.size(), 1u);
  FailureArtifactGuard guard(outputs);

  const support::ParsedWav wav = support::ReadWav(outputs.front());
  EXPECT_EQ(wav.audio_format, 1u);
  EXPECT_EQ(wav.sample_rate, ctx.spec.sample_rate_hz);
  EXPECT_EQ(wav.channels, ctx.spec.output_channels);
  EXPECT_EQ(wav.bits_per_sample, ctx.spec.bit_depth_bits);

  const std::uint64_t hash = support::Fnv1a64(wav.data);
  constexpr std::uint64_t kExpectedHash = 15661357029020024030ull;
  EXPECT_EQ(hash, kExpectedHash);

  const std::uint64_t left_hash = HashChannelBytes(wav, 0);
  const std::uint64_t right_hash = HashChannelBytes(wav, 1);
  constexpr std::uint64_t kExpectedLeftHash = 7792556221712049445ull;
  constexpr std::uint64_t kExpectedRightHash = 3376279170353656508ull;
  EXPECT_EQ(left_hash, kExpectedLeftHash);
  EXPECT_EQ(right_hash, kExpectedRightHash);

  const auto left = DecodeChannel(wav, 0);
  const auto right = DecodeChannel(wav, 1);
  ASSERT_EQ(left.size(), right.size());

  const std::int64_t full_scale = 1ll << (wav.bits_per_sample - 1);
  const double left_rms = ComputeRms(left, full_scale);
  constexpr double kExpectedLeftRms = 0.35354217456110504;
  const double tolerance = 1.0 / static_cast<double>(full_scale);
  EXPECT_NEAR(left_rms, kExpectedLeftRms, tolerance);

  const double impulse_rms = ComputeRms(right, full_scale);
  constexpr double kExpectedImpulseRms = 0.0045643541017629094;
  EXPECT_NEAR(impulse_rms, kExpectedImpulseRms, tolerance);

  const double correlation = ComputeCorrelation(left, right);
  EXPECT_GE(correlation, -0.01);
  EXPECT_LE(correlation, 0.01);
}

TEST(RenderTracksBasic, SineWithDcOffsetRemainsStable) {
  const auto *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  ScratchDir scratch(test_info->name());

  auto ctx = MakeBaseContext(test_info->name(), 48000, 24, 1);
  ctx.spec.output_directory = scratch.path();

  orpheus::core::render::Clip sine_clip;
  sine_clip.start_beats = 0.0;
  sine_clip.samples.push_back(
      support::GenerateSine(ctx.spec.sample_rate_hz, ctx.spec.sample_rate_hz,
                            440u, 0.5f));

  orpheus::core::render::Clip dc_clip;
  dc_clip.start_beats = 0.0;
  dc_clip.samples.push_back(
      support::GenerateDc(ctx.spec.sample_rate_hz, 3.0f / 8388608.0f));

  orpheus::core::render::Track track;
  track.name = "tone_dc";
  track.clips.push_back(std::move(sine_clip));
  track.clips.push_back(std::move(dc_clip));
  ctx.tracks.push_back(std::move(track));

  const auto outputs =
      orpheus::core::render::render_tracks(ctx.session, ctx.tracks, ctx.spec);
  ASSERT_EQ(outputs.size(), 1u);
  FailureArtifactGuard guard(outputs);

  const support::ParsedWav wav = support::ReadWav(outputs.front());
  EXPECT_EQ(wav.audio_format, 1u);
  EXPECT_EQ(wav.sample_rate, ctx.spec.sample_rate_hz);
  EXPECT_EQ(wav.channels, ctx.spec.output_channels);
  EXPECT_EQ(wav.bits_per_sample, ctx.spec.bit_depth_bits);

  const std::uint64_t hash = support::Fnv1a64(wav.data);
  constexpr std::uint64_t kExpectedHash = 5577600473188412997ull;
  EXPECT_EQ(hash, kExpectedHash);

  const auto samples = DecodeChannel(wav, 0);
  const std::int64_t full_scale = 1ll << (wav.bits_per_sample - 1);
  const double rms = ComputeRms(samples, full_scale);
  constexpr double kBaselineRms = 0.35354217456110504;
  const double tolerance = 1.0 / static_cast<double>(full_scale);
  EXPECT_NEAR(rms, kBaselineRms, tolerance);

  const double mean = ComputeMean(samples, full_scale);
  constexpr double kExpectedMean = 3.5762786865234375e-07;
  EXPECT_NEAR(mean, kExpectedMean, tolerance);
}
