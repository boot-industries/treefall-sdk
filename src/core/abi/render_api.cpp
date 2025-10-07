// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"
#include "session/json_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <string>
#include <stdexcept>
#include <vector>

using orpheus::abi_internal::GuardAbiCall;

namespace {

constexpr int kBeatsPerBar = 4;
constexpr int kClickBitsPerSample = 16;

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
  std::uint16_t bitsPerSample = 0;
  char data[4] = {'d', 'a', 't', 'a'};
  std::uint32_t dataSize = 0;
};

void WriteWaveFile(const std::string &path, std::uint32_t sample_rate,
                   std::uint16_t channels, std::uint16_t bits_per_sample,
                   const void *data, std::size_t byte_count) {
  namespace fs = std::filesystem;
  const fs::path output_path(path);
  if (!output_path.parent_path().empty()) {
    fs::create_directories(output_path.parent_path());
  }

  WavHeader header;
  header.numChannels = channels;
  header.sampleRate = sample_rate;
  const std::uint16_t bytes_per_sample =
      static_cast<std::uint16_t>((bits_per_sample + 7u) / 8u);
  header.bitsPerSample = bits_per_sample;
  header.blockAlign = header.numChannels * bytes_per_sample;
  header.byteRate = header.sampleRate * header.blockAlign;
  if (byte_count > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("render payload too large");
  }
  header.dataSize = static_cast<std::uint32_t>(byte_count);
  header.chunkSize = 36 + header.dataSize;

  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw orpheus::abi_internal::IoException("Unable to open render target: " +
                                             path);
  }

  stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
  if (byte_count > 0) {
    stream.write(reinterpret_cast<const char *>(data),
                 static_cast<std::streamsize>(byte_count));
  }
  if (!stream) {
    throw orpheus::abi_internal::IoException("Failed to write render output: " +
                                             path);
  }
}

class TpdfDitherGenerator {
 public:
  explicit TpdfDitherGenerator(std::uint64_t seed) : state_(seed) {}

  double Next(double lsb) {
    if (lsb == 0.0) {
      return 0.0;
    }
    return (Uniform() - Uniform()) * lsb;
  }

 private:
  double Uniform() {
    state_ = state_ * 6364136223846793005ull + 1ull;
    const std::uint64_t mantissa = (state_ >> 11u) & ((1ull << 53u) - 1u);
    return static_cast<double>(mantissa) / static_cast<double>(1ull << 53u);
  }

  std::uint64_t state_;
};

std::vector<std::uint8_t> QuantizeInterleaved(
    const std::vector<double> &samples, std::uint16_t bit_depth_bits,
    bool dither, std::uint64_t seed) {
  const std::size_t bytes_per_sample = (bit_depth_bits + 7u) / 8u;
  if (bytes_per_sample != 2u && bytes_per_sample != 3u) {
    throw std::invalid_argument("Unsupported bit depth");
  }

  const std::int64_t min_value = -(1ll << (bit_depth_bits - 1));
  const std::int64_t max_value = (1ll << (bit_depth_bits - 1)) - 1ll;
  const double max_amplitude = static_cast<double>(max_value);
  const double lsb = 1.0 / static_cast<double>(1ull << (bit_depth_bits - 1));

  std::vector<std::uint8_t> pcm(samples.size() * bytes_per_sample);
  TpdfDitherGenerator generator(seed);

  for (std::size_t index = 0; index < samples.size(); ++index) {
    double sample = std::clamp(samples[index], -1.0, 1.0);
    if (dither) {
      sample += generator.Next(lsb);
    }
    sample = std::clamp(sample, -1.0, 1.0);

    std::int64_t quantized =
        static_cast<std::int64_t>(std::llround(sample * max_amplitude));
    quantized = std::clamp(quantized, min_value, max_value);

    const std::size_t offset = index * bytes_per_sample;
    if (bytes_per_sample == 2u) {
      const std::int16_t value = static_cast<std::int16_t>(quantized);
      pcm[offset] = static_cast<std::uint8_t>(value & 0xFF);
      pcm[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    } else {
      const std::int32_t value = static_cast<std::int32_t>(quantized);
      pcm[offset] = static_cast<std::uint8_t>(value & 0xFF);
      pcm[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
      pcm[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    }
  }

  return pcm;
}

orpheus_status RenderClick(const orpheus_render_click_spec *spec,
                           const char *out_path) {
  if (spec == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const RenderClickParams params = NormalizeRenderSpec(*spec);
    const std::vector<int16_t> samples = GenerateClickSamples(params);
    WriteWaveFile(out_path, params.sample_rate,
                  static_cast<std::uint16_t>(params.channels),
                  kClickBitsPerSample, samples.data(),
                  samples.size() * sizeof(int16_t));
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status RenderTracks(orpheus_session_handle session,
                            const char *out_path) {
  if (session == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }

  return GuardAbiCall([&]() -> orpheus_status {
    auto *session_graph = orpheus::abi_internal::ToSession(session);
    if (session_graph == nullptr) {
      return ORPHEUS_STATUS_INVALID_ARGUMENT;
    }

    const auto &tracks = session_graph->tracks();
    if (tracks.empty()) {
      return ORPHEUS_STATUS_OK;
    }

    const double tempo = session_graph->tempo();
    if (tempo <= 0.0) {
      throw std::invalid_argument("Tempo must be positive");
    }

    const std::uint32_t sample_rate = session_graph->render_sample_rate();
    const std::uint16_t bit_depth = session_graph->render_bit_depth();
    const bool dither = session_graph->render_dither();

    const double session_start = session_graph->session_start_beats();
    const double session_end = session_graph->session_end_beats();
    const double total_beats = std::max(0.0, session_end - session_start);
    const double seconds_per_beat = 60.0 / tempo;

    const auto BeatsToSampleIndex = [&](double beats) -> std::size_t {
      const double samples = beats * seconds_per_beat *
                             static_cast<double>(sample_rate);
      const auto rounded = static_cast<long long>(std::llround(samples));
      if (rounded <= 0) {
        return 0;
      }
      return static_cast<std::size_t>(rounded);
    };

    const auto BeatsToSampleCount = [&](double beats) -> std::size_t {
      if (beats <= 0.0) {
        return 0;
      }
      const double samples = beats * seconds_per_beat *
                             static_cast<double>(sample_rate);
      const auto rounded = static_cast<long long>(std::llround(samples));
      return static_cast<std::size_t>(std::max<long long>(1, rounded));
    };

    const std::size_t total_samples = BeatsToSampleCount(total_beats);

    namespace fs = std::filesystem;
    fs::path base_path(out_path);
    if (base_path.empty()) {
      base_path = fs::current_path();
    }
    fs::create_directories(base_path);

    const std::size_t track_count = tracks.size();
    for (std::size_t track_index = 0; track_index < track_count; ++track_index) {
      const auto &track = tracks[track_index];
      std::vector<double> interleaved(total_samples * 2u, 0.0);

      const double pan =
          track_count > 1 ? static_cast<double>(track_index) /
                                static_cast<double>(track_count - 1)
                           : 0.5;
      const double left_gain = std::clamp(1.0 - pan, 0.0, 1.0);
      const double right_gain = std::clamp(pan, 0.0, 1.0);
      const double amplitude = 0.4;
      const double frequency = 220.0 + 110.0 * static_cast<double>(track_index);

      for (const auto &clip : track->clips()) {
        const double clip_start = clip->start() - session_start;
        const double clip_length = clip->length();
        const std::size_t start_sample = BeatsToSampleIndex(clip_start);
        std::size_t clip_samples = BeatsToSampleCount(clip_length);
        if (clip_samples == 0u) {
          continue;
        }
        for (std::size_t i = 0; i < clip_samples; ++i) {
          const std::size_t sample_index = start_sample + i;
          if (sample_index >= interleaved.size() / 2u) {
            break;
          }
          const double t = static_cast<double>(sample_index) /
                           static_cast<double>(sample_rate);
          const double sample_value =
              std::sin(2.0 * std::numbers::pi * frequency * t) * amplitude;
          interleaved[sample_index * 2u] += sample_value * left_gain;
          interleaved[sample_index * 2u + 1u] += sample_value * right_gain;
        }
      }

      const std::vector<std::uint8_t> pcm =
          QuantizeInterleaved(interleaved, bit_depth, dither,
                              0x9e3779b97f4a7c15ull + track_index);
      const std::string stem_name =
          orpheus::core::session_json::MakeRenderStemFilename(
              session_graph->name(), track->name(), sample_rate, bit_depth);
      const fs::path target_path = base_path / stem_name;
      WriteWaveFile(target_path.string(), sample_rate, 2u, bit_depth,
                    pcm.data(), pcm.size());
    }

    return ORPHEUS_STATUS_OK;
  });
}

const orpheus_render_api_v1 kRenderApiV1{ORPHEUS_RENDER_CAP_V1_CORE, &RenderClick,
                                         &RenderTracks};

}  // namespace

extern "C" ORPHEUS_API const orpheus_render_api_v1 *
orpheus_render_abi_v1(uint32_t want_major, uint32_t *got_major,
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
