// SPDX-License-Identifier: MIT
#include "render/render_tracks.h"

#include "render/orpheus_wav.hpp"
#include "render/pcm.hpp"
#include "session/json_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace orpheus::core::render {
namespace {

std::size_t BeatsToSampleIndex(double beats, double seconds_per_beat, std::uint32_t sample_rate) {
  const double samples = beats * seconds_per_beat * static_cast<double>(sample_rate);
  const auto rounded = static_cast<long long>(std::llround(samples));
  if (rounded <= 0) {
    return 0;
  }
  return static_cast<std::size_t>(rounded);
}

std::size_t BeatsToSampleCount(double beats, double seconds_per_beat, std::uint32_t sample_rate) {
  if (beats <= 0.0) {
    return 0;
  }
  const double samples = beats * seconds_per_beat * static_cast<double>(sample_rate);
  const auto rounded = static_cast<long long>(std::llround(samples));
  return static_cast<std::size_t>(std::max<long long>(1, rounded));
}

std::size_t ClipFrameCount(const Clip& clip) {
  if (clip.samples.empty()) {
    return 0;
  }
  const std::size_t frames = clip.samples.front().size();
  for (const auto& channel : clip.samples) {
    if (channel.size() != frames) {
      throw std::invalid_argument("clip channels have mismatched length");
    }
  }
  return frames;
}

std::vector<int> MakeDefaultMap(std::size_t clip_channels, std::uint32_t output_channels) {
  std::vector<int> map;
  if (output_channels == 0u) {
    return map;
  }
  const std::size_t routes = std::max<std::size_t>(
      1u, clip_channels == 0 ? 0u : std::min<std::size_t>(clip_channels, output_channels));
  map.reserve(routes);
  for (std::size_t route = 0; route < routes; ++route) {
    const std::uint32_t target =
        output_channels == 1u
            ? 0u
            : static_cast<std::uint32_t>(std::min<std::size_t>(route, output_channels - 1u));
    map.push_back(static_cast<int>(target));
  }
  return map;
}

void MixClip(const Clip& clip, const std::vector<int>& routing, std::uint32_t output_channels,
             std::vector<double>& buffer, std::size_t start_sample) {
  const std::size_t clip_frames = ClipFrameCount(clip);
  if (clip_frames == 0u || routing.empty() || output_channels == 0u) {
    return;
  }
  const std::size_t clip_channels = clip.samples.size();
  const std::size_t frame_capacity = buffer.size() / output_channels;
  for (std::size_t route = 0; route < routing.size(); ++route) {
    const int target = routing[route];
    if (target < 0 || static_cast<std::uint32_t>(target) >= output_channels) {
      throw std::invalid_argument("clip routing index out of range");
    }
    const std::size_t source_channel =
        clip_channels == 1u ? 0u : std::min<std::size_t>(route, clip_channels - 1u);
    const auto& channel_samples = clip.samples[source_channel];
    for (std::size_t frame = 0; frame < clip_frames; ++frame) {
      const std::size_t dest_frame = start_sample + frame;
      if (dest_frame >= frame_capacity) {
        break;
      }
      buffer[dest_frame * output_channels + static_cast<std::size_t>(target)] +=
          static_cast<double>(channel_samples[frame]);
    }
  }
}

void ValidateSpec(const RenderSpec& spec) {
  if (spec.output_directory.empty()) {
    throw std::invalid_argument("render output directory is empty");
  }
  if (spec.sample_rate_hz == 0u) {
    throw std::invalid_argument("render sample rate must be positive");
  }
  if (spec.output_channels != 1u && spec.output_channels != 2u) {
    throw std::invalid_argument("render requires mono or stereo output");
  }
  if (spec.bit_depth_bits != 16u && spec.bit_depth_bits != 24u && spec.bit_depth_bits != 32u) {
    throw std::invalid_argument("render supports 16, 24, or 32-bit output");
  }
}

} // namespace

std::vector<std::filesystem::path> render_tracks(const Session& session, const TrackList& tracks,
                                                 const RenderSpec& spec) {
  if (session.tempo_bpm <= 0.0) {
    throw std::invalid_argument("tempo must be positive");
  }
  if (session.end_beats < session.start_beats) {
    throw std::invalid_argument("session range is invalid");
  }

  ValidateSpec(spec);
  std::filesystem::create_directories(spec.output_directory);

  const double seconds_per_beat = 60.0 / session.tempo_bpm;
  const double total_beats = std::max(0.0, session.end_beats - session.start_beats);
  const std::size_t session_frames =
      BeatsToSampleCount(total_beats, seconds_per_beat, spec.sample_rate_hz);

  std::vector<std::filesystem::path> outputs;
  outputs.reserve(tracks.size());

  for (std::size_t track_index = 0; track_index < tracks.size(); ++track_index) {
    const Track& track = tracks[track_index];

    std::size_t required_frames = session_frames;
    for (const auto& clip : track.clips) {
      const std::size_t clip_frames = ClipFrameCount(clip);
      if (clip_frames == 0u) {
        continue;
      }
      const double offset_beats = clip.start_beats - session.start_beats;
      const std::size_t start_sample =
          BeatsToSampleIndex(offset_beats, seconds_per_beat, spec.sample_rate_hz);
      required_frames = std::max(required_frames, start_sample + clip_frames);
    }

    std::vector<double> mix_buffer(required_frames * spec.output_channels, 0.0);

    for (const auto& clip : track.clips) {
      const std::size_t clip_frames = ClipFrameCount(clip);
      if (clip_frames == 0u) {
        continue;
      }
      const double offset_beats = clip.start_beats - session.start_beats;
      const std::size_t start_sample =
          BeatsToSampleIndex(offset_beats, seconds_per_beat, spec.sample_rate_hz);
      std::vector<int> fallback_map;
      const std::vector<int>* routing = nullptr;
      if (track.output_map.empty()) {
        fallback_map = MakeDefaultMap(clip.samples.size(), spec.output_channels);
        routing = &fallback_map;
      } else {
        routing = &track.output_map;
      }
      if (routing->empty()) {
        fallback_map = MakeDefaultMap(clip.samples.size(), spec.output_channels);
        routing = &fallback_map;
      }
      MixClip(clip, *routing, spec.output_channels, mix_buffer, start_sample);
    }

    const std::uint64_t dither_seed = spec.dither_seed + track_index;
    const std::vector<std::uint8_t> pcm =
        QuantizeInterleaved(mix_buffer, spec.bit_depth_bits, spec.dither, dither_seed);

    const std::string filename = session_json::MakeRenderStemFilename(
        session.name, track.name, spec.sample_rate_hz, spec.bit_depth_bits);
    const std::filesystem::path target_path = spec.output_directory / filename;
    WriteWaveFile(target_path, spec.sample_rate_hz,
                  static_cast<std::uint16_t>(spec.output_channels), spec.bit_depth_bits, pcm);
    outputs.push_back(target_path);
  }

  return outputs;
}

} // namespace orpheus::core::render
