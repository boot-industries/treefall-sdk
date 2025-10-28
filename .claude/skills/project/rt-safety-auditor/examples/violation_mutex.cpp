// Example: VIOLATION - Mutex Lock in Audio Thread
// This demonstrates priority inversion risk

#include <atomic>
#include <mutex>

class UnsafeAudioMixer {
public:
  // VIOLATION: Mutex lock in audio thread causes priority inversion
  void processBlock(float** outputs, int numFrames) {
    // VIOLATION: lock_guard blocks until mutex is acquired
    std::lock_guard<std::mutex> lock(gainMutex_);

    float gain = currentGain_; // Protected by mutex

    for (int i = 0; i < numFrames; ++i) {
      outputs[0][i] *= gain;
    }
  }

  // UI thread
  void setGain(float gain) {
    std::lock_guard<std::mutex> lock(gainMutex_);
    currentGain_ = gain;
  }

private:
  std::mutex gainMutex_; // VIOLATION: Mutex in audio path
  float currentGain_{1.0f};
};

// FIX: Use atomic instead of mutex
class SafeAudioMixer {
public:
  // SAFE: Lock-free atomic operation
  void processBlock(float** outputs, int numFrames) {
    // SAFE: Atomic load is wait-free
    float gain = currentGain_.load(std::memory_order_acquire);

    for (int i = 0; i < numFrames; ++i) {
      outputs[0][i] *= gain;
    }
  }

  // UI thread
  void setGain(float gain) {
    // SAFE: Atomic store is wait-free
    currentGain_.store(gain, std::memory_order_release);
  }

private:
  std::atomic<float> currentGain_{1.0f}; // SAFE: Lock-free
};
