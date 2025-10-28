// Example: Correct Atomic Usage Patterns
// Demonstrates lock-free inter-thread communication

#include <array>
#include <atomic>
#include <cstdint>

class ProperAtomicUsage {
public:
  // SAFE: Simple atomic flag
  void setEnabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
  }

  bool isEnabled() const {
    return enabled_.load(std::memory_order_acquire);
  }

  // SAFE: Atomic counter (sample-accurate timing)
  void advanceClock(int numFrames) {
    // SAFE: Atomic increment
    sampleCount_.fetch_add(numFrames, std::memory_order_relaxed);
  }

  uint64_t getCurrentSample() const {
    return sampleCount_.load(std::memory_order_relaxed);
  }

  // SAFE: Producer-consumer pattern with memory ordering
  void updateGain(float newGain) {
    // Producer (UI thread)
    nextGain_ = newGain;
    gainReady_.store(true, std::memory_order_release);
  }

  void processBlock(float** outputs, int numFrames) {
    // Consumer (audio thread)
    if (gainReady_.load(std::memory_order_acquire)) {
      currentGain_ = nextGain_;
      gainReady_.store(false, std::memory_order_release);
    }

    // Use current gain
    for (int i = 0; i < numFrames; ++i) {
      outputs[0][i] *= currentGain_;
    }
  }

  // SAFE: Atomic exchange (test-and-set)
  void triggerEvent() {
    eventTriggered_.store(true, std::memory_order_release);
  }

  bool checkAndClearEvent() {
    // SAFE: Exchange returns old value and sets new value atomically
    return eventTriggered_.exchange(false, std::memory_order_acq_rel);
  }

  // SAFE: Compare-and-swap for complex updates
  void incrementIfPositive() {
    int current = counter_.load(std::memory_order_relaxed);
    int desired;

    do {
      if (current <= 0)
        break; // Don't increment if non-positive
      desired = current + 1;
    } while (!counter_.compare_exchange_weak(current, desired, std::memory_order_release,
                                             std::memory_order_relaxed));
  }

private:
  // SAFE: Atomic primitives for lock-free communication
  std::atomic<bool> enabled_{false};
  std::atomic<uint64_t> sampleCount_{0};
  std::atomic<bool> gainReady_{false};
  std::atomic<bool> eventTriggered_{false};
  std::atomic<int> counter_{0};

  // Non-atomic data protected by memory ordering
  float nextGain_{1.0f};
  float currentGain_{1.0f};
};

// SAFE: Lock-free ring buffer (single producer, single consumer)
template <typename T, size_t N> class LockFreeRingBuffer {
public:
  bool push(const T& item) {
    uint32_t write = writePos_.load(std::memory_order_relaxed);
    uint32_t nextWrite = (write + 1) % N;

    // Check if buffer is full
    if (nextWrite == readPos_.load(std::memory_order_acquire)) {
      return false; // Buffer full
    }

    // Write data
    buffer_[write] = item;

    // Update write position with release semantics
    writePos_.store(nextWrite, std::memory_order_release);
    return true;
  }

  bool pop(T& item) {
    uint32_t read = readPos_.load(std::memory_order_relaxed);

    // Check if buffer is empty
    if (read == writePos_.load(std::memory_order_acquire)) {
      return false; // Buffer empty
    }

    // Read data
    item = buffer_[read];

    // Update read position with release semantics
    uint32_t nextRead = (read + 1) % N;
    readPos_.store(nextRead, std::memory_order_release);
    return true;
  }

private:
  std::array<T, N> buffer_;
  std::atomic<uint32_t> writePos_{0};
  std::atomic<uint32_t> readPos_{0};
};
