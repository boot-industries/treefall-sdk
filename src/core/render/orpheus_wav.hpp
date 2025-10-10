// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace orpheus::core::render {

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  std::uint32_t chunk_size = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  std::uint32_t fmt_chunk_size = 16;
  std::uint16_t audio_format = 1;
  std::uint16_t num_channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint32_t byte_rate = 0;
  std::uint16_t block_align = 0;
  std::uint16_t bits_per_sample = 0;
  char data[4] = {'d', 'a', 't', 'a'};
  std::uint32_t data_size = 0;
};

inline void WriteWaveFile(const std::filesystem::path& path, std::uint32_t sample_rate,
                          std::uint16_t channels, std::uint16_t bits_per_sample,
                          const std::uint8_t* data, std::size_t byte_count) {
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }

  WavHeader header;
  header.num_channels = channels;
  header.sample_rate = sample_rate;
  const std::uint16_t bytes_per_sample = static_cast<std::uint16_t>((bits_per_sample + 7u) / 8u);
  header.bits_per_sample = bits_per_sample;
  header.block_align = header.num_channels * bytes_per_sample;
  header.byte_rate = header.sample_rate * header.block_align;
  header.audio_format = bits_per_sample == 32u ? 3u : 1u;
  if (byte_count > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("render payload too large");
  }
  header.data_size = static_cast<std::uint32_t>(byte_count);
  header.chunk_size = 36u + header.data_size;

  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::ios_base::failure("Unable to open WAV target");
  }

  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (byte_count > 0) {
    stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(byte_count));
  }
  if (!stream) {
    throw std::ios_base::failure("Failed to write WAV payload");
  }
}

inline void WriteWaveFile(const std::filesystem::path& path, std::uint32_t sample_rate,
                          std::uint16_t channels, std::uint16_t bits_per_sample,
                          const std::vector<std::uint8_t>& data) {
  WriteWaveFile(path, sample_rate, channels, bits_per_sample, data.empty() ? nullptr : data.data(),
                data.size());
}

} // namespace orpheus::core::render
