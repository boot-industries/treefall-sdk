// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"
#include "orpheus/music_timing.h"

#include "abi/abi_internal.h"
#include "render/orpheus_wav.hpp"
#include "render/render_tracks.h"
#include "session/json_io.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

using orpheus::abi_internal::GuardAbiCall;

namespace {

using orpheus::kBeatsPerBar;
constexpr int kClickBitsPerSample = 16;
constexpr std::uint32_t kMaximumClickSampleRate = 192000;
constexpr std::uint32_t kMaximumClickChannels = 8;
constexpr std::uint64_t kMaximumClickPayloadBytes = 268435456;
constexpr std::size_t kRenderChunkFrames = 4096;

struct RenderClickParams {
  double tempo_bpm;
  std::uint32_t bars;
  std::uint32_t sample_rate;
  std::uint32_t channels;
  double gain;
  double frequency_hz;
  double duration_seconds;
};

struct RenderClickPlan {
  RenderClickParams params;
  std::uint64_t samples_per_beat;
  std::uint64_t click_samples;
  std::uint64_t total_frames;
  std::uint64_t interleaved_samples;
  std::uint64_t data_bytes;
};

void RequireFinite(double value, const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
}

std::uint64_t CheckedMultiply(std::uint64_t lhs, std::uint64_t rhs, const char* name) {
  if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
    throw std::invalid_argument(std::string(name) + " overflows");
  }
  return lhs * rhs;
}

std::uint64_t RoundedFrameCount(double value, const char* name) {
  if (!std::isfinite(value) || value > static_cast<double>(std::numeric_limits<long long>::max())) {
    throw std::invalid_argument(std::string(name) + " is out of range");
  }
  const long long rounded = std::llround(value);
  return rounded > 0 ? static_cast<std::uint64_t>(rounded) : 1u;
}

RenderClickPlan BuildRenderPlan(const orpheus_render_click_spec& spec) {
  RequireFinite(spec.tempo_bpm, "Click tempo");
  RequireFinite(spec.gain, "Click gain");
  RequireFinite(spec.click_frequency_hz, "Click frequency");
  RequireFinite(spec.click_duration_seconds, "Click duration");

  RenderClickParams params{};
  params.tempo_bpm = spec.tempo_bpm > 0.0 ? spec.tempo_bpm : 120.0;
  params.bars = spec.bars > 0u ? spec.bars : 4u;
  params.sample_rate = spec.sample_rate > 0u ? spec.sample_rate : 44100u;
  params.channels = spec.channels > 0u ? spec.channels : 2u;
  params.gain = (spec.gain > 0.0 && spec.gain <= 1.0) ? spec.gain : 0.25;
  params.frequency_hz = spec.click_frequency_hz > 0.0 ? spec.click_frequency_hz : 1000.0;
  params.duration_seconds = spec.click_duration_seconds > 0.0 ? spec.click_duration_seconds : 0.05;

  if (params.sample_rate > kMaximumClickSampleRate || params.channels > kMaximumClickChannels) {
    throw std::invalid_argument("Click render format exceeds resource limits");
  }

  const double samples_per_beat_f =
      orpheus::samplesPerBeat(params.tempo_bpm, static_cast<double>(params.sample_rate));
  const std::uint64_t samples_per_beat =
      RoundedFrameCount(samples_per_beat_f, "Click samples per beat");
  const double click_samples_f = params.duration_seconds * static_cast<double>(params.sample_rate);
  const std::uint64_t click_samples = RoundedFrameCount(click_samples_f, "Click duration frames");
  const std::uint64_t total_beats =
      CheckedMultiply(static_cast<std::uint64_t>(params.bars), kBeatsPerBar, "Click beat count");
  const std::uint64_t total_frames =
      CheckedMultiply(samples_per_beat, total_beats, "Click frame count");
  const std::uint64_t interleaved_samples =
      CheckedMultiply(total_frames, params.channels, "Click sample count");
  const std::uint64_t data_bytes =
      CheckedMultiply(interleaved_samples, sizeof(std::int16_t), "Click payload size");
  if (data_bytes > kMaximumClickPayloadBytes) {
    throw std::invalid_argument("Click payload exceeds 256 MiB limit");
  }

  const double phase_increment =
      2.0 * std::numbers::pi * params.frequency_hz / static_cast<double>(params.sample_rate);
  RequireFinite(phase_increment, "Click phase increment");

  return {params, samples_per_beat, click_samples, total_frames, interleaved_samples, data_bytes};
}

orpheus_status RenderClick(const orpheus_render_click_spec* spec, const char* out_path) {
  if (spec == nullptr || out_path == nullptr) {
    return ORPHEUS_STATUS_INVALID_ARGUMENT;
  }
  return GuardAbiCall([&]() -> orpheus_status {
    const RenderClickPlan plan = BuildRenderPlan(*spec);
    const double phase_increment = 2.0 * std::numbers::pi * plan.params.frequency_hz /
                                   static_cast<double>(plan.params.sample_rate);
    std::array<std::int16_t, kRenderChunkFrames * kMaximumClickChannels> buffer{};
    orpheus::core::render::WavStreamWriter writer(
        std::filesystem::path(out_path), plan.params.sample_rate,
        static_cast<std::uint16_t>(plan.params.channels), kClickBitsPerSample, plan.data_bytes);

    for (std::uint64_t frame_offset = 0; frame_offset < plan.total_frames;) {
      const std::uint64_t frame_count =
          std::min<std::uint64_t>(kRenderChunkFrames, plan.total_frames - frame_offset);
      const std::size_t sample_count = static_cast<std::size_t>(frame_count * plan.params.channels);
      std::fill_n(buffer.begin(), sample_count, std::int16_t{0});

      for (std::uint64_t frame = 0; frame < frame_count; ++frame) {
        const std::uint64_t absolute_frame = frame_offset + frame;
        const std::uint64_t beat = absolute_frame / plan.samples_per_beat;
        const std::uint64_t click_frame = absolute_frame % plan.samples_per_beat;
        if (click_frame >= plan.click_samples) {
          continue;
        }
        const double accent = (beat % kBeatsPerBar == 0) ? 1.0 : 0.75;
        const double envelope =
            0.5 * (1.0 - std::cos(std::numbers::pi * static_cast<double>(click_frame) /
                                  static_cast<double>(plan.click_samples)));
        const double sample_value = std::sin(phase_increment * static_cast<double>(click_frame)) *
                                    envelope * plan.params.gain * accent;
        const double clamped = std::clamp(sample_value, -1.0, 1.0);
        const std::int16_t pcm = static_cast<std::int16_t>(std::lrint(clamped * 32767.0));
        for (std::uint32_t channel = 0; channel < plan.params.channels; ++channel) {
          buffer[static_cast<std::size_t>(frame * plan.params.channels + channel)] = pcm;
        }
      }

      writer.write(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                   sample_count * sizeof(std::int16_t));
      frame_offset += frame_count;
    }
    writer.close();
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
    const double seconds_per_beat = orpheus::secondsPerBeat(tempo);

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
