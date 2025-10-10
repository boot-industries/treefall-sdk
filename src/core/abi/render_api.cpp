// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include "abi/abi_internal.h"
#include "render/orpheus_wav.hpp"
#include "render/render_tracks.h"
#include "session/json_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <string>
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

RenderClickParams NormalizeRenderSpec(const orpheus_render_click_spec& spec) {
  RenderClickParams params{};
  params.tempo_bpm = spec.tempo_bpm > 0.0 ? spec.tempo_bpm : 120.0;
  params.bars = spec.bars > 0u ? spec.bars : 4u;
  params.sample_rate = spec.sample_rate > 0u ? spec.sample_rate : 44100u;
  params.channels = spec.channels > 0u ? spec.channels : 2u;
  params.gain = (spec.gain > 0.0 && spec.gain <= 1.0) ? spec.gain : 0.25;
  params.frequency_hz = spec.click_frequency_hz > 0.0 ? spec.click_frequency_hz : 1000.0;
  params.duration_seconds = spec.click_duration_seconds > 0.0 ? spec.click_duration_seconds : 0.05;
  return params;
}

std::vector<int16_t> GenerateClickSamples(const RenderClickParams& params) {
  const std::uint64_t total_beats = static_cast<std::uint64_t>(params.bars) * kBeatsPerBar;
  const double samples_per_beat_f =
      static_cast<double>(params.sample_rate) * 60.0 / params.tempo_bpm;
  std::uint64_t samples_per_beat = static_cast<std::uint64_t>(std::llround(samples_per_beat_f));
  samples_per_beat = std::max<std::uint64_t>(1, samples_per_beat);

  const double click_samples_f = params.duration_seconds * static_cast<double>(params.sample_rate);
  std::uint64_t click_samples = static_cast<std::uint64_t>(std::llround(click_samples_f));
  click_samples = std::max<std::uint64_t>(1, click_samples);

  const std::uint64_t total_samples = samples_per_beat * total_beats;
  std::vector<int16_t> buffer(total_samples * params.channels, 0);

  const double phase_increment =
      2.0 * std::numbers::pi * params.frequency_hz / static_cast<double>(params.sample_rate);

  for (std::uint64_t beat = 0; beat < total_beats; ++beat) {
    const std::uint64_t offset = beat * samples_per_beat;
    const double accent = (beat % kBeatsPerBar == 0) ? 1.0 : 0.75;
    for (std::uint64_t i = 0; i < click_samples && (offset + i) < total_samples; ++i) {
      const double envelope = 0.5 * (1.0 - std::cos(std::numbers::pi * static_cast<double>(i) /
                                                    static_cast<double>(click_samples)));
      const double sample_value =
          std::sin(phase_increment * static_cast<double>(i)) * envelope * params.gain * accent;
      const double clamped = std::clamp(sample_value, -1.0, 1.0);
      const int16_t pcm = static_cast<int16_t>(std::lrint(clamped * 32767.0));
      for (std::uint32_t channel = 0; channel < params.channels; ++channel) {
        buffer[(offset + i) * params.channels + channel] = pcm;
      }
    }
  }

  return buffer;
}

orpheus_status RenderClick(const orpheus_render_click_spec* spec, const char* out_path) {
  if (spec == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const RenderClickParams params = NormalizeRenderSpec(*spec);
    const std::vector<int16_t> samples = GenerateClickSamples(params);
    orpheus::core::render::WriteWaveFile(
        std::filesystem::path(out_path), params.sample_rate,
        static_cast<std::uint16_t>(params.channels), kClickBitsPerSample,
        reinterpret_cast<const std::uint8_t*>(samples.data()), samples.size() * sizeof(int16_t));
    return ORPHEUS_STATUS_OK;
  });
}

orpheus_status RenderTracks(orpheus_session_handle session, const char* out_path) {
  if (session == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }

  return GuardAbiCall([&]() -> orpheus_status {
    auto* session_graph = orpheus::abi_internal::ToSession(session);
    if (session_graph == nullptr) {
      return ORPHEUS_STATUS_INVALID_ARGUMENT;
    }

    const auto& tracks = session_graph->tracks();
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

    namespace fs = std::filesystem;
    fs::path base_path(out_path);
    if (base_path.empty()) {
      base_path = fs::current_path();
    }
    const double session_start = session_graph->session_start_beats();
    const double session_end = session_graph->session_end_beats();
    const double seconds_per_beat = 60.0 / tempo;

    const auto BeatsToSampleIndex = [&](double beats) -> std::size_t {
      const double samples = beats * seconds_per_beat * static_cast<double>(sample_rate);
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
      const double samples = beats * seconds_per_beat * static_cast<double>(sample_rate);
      const auto rounded = static_cast<long long>(std::llround(samples));
      return static_cast<std::size_t>(std::max<long long>(1, rounded));
    };

    orpheus::core::render::Session session_desc;
    session_desc.name = session_graph->name();
    session_desc.tempo_bpm = tempo;
    session_desc.start_beats = session_start;
    session_desc.end_beats = session_end;

    orpheus::core::render::TrackList render_tracks_list;
    render_tracks_list.reserve(tracks.size());

    const std::size_t track_count = tracks.size();
    for (std::size_t track_index = 0; track_index < track_count; ++track_index) {
      const auto& track = tracks[track_index];
      orpheus::core::render::Track render_track;
      render_track.name = track->name();
      render_track.output_map = {0, 1};

      const double pan =
          track_count > 1 ? static_cast<double>(track_index) / static_cast<double>(track_count - 1)
                          : 0.5;
      const double left_gain = std::clamp(1.0 - pan, 0.0, 1.0);
      const double right_gain = std::clamp(pan, 0.0, 1.0);
      const double amplitude = 0.4;
      const double frequency = 220.0 + 110.0 * static_cast<double>(track_index);

      for (const auto& clip : track->clips()) {
        const double clip_length = clip->length();
        std::size_t clip_samples = BeatsToSampleCount(clip_length);
        if (clip_samples == 0u) {
          continue;
        }
        const std::size_t start_sample = BeatsToSampleIndex(clip->start() - session_start);
        std::vector<float> left_channel(clip_samples, 0.0f);
        std::vector<float> right_channel(clip_samples, 0.0f);
        for (std::size_t i = 0; i < clip_samples; ++i) {
          const std::size_t sample_index = start_sample + i;
          const double t = static_cast<double>(sample_index) / static_cast<double>(sample_rate);
          const double sample_value = std::sin(2.0 * std::numbers::pi * frequency * t) * amplitude;
          left_channel[i] = static_cast<float>(sample_value * left_gain);
          right_channel[i] = static_cast<float>(sample_value * right_gain);
        }
        orpheus::core::render::Clip render_clip;
        render_clip.start_beats = clip->start();
        render_clip.samples.push_back(std::move(left_channel));
        render_clip.samples.push_back(std::move(right_channel));
        render_track.clips.push_back(std::move(render_clip));
      }

      render_tracks_list.push_back(std::move(render_track));
    }

    orpheus::core::render::RenderSpec spec;
    spec.output_directory = base_path;
    spec.sample_rate_hz = sample_rate;
    spec.bit_depth_bits = bit_depth;
    spec.output_channels = 2u;
    spec.dither = dither;
    spec.dither_seed = 0x9e3779b97f4a7c15ull;

    orpheus::core::render::render_tracks(session_desc, render_tracks_list, spec);

    return ORPHEUS_STATUS_OK;
  });
}

const orpheus_render_api_v1 kRenderApiV1{ORPHEUS_RENDER_CAP_V1_CORE, &RenderClick, &RenderTracks};

} // namespace

extern "C" ORPHEUS_API const orpheus_render_api_v1*
orpheus_render_abi_v1(uint32_t want_major, uint32_t* got_major, uint32_t* got_minor) {
  if (got_major != nullptr) {
    *got_major = ORPHEUS_ABI_MAJOR;
  }
  if (got_minor != nullptr) {
    *got_minor = ORPHEUS_ABI_MINOR;
  }
  if (want_major != ORPHEUS_ABI_MAJOR) {
    return nullptr;
  }
  return &kRenderApiV1;
}
