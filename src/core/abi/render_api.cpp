// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

using orpheus::abi_internal::GuardAbiCall;

namespace {

constexpr int kBeatsPerBar = 4;
constexpr int kBitsPerSample = 16;

struct RenderClickParams {
  double tempo_bpm;
  std::uint32_t bars;
  std::uint32_t sample_rate;
  std::uint32_t channels;
  double gain;
  double frequency_hz;
  double duration_seconds;
};

RenderClickParams NormalizeRenderSpec(const orpheus_render_click_spec &spec) {
  RenderClickParams params{};
  params.tempo_bpm = spec.tempo_bpm > 0.0 ? spec.tempo_bpm : 120.0;
  params.bars = spec.bars > 0u ? spec.bars : 4u;
  params.sample_rate = spec.sample_rate > 0u ? spec.sample_rate : 44100u;
  params.channels = spec.channels > 0u ? spec.channels : 2u;
  params.gain = (spec.gain > 0.0 && spec.gain <= 1.0) ? spec.gain : 0.25;
  params.frequency_hz =
      spec.click_frequency_hz > 0.0 ? spec.click_frequency_hz : 1000.0;
  params.duration_seconds = spec.click_duration_seconds > 0.0
                                ? spec.click_duration_seconds
                                : 0.05;
  return params;
}

std::vector<int16_t> GenerateClickSamples(const RenderClickParams &params) {
  const std::uint64_t total_beats =
      static_cast<std::uint64_t>(params.bars) * kBeatsPerBar;
  const double samples_per_beat_f =
      static_cast<double>(params.sample_rate) * 60.0 / params.tempo_bpm;
  std::uint64_t samples_per_beat =
      static_cast<std::uint64_t>(std::llround(samples_per_beat_f));
  samples_per_beat = std::max<std::uint64_t>(1, samples_per_beat);

  const double click_samples_f =
      params.duration_seconds * static_cast<double>(params.sample_rate);
  std::uint64_t click_samples =
      static_cast<std::uint64_t>(std::llround(click_samples_f));
  click_samples = std::max<std::uint64_t>(1, click_samples);

  const std::uint64_t total_samples = samples_per_beat * total_beats;
  std::vector<int16_t> buffer(total_samples * params.channels, 0);

  const double phase_increment =
      2.0 * std::numbers::pi * params.frequency_hz /
      static_cast<double>(params.sample_rate);

  for (std::uint64_t beat = 0; beat < total_beats; ++beat) {
    const std::uint64_t offset = beat * samples_per_beat;
    const double accent = (beat % kBeatsPerBar == 0) ? 1.0 : 0.75;
    for (std::uint64_t i = 0; i < click_samples && (offset + i) < total_samples;
         ++i) {
      const double envelope = 0.5 *
                               (1.0 - std::cos(std::numbers::pi *
                                               static_cast<double>(i) /
                                               static_cast<double>(click_samples)));
      const double sample_value =
          std::sin(phase_increment * static_cast<double>(i)) * envelope *
          params.gain * accent;
      const double clamped = std::clamp(sample_value, -1.0, 1.0);
      const int16_t pcm = static_cast<int16_t>(std::lrint(clamped * 32767.0));
      for (std::uint32_t channel = 0; channel < params.channels; ++channel) {
        buffer[(offset + i) * params.channels + channel] = pcm;
      }
    }
  }

  return buffer;
}

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  std::uint32_t chunkSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  std::uint32_t fmtChunkSize = 16;
  std::uint16_t audioFormat = 1;
  std::uint16_t numChannels = 0;
  std::uint32_t sampleRate = 0;
  std::uint32_t byteRate = 0;
  std::uint16_t blockAlign = 0;
  std::uint16_t bitsPerSample = kBitsPerSample;
  char data[4] = {'d', 'a', 't', 'a'};
  std::uint32_t dataSize = 0;
};

void WriteWaveFile(const std::string &path, const RenderClickParams &params,
                   const std::vector<int16_t> &samples) {
  namespace fs = std::filesystem;
  const fs::path output_path(path);
  if (!output_path.parent_path().empty()) {
    fs::create_directories(output_path.parent_path());
  }

  WavHeader header;
  header.numChannels = static_cast<std::uint16_t>(params.channels);
  header.sampleRate = params.sample_rate;
  header.blockAlign =
      header.numChannels * static_cast<std::uint16_t>(sizeof(int16_t));
  header.byteRate = header.sampleRate * header.blockAlign;
  header.dataSize =
      static_cast<std::uint32_t>(samples.size() * sizeof(int16_t));
  header.chunkSize = 36 + header.dataSize;

  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw orpheus::abi_internal::IoException("Unable to open render target: " +
                                             path);
  }

  stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
  stream.write(reinterpret_cast<const char *>(samples.data()),
               static_cast<std::streamsize>(samples.size() * sizeof(int16_t)));
  if (!stream) {
    throw orpheus::abi_internal::IoException("Failed to write render output: " +
                                             path);
  }
}

orpheus_status RenderClick(const orpheus_render_click_spec *spec,
                           const char *out_path) {
  if (spec == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const RenderClickParams params = NormalizeRenderSpec(*spec);
    const std::vector<int16_t> samples = GenerateClickSamples(params);
    WriteWaveFile(out_path, params, samples);
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status RenderTracks(orpheus_session_handle /*session*/,
                            const char * /*out_path*/) {
  return ORPHEUS_STATUS_NOT_IMPLEMENTED;
}

const orpheus_render_api_v1 kRenderApiV1{ORPHEUS_RENDER_CAP_V1_CORE, &RenderClick,
                                         &RenderTracks};

}  // namespace

extern "C" {

const orpheus_render_api_v1 *orpheus_render_abi_v1(uint32_t want_major,
                                                   uint32_t *got_major,
                                                   uint32_t *got_minor) {
  (void)want_major;
  if (got_major != nullptr) {
    *got_major = ORPHEUS_ABI_V1_MAJOR;
  }
  if (got_minor != nullptr) {
    *got_minor = ORPHEUS_ABI_V1_MINOR;
  }
  return &kRenderApiV1;
}

}  // extern "C"
