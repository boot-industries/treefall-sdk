// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace orpheus::tests::support {

struct ParsedWav {
  std::uint32_t sample_rate{};
  std::uint16_t channels{};
  std::uint16_t bits_per_sample{};
  std::uint16_t audio_format{};
  std::vector<std::uint8_t> data;
};

inline ParsedWav ReadWav(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to open WAV: " + path.string());
  }

  auto ReadBytes = [&](char* buffer, std::size_t count) {
    stream.read(buffer, static_cast<std::streamsize>(count));
    if (!stream) {
      throw std::runtime_error("Failed to read WAV: " + path.string());
    }
  };

  char riff[4];
  ReadBytes(riff, sizeof(riff));
  if (std::string(riff, sizeof(riff)) != "RIFF") {
    throw std::runtime_error("Not a RIFF WAV file: " + path.string());
  }

  std::uint32_t riff_size = 0;
  ReadBytes(reinterpret_cast<char*>(&riff_size), sizeof(riff_size));
  (void)riff_size;

  char wave[4];
  ReadBytes(wave, sizeof(wave));
  if (std::string(wave, sizeof(wave)) != "WAVE") {
    throw std::runtime_error("Invalid WAV header: " + path.string());
  }

  ParsedWav result;
  bool have_fmt = false;
  bool have_data = false;

  while (!have_fmt || !have_data) {
    char chunk_id[4];
    if (!stream.read(chunk_id, sizeof(chunk_id))) {
      throw std::runtime_error("Unexpected EOF in WAV: " + path.string());
    }
    std::uint32_t chunk_size = 0;
    ReadBytes(reinterpret_cast<char*>(&chunk_size), sizeof(chunk_size));

    if (std::string(chunk_id, sizeof(chunk_id)) == "fmt ") {
      std::vector<char> fmt(chunk_size);
      ReadBytes(fmt.data(), fmt.size());
      if (chunk_size < 16) {
        throw std::runtime_error("Unsupported fmt chunk size");
      }
      result.audio_format = static_cast<std::uint16_t>(static_cast<unsigned char>(fmt[0]) |
                                                       (static_cast<unsigned char>(fmt[1]) << 8));
      result.channels = static_cast<std::uint16_t>(static_cast<unsigned char>(fmt[2]) |
                                                   (static_cast<unsigned char>(fmt[3]) << 8));
      result.sample_rate = static_cast<std::uint32_t>(
          static_cast<unsigned char>(fmt[4]) | (static_cast<unsigned char>(fmt[5]) << 8) |
          (static_cast<unsigned char>(fmt[6]) << 16) | (static_cast<unsigned char>(fmt[7]) << 24));
      result.bits_per_sample = static_cast<std::uint16_t>(
          static_cast<unsigned char>(fmt[14]) | (static_cast<unsigned char>(fmt[15]) << 8));
      have_fmt = true;
      if (chunk_size & 1u) {
        stream.ignore(1);
      }
    } else if (std::string(chunk_id, sizeof(chunk_id)) == "data") {
      result.data.resize(chunk_size);
      if (chunk_size > 0) {
        ReadBytes(reinterpret_cast<char*>(result.data.data()), chunk_size);
      }
      have_data = true;
      if (chunk_size & 1u) {
        stream.ignore(1);
      }
    } else {
      stream.ignore(static_cast<std::streamsize>(chunk_size));
      if (chunk_size & 1u) {
        stream.ignore(1);
      }
    }
  }

  return result;
}

} // namespace orpheus::tests::support
