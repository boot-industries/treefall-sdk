// SPDX-License-Identifier: MIT
#include <orpheus/audio_input.h>

#include "../common/realtime_counter.h"
#include "../common/spsc_observation.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
namespace orpheus {
namespace {

size_t roundUpPowerOfTwo(size_t value) {
  if (value <= 1) {
    return 1;
  }
  constexpr size_t highestPowerOfTwo = size_t{1} << (std::numeric_limits<size_t>::digits - 1);
  if (value > highestPowerOfTwo) {
    throw std::length_error("AudioInputRing capacity cannot be rounded to a power of two");
  }
  size_t result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

size_t checkedElementCount(size_t frames, uint16_t channels) {
  if (frames > std::numeric_limits<size_t>::max() / static_cast<size_t>(channels)) {
    throw std::length_error("AudioInputRing element count overflows size_t");
  }
  const size_t elements = frames * static_cast<size_t>(channels);
  const size_t maxElements = std::vector<float>{}.max_size();
  if (elements > maxElements || elements > std::numeric_limits<size_t>::max() / sizeof(float)) {
    throw std::length_error("AudioInputRing allocation exceeds vector capacity");
  }
  return elements;
}

} // namespace

// ============================================================================
// AudioInputRing
// ============================================================================

AudioInputRing::AudioInputRing(uint16_t numChannels, size_t capacityFrames)
    : m_numChannels(numChannels),
      m_capacityFrames(roundUpPowerOfTwo(std::max<size_t>(2, capacityFrames))) {
  if (numChannels == 0) {
    throw std::invalid_argument("AudioInputRing requires at least one channel");
  }
  m_data.assign(checkedElementCount(m_capacityFrames, m_numChannels), 0.0f);
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
    detail::publishSaturatingIncrement(m_overflows);
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
  return detail::observeBoundedPending(m_readIndex, m_writeIndex, m_capacityFrames);
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
