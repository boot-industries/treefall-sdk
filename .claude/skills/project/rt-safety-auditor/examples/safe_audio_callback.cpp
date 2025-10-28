// Example: Real-Time Safe Audio Callback
// This demonstrates correct patterns for audio thread code

#include <array>
#include <atomic>
#include <cstring>

class SafeAudioProcessor {
public:
  SafeAudioProcessor() {
    // SAFE: Pre-allocate in constructor (UI thread)
    scratchBuffer_.resize(kMaxFrames * kMaxChannels);
  }

  // SAFE: Audio thread callback - no allocations, no locks, bounded execution
  void processBlock(float** outputs, int numFrames, int numChannels) {
    // SAFE: Early return for fast path
    if (!isPlaying_.load(std::memory_order_acquire)) {
      // Output silence
      for (int ch = 0; ch < numChannels; ++ch) {
        std::memset(outputs[ch], 0, numFrames * sizeof(float));
      }
      return;
    }

    // SAFE: Stack allocation (bounded size)
    float mixBuffer[kMaxFrames];

    // SAFE: Atomic load (lock-free)
    float gain = currentGain_.load(std::memory_order_acquire);

    // SAFE: Bounded loop
    for (int frame = 0; frame < numFrames; ++frame) {
      // SAFE: Sample-accurate processing
      uint64_t samplePos = playheadSamples_.load(std::memory_order_relaxed);

      // SAFE: Fixed arithmetic
      float sample = processSample(samplePos + frame);

      // SAFE: Apply gain
      mixBuffer[frame] = sample * gain;
    }

    // SAFE: Copy to output
    for (int ch = 0; ch < numChannels; ++ch) {
      std::memcpy(outputs[ch], mixBuffer, numFrames * sizeof(float));
    }

    // SAFE: Atomic update
    playheadSamples_.fetch_add(numFrames, std::memory_order_relaxed);
  }

  // UI thread interface
  void setPlaying(bool playing) {
    isPlaying_.store(playing, std::memory_order_release);
  }

  void setGain(float gain) {
    currentGain_.store(gain, std::memory_order_release);
  }

private:
  float processSample(uint64_t samplePos) {
    // SAFE: Lookup table instead of computation
    int tableIndex = samplePos % kTableSize;
    return sineTable_[tableIndex];
  }

  // SAFE: Compile-time constants
  static constexpr int kMaxFrames = 2048;
  static constexpr int kMaxChannels = 2;
  static constexpr int kTableSize = 4096;

  // SAFE: Fixed-size containers (no runtime allocation)
  std::array<float, kTableSize> sineTable_;

  // SAFE: Pre-allocated buffer (resized once in constructor)
  std::vector<float> scratchBuffer_;

  // SAFE: Atomic variables for cross-thread communication
  std::atomic<bool> isPlaying_{false};
  std::atomic<float> currentGain_{1.0f};
  std::atomic<uint64_t> playheadSamples_{0};
};
