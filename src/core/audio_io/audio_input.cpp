// SPDX-License-Identifier: MIT
#include <orpheus/audio_input.h>

#include <algorithm>
#include <cstring>

namespace orpheus {

namespace {

size_t roundUpPowerOfTwo(size_t value) {
  size_t result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

} // namespace

// ============================================================================
// AudioInputRing
// ============================================================================

AudioInputRing::AudioInputRing(uint16_t numChannels, size_t capacityFrames)
    : m_numChannels(numChannels == 0 ? 1 : numChannels),
      m_capacityFrames(roundUpPowerOfTwo(std::max<size_t>(2, capacityFrames))) {
  m_data.assign(m_capacityFrames * m_numChannels, 0.0f);
}

size_t AudioInputRing::write(const float* interleaved, size_t frames) {
  if (interleaved == nullptr || frames == 0) {
    return 0;
  }

  const size_t writeIndex = m_writeIndex.load(std::memory_order_relaxed);
  const size_t readIndex = m_readIndex.load(std::memory_order_acquire);
  const size_t used = writeIndex - readIndex; // wraps correctly (unsigned)
  const size_t freeFrames = m_capacityFrames - used;

  if (frames > freeFrames) {
    // All-or-nothing: dropping the whole buffer keeps frames contiguous and
    // never blocks the audio thread.
    m_overflows.fetch_add(1, std::memory_order_relaxed);
    return 0;
  }

  const size_t mask = m_capacityFrames - 1;
  size_t written = 0;
  while (written < frames) {
    const size_t slot = (writeIndex + written) & mask;
    const size_t contiguous = std::min(frames - written, m_capacityFrames - slot);
    std::memcpy(m_data.data() + slot * m_numChannels, interleaved + written * m_numChannels,
                contiguous * m_numChannels * sizeof(float));
    written += contiguous;
  }

  m_writeIndex.store(writeIndex + frames, std::memory_order_release);
  return frames;
}

size_t AudioInputRing::read(float* dest, size_t maxFrames) {
  if (dest == nullptr || maxFrames == 0) {
    return 0;
  }

  const size_t readIndex = m_readIndex.load(std::memory_order_relaxed);
  const size_t writeIndex = m_writeIndex.load(std::memory_order_acquire);
  const size_t available = writeIndex - readIndex;
  const size_t toRead = std::min(maxFrames, available);
  if (toRead == 0) {
    return 0;
  }

  const size_t mask = m_capacityFrames - 1;
  size_t done = 0;
  while (done < toRead) {
    const size_t slot = (readIndex + done) & mask;
    const size_t contiguous = std::min(toRead - done, m_capacityFrames - slot);
    std::memcpy(dest + done * m_numChannels, m_data.data() + slot * m_numChannels,
                contiguous * m_numChannels * sizeof(float));
    done += contiguous;
  }

  m_readIndex.store(readIndex + toRead, std::memory_order_release);
  return toRead;
}

size_t AudioInputRing::framesAvailable() const {
  return m_writeIndex.load(std::memory_order_acquire) - m_readIndex.load(std::memory_order_acquire);
}

// ============================================================================
// Ring-backed IAudioInputStream
// ============================================================================

namespace {

class RingAudioInputStream : public IAudioInputStream {
public:
  explicit RingAudioInputStream(const AudioInputStreamConfig& config)
      : m_config(config), m_ring(config.num_channels, config.ring_capacity_frames) {}

  uint16_t numChannels() const override {
    return m_ring.numChannels();
  }
  uint32_t sampleRate() const override {
    return m_config.sample_rate;
  }
  size_t capture(const float* interleaved, size_t frames) override {
    return m_ring.write(interleaved, frames);
  }
  size_t drain(float* dest, size_t maxFrames) override {
    return m_ring.read(dest, maxFrames);
  }
  size_t framesPending() const override {
    return m_ring.framesAvailable();
  }
  uint64_t overflowCount() const override {
    return m_ring.overflowCount();
  }

private:
  AudioInputStreamConfig m_config;
  AudioInputRing m_ring;
};

} // namespace

std::unique_ptr<IAudioInputStream> createAudioInputStream(const AudioInputStreamConfig& config) {
  return std::make_unique<RingAudioInputStream>(config);
}

} // namespace orpheus
