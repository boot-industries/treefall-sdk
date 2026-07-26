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

static_assert(sizeof(WavHeader) == 44);

inline WavHeader MakeWavHeader(std::uint32_t sample_rate, std::uint16_t channels,
                               std::uint16_t bits_per_sample, std::uint64_t total_data_bytes) {
  if (sample_rate == 0 || channels == 0 || bits_per_sample == 0) {
    throw std::invalid_argument("WAV format fields must be non-zero");
  }
  if (total_data_bytes > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("render payload too large for RIFF/WAV");
  }

  const std::uint32_t bytes_per_sample = (static_cast<std::uint32_t>(bits_per_sample) + 7u) / 8u;
  const std::uint64_t block_align =
      static_cast<std::uint64_t>(channels) * static_cast<std::uint64_t>(bytes_per_sample);
  if (block_align > std::numeric_limits<std::uint16_t>::max()) {
    throw std::invalid_argument("WAV block alignment exceeds format limit");
  }
  const std::uint64_t byte_rate = static_cast<std::uint64_t>(sample_rate) * block_align;
  if (byte_rate > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("WAV byte rate exceeds format limit");
  }

  const std::uint32_t data_size = static_cast<std::uint32_t>(total_data_bytes);
  if (data_size > std::numeric_limits<std::uint32_t>::max() - 36u) {
    throw std::invalid_argument("WAV RIFF chunk size exceeds format limit");
  }

  WavHeader header;
  header.num_channels = channels;
  header.sample_rate = sample_rate;
  header.byte_rate = static_cast<std::uint32_t>(byte_rate);
  header.block_align = static_cast<std::uint16_t>(block_align);
  header.bits_per_sample = bits_per_sample;
  header.audio_format = bits_per_sample == 32u ? 3u : 1u;
  header.data_size = data_size;
  header.chunk_size = 36u + data_size;
  return header;
}

class WavStreamWriter {
public:
  WavStreamWriter(const std::filesystem::path& path, std::uint32_t sample_rate,
                  std::uint16_t channels, std::uint16_t bits_per_sample,
                  std::uint64_t total_data_bytes)
      : total_data_bytes_(total_data_bytes) {
    const WavHeader header =
        MakeWavHeader(sample_rate, channels, bits_per_sample, total_data_bytes);
    if (!path.parent_path().empty()) {
      std::filesystem::create_directories(path.parent_path());
    }
    stream_.open(path, std::ios::binary);
    if (!stream_) {
      throw std::ios_base::failure("Unable to open WAV target");
    }
    stream_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!stream_) {
      throw std::ios_base::failure("Failed to write WAV header");
    }
  }

  WavStreamWriter(const WavStreamWriter&) = delete;
  WavStreamWriter& operator=(const WavStreamWriter&) = delete;

  void write(const std::uint8_t* data, std::size_t byte_count) {
    if (closed_) {
      throw std::logic_error("Cannot write a closed WAV stream");
    }
    if (byte_count == 0) {
      return;
    }
    if (data == nullptr || byte_count > total_data_bytes_ - written_data_bytes_) {
      throw std::invalid_argument("WAV payload write exceeds declared size");
    }
    if (byte_count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
      throw std::invalid_argument("WAV payload write exceeds stream size");
    }

    stream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(byte_count));
    if (!stream_) {
      throw std::ios_base::failure("Failed to write WAV payload");
    }
    written_data_bytes_ += byte_count;
  }

  void close() {
    if (closed_) {
      return;
    }
    if (written_data_bytes_ != total_data_bytes_) {
      stream_.close();
      closed_ = true;
      throw std::ios_base::failure("WAV payload size does not match declared size");
    }
    stream_.close();
    closed_ = true;
    if (!stream_) {
      throw std::ios_base::failure("Failed to close WAV target");
    }
  }

private:
  std::ofstream stream_;
  std::uint64_t total_data_bytes_ = 0;
  std::uint64_t written_data_bytes_ = 0;
  bool closed_ = false;
};

inline void WriteWaveFile(const std::filesystem::path& path, std::uint32_t sample_rate,
                          std::uint16_t channels, std::uint16_t bits_per_sample,
                          const std::uint8_t* data, std::size_t byte_count) {
  WavStreamWriter writer(path, sample_rate, channels, bits_per_sample, byte_count);
  writer.write(data, byte_count);
  writer.close();
}

inline void WriteWaveFile(const std::filesystem::path& path, std::uint32_t sample_rate,
                          std::uint16_t channels, std::uint16_t bits_per_sample,
                          const std::vector<std::uint8_t>& data) {
  WriteWaveFile(path, sample_rate, channels, bits_per_sample, data.empty() ? nullptr : data.data(),
                data.size());
}

} // namespace orpheus::core::render
