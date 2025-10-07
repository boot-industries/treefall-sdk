// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace orpheus::core::render {

struct Session {
  std::string name;
  double tempo_bpm{120.0};
  double start_beats{0.0};
  double end_beats{0.0};
};

struct Clip {
  double start_beats{0.0};
  std::vector<std::vector<float>> samples;
};

struct Track {
  std::string name;
  std::vector<Clip> clips;
  std::vector<int> output_map;
};

using TrackList = std::vector<Track>;

struct RenderSpec {
  std::filesystem::path output_directory;
  std::uint32_t sample_rate_hz{44100};
  std::uint16_t bit_depth_bits{24};
  std::uint32_t output_channels{2};
  bool dither{true};
  std::uint64_t dither_seed{0x9e3779b97f4a7c15ull};
};

std::vector<std::filesystem::path> render_tracks(const Session &session,
                                                  const TrackList &tracks,
                                                  const RenderSpec &spec);

}  // namespace orpheus::core::render
