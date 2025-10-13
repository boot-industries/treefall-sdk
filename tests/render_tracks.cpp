// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "session/json_io.h"
#include "session/session_graph.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
namespace session_json = orpheus::core::session_json;

namespace orpheus::tests {
namespace {

struct WavData {
  std::uint32_t sample_rate{};
  std::uint16_t channels{};
  std::uint16_t bit_depth{};
  std::vector<double> samples;
};

WavData LoadWave(const fs::path& path) {
  struct Header {
    char riff[4];
    std::uint32_t chunkSize;
    char wave[4];
    char fmt[4];
    std::uint32_t fmtChunkSize;
    std::uint16_t audioFormat;
    std::uint16_t numChannels;
    std::uint32_t sampleRate;
    std::uint32_t byteRate;
    std::uint16_t blockAlign;
    std::uint16_t bitsPerSample;
    char data[4];
    std::uint32_t dataSize;
  } header{};

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open WAV: " + path.string());
  }
  file.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!file) {
    throw std::runtime_error("Failed to read WAV header: " + path.string());
  }

  if (std::string(header.riff, 4) != "RIFF" || std::string(header.wave, 4) != "WAVE") {
    throw std::runtime_error("Unsupported WAV container: " + path.string());
  }
  if (header.audioFormat != 1u) {
    throw std::runtime_error("Only PCM WAV is supported for tests");
  }

  const std::size_t bytes_per_sample = (header.bitsPerSample + 7u) / 8u;
  if (bytes_per_sample != 2u && bytes_per_sample != 3u) {
    throw std::runtime_error("Unexpected bit depth in test WAV");
  }
  if (header.blockAlign != header.numChannels * bytes_per_sample) {
    throw std::runtime_error("Invalid block alignment in test WAV");
  }

  std::vector<std::uint8_t> payload(header.dataSize);
  if (!payload.empty()) {
    file.read(reinterpret_cast<char*>(payload.data()), header.dataSize);
    if (!file) {
      throw std::runtime_error("Failed to read WAV payload");
    }
  }

  const std::size_t frame_count = header.blockAlign == 0 ? 0 : header.dataSize / header.blockAlign;
  WavData data;
  data.sample_rate = header.sampleRate;
  data.channels = header.numChannels;
  data.bit_depth = header.bitsPerSample;
  data.samples.resize(frame_count * header.numChannels);

  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    for (std::size_t channel = 0; channel < header.numChannels; ++channel) {
      const std::size_t offset = frame * header.blockAlign + channel * bytes_per_sample;
      double value = 0.0;
      if (bytes_per_sample == 2u) {
        const std::int16_t sample =
            static_cast<std::int16_t>(payload[offset] | (payload[offset + 1] << 8));
        value = static_cast<double>(sample) / 32767.0;
      } else {
        std::int32_t sample = static_cast<std::int32_t>(payload[offset]) |
                              (static_cast<std::int32_t>(payload[offset + 1]) << 8) |
                              (static_cast<std::int32_t>(payload[offset + 2]) << 16);
        if (sample & 0x00800000) {
          sample |= ~0x00FFFFFF;
        }
        value = static_cast<double>(sample) / 8388607.0;
      }
      data.samples[frame * header.numChannels + channel] = value;
    }
  }

  return data;
}

struct ClipSpec {
  double start_beats;
  double length_beats;
};

class TempDirGuard {
public:
  explicit TempDirGuard(fs::path path) : path_(std::move(path)) {}
  ~TempDirGuard() {
    if (!path_.empty()) {
      std::error_code ec;
      fs::remove_all(path_, ec);
    }
  }

  const fs::path& get() const {
    return path_;
  }

private:
  fs::path path_;
};

std::size_t BeatsToSampleIndex(double beats, double seconds_per_beat, std::uint32_t sample_rate) {
  const double raw = beats * seconds_per_beat * sample_rate;
  const auto rounded = static_cast<long long>(std::llround(raw));
  if (rounded <= 0) {
    return 0;
  }
  return static_cast<std::size_t>(rounded);
}

std::size_t BeatsToSampleCount(double beats, double seconds_per_beat, std::uint32_t sample_rate) {
  if (beats <= 0.0) {
    return 0;
  }
  const double raw = beats * seconds_per_beat * sample_rate;
  const auto rounded = static_cast<long long>(std::llround(raw));
  return static_cast<std::size_t>(std::max<long long>(1, rounded));
}

} // namespace

TEST(RenderTracks, GeneratesSineStemsWithDitheredQuantization) {
  uint32_t session_major = 0;
  uint32_t session_minor = 0;
  const auto* session_api =
      orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &session_major, &session_minor);
  ASSERT_NE(session_api, nullptr);
  ASSERT_EQ(session_major, ORPHEUS_ABI_MAJOR);
  ASSERT_EQ(session_minor, ORPHEUS_ABI_MINOR);

  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  const auto* clipgrid_api = orpheus_clipgrid_abi_v1(ORPHEUS_ABI_MAJOR, &clip_major, &clip_minor);
  ASSERT_NE(clipgrid_api, nullptr);
  ASSERT_EQ(clip_major, ORPHEUS_ABI_MAJOR);
  ASSERT_EQ(clip_minor, ORPHEUS_ABI_MINOR);

  uint32_t render_major = 0;
  uint32_t render_minor = 0;
  const auto* render_api = orpheus_render_abi_v1(ORPHEUS_ABI_MAJOR, &render_major, &render_minor);
  ASSERT_NE(render_api, nullptr);
  ASSERT_EQ(render_major, ORPHEUS_ABI_MAJOR);
  ASSERT_EQ(render_minor, ORPHEUS_ABI_MINOR);

  orpheus_session_handle session_handle{};
  ASSERT_EQ(session_api->create(&session_handle), ORPHEUS_STATUS_OK);

  struct SessionGuard {
    const orpheus_session_api_v1* api{};
    orpheus_session_handle handle{};
    ~SessionGuard() {
      if (api && handle) {
        api->destroy(handle);
      }
    }
  } guard{session_api, session_handle};

  auto* session_impl = reinterpret_cast<orpheus::core::SessionGraph*>(session_handle);
  session_impl->set_name("Dialogue Demo");
  session_impl->set_render_sample_rate(48000);
  session_impl->set_render_bit_depth(24);
  session_impl->set_render_dither(true);

  constexpr double kTempo = 120.0;
  ASSERT_EQ(session_api->set_tempo(session_handle, kTempo), ORPHEUS_STATUS_OK);

  struct TrackDefinition {
    std::string name;
    std::vector<ClipSpec> clips;
  };

  const std::vector<TrackDefinition> track_defs = {
      {"DX", {{0.0, 8.0}}},
      {"MUS", {{4.0, 8.0}}},
      {"SFX", {{2.0, 4.0}}},
  };

  std::map<std::string, std::vector<ClipSpec>> clip_lookup;

  for (const auto& track_def : track_defs) {
    orpheus_track_handle track_handle{};
    const orpheus_track_desc desc{track_def.name.c_str()};
    ASSERT_EQ(session_api->add_track(session_handle, &desc, &track_handle), ORPHEUS_STATUS_OK);

    for (const auto& clip_spec : track_def.clips) {
      const orpheus_clip_desc clip_desc{track_def.name.c_str(), clip_spec.start_beats,
                                        clip_spec.length_beats, 0u};
      orpheus_clip_handle clip_handle{};
      ASSERT_EQ(clipgrid_api->add_clip(session_handle, track_handle, &clip_desc, &clip_handle),
                ORPHEUS_STATUS_OK);
    }
    clip_lookup[track_def.name] = track_def.clips;
  }

  ASSERT_EQ(clipgrid_api->commit(session_handle), ORPHEUS_STATUS_OK);

  const fs::path temp_root =
      fs::temp_directory_path() /
      fs::path("orpheus_render_tracks_test" +
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(temp_root);
  TempDirGuard temp_guard(temp_root);

  ASSERT_EQ(render_api->render_tracks(session_handle, temp_guard.get().string().c_str()),
            ORPHEUS_STATUS_OK);

  const double seconds_per_beat = 60.0 / kTempo;
  const double session_start = session_impl->session_start_beats();
  const double session_end = session_impl->session_end_beats();
  const std::uint32_t sample_rate = session_impl->render_sample_rate();
  const std::size_t total_samples =
      BeatsToSampleCount(session_end - session_start, seconds_per_beat, sample_rate);

  ASSERT_GT(total_samples, 0u);

  for (std::size_t track_index = 0; track_index < track_defs.size(); ++track_index) {
    const auto& track_def = track_defs[track_index];
    const std::string stem_name = session_json::MakeRenderStemFilename(
        session_impl->name(), track_def.name, sample_rate, session_impl->render_bit_depth());
    const fs::path rendered_path = temp_guard.get() / stem_name;
    ASSERT_TRUE(fs::exists(rendered_path)) << "Missing rendered stem: " << rendered_path;

    const WavData wav = LoadWave(rendered_path);
    EXPECT_EQ(wav.sample_rate, sample_rate);
    EXPECT_EQ(wav.channels, 2u);
    EXPECT_EQ(wav.bit_depth, session_impl->render_bit_depth());
    EXPECT_EQ(wav.samples.size(), total_samples * wav.channels);

    const double pan = track_defs.size() > 1 ? static_cast<double>(track_index) /
                                                   static_cast<double>(track_defs.size() - 1)
                                             : 0.5;
    const double left_gain = std::clamp(1.0 - pan, 0.0, 1.0);
    const double right_gain = std::clamp(pan, 0.0, 1.0);
    const double frequency = 220.0 + 110.0 * static_cast<double>(track_index);

    std::vector<double> expected(wav.samples.size(), 0.0);
    std::vector<bool> active(total_samples, false);

    const auto& clips = clip_lookup.at(track_def.name);
    for (const auto& clip : clips) {
      const std::size_t start_index =
          BeatsToSampleIndex(clip.start_beats - session_start, seconds_per_beat, sample_rate);
      const std::size_t clip_samples =
          BeatsToSampleCount(clip.length_beats, seconds_per_beat, sample_rate);
      for (std::size_t i = 0; i < clip_samples; ++i) {
        const std::size_t sample_index = start_index + i;
        if (sample_index >= total_samples) {
          break;
        }
        const double t = static_cast<double>(sample_index) / static_cast<double>(sample_rate);
        const double value = std::sin(2.0 * std::numbers::pi * frequency * t) * 0.4;
        expected[sample_index * 2u] += value * left_gain;
        expected[sample_index * 2u + 1u] += value * right_gain;
        active[sample_index] = true;
      }
    }

    for (std::size_t channel = 0; channel < 2; ++channel) {
      double diff_energy = 0.0;
      double signal_energy = 0.0;
      double expected_energy = 0.0;
      double cross_energy = 0.0;
      std::size_t active_samples = 0;
      for (std::size_t sample_index = 0; sample_index < total_samples; ++sample_index) {
        if (!active[sample_index]) {
          continue;
        }
        const double actual = wav.samples[sample_index * 2u + channel];
        const double target = expected[sample_index * 2u + channel];
        const double error = actual - target;
        diff_energy += error * error;
        signal_energy += actual * actual;
        expected_energy += target * target;
        cross_energy += actual * target;
        ++active_samples;
      }

      if (active_samples == 0) {
        continue;
      }

      const double rms_error = std::sqrt(diff_energy / static_cast<double>(active_samples));
      if (expected_energy <= 1e-12) {
        // Channel should remain silent aside from dither.
        EXPECT_LT(rms_error, 5e-5);
      } else {
        EXPECT_LT(rms_error, 5e-5);
        const double correlation = cross_energy / std::sqrt(signal_energy * expected_energy);
        EXPECT_GT(correlation, 0.999);
      }
    }
  }
}

} // namespace orpheus::tests
