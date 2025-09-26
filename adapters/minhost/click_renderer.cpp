#include "click_renderer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <fstream>
#include <stdexcept>

namespace orpheus::minhost {

namespace {
constexpr int kChannels = 1;
constexpr int kBitsPerSample = 16;
constexpr double kClickFrequency = 1000.0;
constexpr double kClickDurationSeconds = 0.1;
constexpr int kBeatsPerBar = 4;

std::vector<int16_t> GenerateClickTrack(int sampleRate, double bpm, int bars) {
  if (sampleRate <= 0 || bpm <= 0.0 || bars <= 0) {
    throw std::invalid_argument("Invalid rendering parameters");
  }

  const int samplesPerBeat = static_cast<int>(
      std::round(static_cast<double>(sampleRate) * 60.0 / bpm));
  const int clickSamples = static_cast<int>(
      std::round(kClickDurationSeconds * static_cast<double>(sampleRate)));

  const int totalBeats = bars * kBeatsPerBar;
  const int totalSamples = samplesPerBeat * totalBeats;

  std::vector<int16_t> samples(static_cast<size_t>(totalSamples), 0);
  const double twoPiF = 2.0 * std::numbers::pi * kClickFrequency;

  for (int beat = 0; beat < totalBeats; ++beat) {
    const int offset = beat * samplesPerBeat;
    for (int i = 0; i < clickSamples && (offset + i) < totalSamples; ++i) {
      const double envelope =
          0.5 * (1.0 - std::cos(std::numbers::pi * i / clickSamples));
      const double value = std::sin(twoPiF * i / sampleRate) * envelope;
      samples[static_cast<size_t>(offset + i)] =
          static_cast<int16_t>(std::clamp(value, -1.0, 1.0) * 32767);
    }
  }

  return samples;
}

}  // namespace

void ClickRenderer::RenderClick(const std::string &path, int sampleRate,
                                double bpm, int bars) const {
  auto samples = GenerateClickTrack(sampleRate, bpm, bars);
  WriteWav(path, sampleRate, samples);
}

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize = 0;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmtChunkSize = 16;
  uint16_t audioFormat = 1;
  uint16_t numChannels = kChannels;
  uint32_t sampleRate = 0;
  uint32_t byteRate = 0;
  uint16_t blockAlign = 0;
  uint16_t bitsPerSample = kBitsPerSample;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t dataSize = 0;
};

void ClickRenderer::WriteWav(const std::string &path, int sampleRate,
                             const std::vector<int16_t> &samples) {
  const uint32_t dataBytes =
      static_cast<uint32_t>(samples.size() * sizeof(int16_t));

  WavHeader header;
  header.sampleRate = static_cast<uint32_t>(sampleRate);
  header.byteRate =
      header.sampleRate * kChannels * (kBitsPerSample / 8);
  header.blockAlign = kChannels * (kBitsPerSample / 8);
  header.dataSize = dataBytes;
  header.chunkSize = 36 + header.dataSize;

  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to open output file: " + path);
  }

  stream.write(reinterpret_cast<const char *>(&header), sizeof(header));
  stream.write(reinterpret_cast<const char *>(samples.data()), dataBytes);
  if (!stream) {
    throw std::runtime_error("Failed to write WAV data");
  }
}

}  // namespace orpheus::minhost
