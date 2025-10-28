// Example: VIOLATION - Heap Allocation in Audio Thread
// This demonstrates incorrect code that will be flagged by the auditor

#include <vector>

class UnsafeAudioProcessor {
public:
  // VIOLATION: Heap allocation in audio thread
  void processBlock(float** outputs, int numFrames) {
    // VIOLATION: new[] allocates on heap - unbounded latency!
    float* tempBuffer = new float[numFrames];

    // Process audio
    for (int i = 0; i < numFrames; ++i) {
      tempBuffer[i] = outputs[0][i] * 0.5f;
    }

    // VIOLATION: delete[] deallocates - may trigger system calls!
    delete[] tempBuffer;

    // VIOLATION: malloc also allocates on heap
    void* memory = malloc(numFrames * sizeof(float));
    // ... use memory
    free(memory); // VIOLATION: free deallocates

    // VIOLATION: vector::push_back may allocate
    std::vector<float> samples;
    for (int i = 0; i < numFrames; ++i) {
      samples.push_back(outputs[0][i]); // VIOLATION!
    }
  }
};

// FIX: Pre-allocate in constructor
class SafeAudioProcessor {
public:
  SafeAudioProcessor() {
    // SAFE: Allocate in constructor (UI thread)
    tempBuffer_.resize(kMaxFrames);
  }

  void processBlock(float** outputs, int numFrames) {
    // SAFE: Use pre-allocated buffer
    for (int i = 0; i < numFrames; ++i) {
      tempBuffer_[i] = outputs[0][i] * 0.5f;
    }
  }

private:
  static constexpr int kMaxFrames = 2048;
  std::vector<float> tempBuffer_; // Pre-allocated
};
