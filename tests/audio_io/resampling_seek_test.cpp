// SPDX-License-Identifier: MIT
//
// ResamplingAudioFileReader::seek must land on exactly the samples a whole-file
// decode produces at the same target-rate position: same FIR phase and a fully
// primed FIR history, so page seeks introduce no seam.

#include "resampling_audio_file_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace orpheus;

namespace {

// Deterministic in-memory source: every sample is a pure function of its frame
// index and channel, so any read window is reproducible.
class SyntheticReader final : public IAudioFileReader {
public:
  SyntheticReader(uint32_t rate, uint16_t channels, int64_t frames)
      : m_rate(rate), m_channels(channels), m_frames(frames) {}

  Result<AudioFileMetadata> open(const std::string&) override {
    m_open = true;
    m_position = 0;
    Result<AudioFileMetadata> result{};
    result.value.format = AudioFileFormat::WAV;
    result.value.sample_rate = m_rate;
    result.value.num_channels = m_channels;
    result.value.duration_samples = m_frames;
    result.value.bit_depth = 32;
    result.error = SessionGraphError::OK;
    return result;
  }

  Result<size_t> readSamples(float* buffer, size_t num_samples) override {
    const size_t available = static_cast<size_t>(std::max<int64_t>(0, m_frames - m_position));
    const size_t give = std::min(num_samples, available);
    for (size_t frame = 0; frame < give; ++frame) {
      const double t = static_cast<double>(m_position + static_cast<int64_t>(frame));
      for (uint16_t channel = 0; channel < m_channels; ++channel) {
        buffer[frame * m_channels + channel] = static_cast<float>(
            0.5 * std::sin(0.0021 * t * static_cast<double>(channel + 1)) +
            0.25 * std::sin(0.61 * t + static_cast<double>(channel)));
      }
    }
    m_position += static_cast<int64_t>(give);
    return {give, SessionGraphError::OK, {}};
  }

  SessionGraphError seek(int64_t sample_position) override {
    m_position = std::clamp<int64_t>(sample_position, 0, m_frames);
    return SessionGraphError::OK;
  }

  void close() override { m_open = false; }
  int64_t getCurrentPosition() const override { return m_position; }
  bool isOpen() const override { return m_open; }

private:
  uint32_t m_rate;
  uint16_t m_channels;
  int64_t m_frames;
  int64_t m_position{0};
  bool m_open{false};
};

struct RatePair {
  uint32_t source;
  uint32_t target;
};

TEST(ResamplingSeekTest, SeekMatchesWholeFileDecodeBitForBit) {
  constexpr uint16_t kChannels = 2;
  constexpr size_t kReadFrames = 4096;
  constexpr int64_t kTargetFrames = 3 * 65536 + 1 + static_cast<int64_t>(kReadFrames) + 8192;

  for (const RatePair pair : {RatePair{44100, 48000}, RatePair{48000, 44100},
                              RatePair{96000, 44100}, RatePair{192000, 44100}}) {
    SCOPED_TRACE("source=" + std::to_string(pair.source) +
                 " target=" + std::to_string(pair.target));
    const auto sourceFrames = static_cast<int64_t>(
        std::ceil(static_cast<double>(kTargetFrames) * static_cast<double>(pair.source) /
                  static_cast<double>(pair.target)));

    // Reference: one continuous decode of the whole file.
    std::vector<float> whole;
    {
      ResamplingAudioFileReader reader(
          std::make_shared<SyntheticReader>(pair.source, kChannels, sourceFrames), pair.target);
      ASSERT_TRUE(reader.open("").isOk());
      std::vector<float> chunk(kReadFrames * kChannels);
      while (true) {
        const auto read = reader.readSamples(chunk.data(), kReadFrames);
        ASSERT_TRUE(read.isOk());
        if (read.value == 0) {
          break;
        }
        whole.insert(whole.end(), chunk.begin(),
                     chunk.begin() + static_cast<std::ptrdiff_t>(read.value * kChannels));
      }
    }
    ASSERT_GE(whole.size() / kChannels, static_cast<size_t>(3 * 65536 + 1) + kReadFrames);

    std::vector<int64_t> targets{0, 12345};
    for (int64_t k = 1; k <= 3; ++k) {
      targets.push_back(65536 * k - 1);
      targets.push_back(65536 * k);
      targets.push_back(65536 * k + 1);
    }

    // One reader, sought repeatedly (forward and backward), as the streaming
    // worker does when it services page demands.
    ResamplingAudioFileReader reader(
        std::make_shared<SyntheticReader>(pair.source, kChannels, sourceFrames), pair.target);
    ASSERT_TRUE(reader.open("").isOk());
    std::vector<float> got(kReadFrames * kChannels);
    for (const bool reversed : {false, true}) {
      if (reversed) {
        std::reverse(targets.begin(), targets.end());
      }
      for (const int64_t target : targets) {
        SCOPED_TRACE("seek=" + std::to_string(target));
        ASSERT_EQ(reader.seek(target), SessionGraphError::OK);
        EXPECT_EQ(reader.getCurrentPosition(), target);
        const auto read = reader.readSamples(got.data(), kReadFrames);
        ASSERT_TRUE(read.isOk());
        ASSERT_EQ(read.value, kReadFrames);
        EXPECT_EQ(reader.getCurrentPosition(), target + static_cast<int64_t>(kReadFrames));
        const float* expected = whole.data() + static_cast<size_t>(target) * kChannels;
        EXPECT_EQ(std::memcmp(got.data(), expected, got.size() * sizeof(float)), 0);
      }
    }
  }
}

} // namespace
