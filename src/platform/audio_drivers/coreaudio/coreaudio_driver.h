// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace orpheus {

// Forward declaration
class IPerformanceMonitor;

/// CoreAudio driver implementation for macOS
///
/// Provides low-latency audio I/O using AudioUnit (HAL Output).
/// Supports device enumeration, configurable sample rate/buffer size,
/// and latency reporting.
///
/// Thread Safety:
/// - initialize(), start(), stop(): UI thread only
/// - isRunning(), getConfig(), getDriverName(), getLatencySamples(): Thread-safe
/// - Audio callback: Real-time audio thread (lock-free)
class CoreAudioDriver : public IAudioDriver {
public:
  CoreAudioDriver();
  ~CoreAudioDriver() override;

  // IAudioDriver interface
  SessionGraphError initialize(const AudioDriverConfig& config) override;
  SessionGraphError start(IAudioCallback* callback) override;
  SessionGraphError stop() override;
  bool isRunning() const override;
  const AudioDriverConfig& getConfig() const override;
  std::string getDriverName() const override;
  uint32_t getLatencySamples() const override;

  /// Set performance monitor for audio metrics tracking
  /// @param monitor Performance monitor instance (can be nullptr to disable)
  /// @note Thread-safe: Can be called before or after start()
  void setPerformanceMonitor(IPerformanceMonitor* monitor);

private:
  /// Audio Unit render callback (invoked on audio thread)
  static OSStatus renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                 UInt32 inNumberFrames, AudioBufferList* ioData);

  /// Enumerate available audio devices
  /// @return Vector of device IDs
  std::vector<AudioDeviceID> enumerateDevices();

  /// Find device by name or get default device
  /// @param device_name Device name (empty = default device)
  /// @return Device ID or 0 if not found
  AudioDeviceID findDevice(const std::string& device_name);

  /// Get device name from device ID
  /// @param device_id Device ID
  /// @return Device name or empty string on failure
  std::string getDeviceName(AudioDeviceID device_id);

  /// Query device latency
  /// @param device_id Device ID
  /// @return Latency in samples
  uint32_t queryDeviceLatency(AudioDeviceID device_id);

  /// Set up AudioUnit with configuration
  /// @param device_id Device to use
  /// @return SessionGraphError::OK on success
  SessionGraphError setupAudioUnit(AudioDeviceID device_id);

  /// Cleanup AudioUnit resources
  void cleanupAudioUnit();

  // Configuration
  AudioDriverConfig config_;

  // CoreAudio state
  AudioUnit audio_unit_{nullptr};
  AudioDeviceID device_id_{0};
  std::atomic<bool> is_running_{false};
  std::atomic<uint32_t> latency_samples_{0};

  // Callback
  IAudioCallback* callback_{nullptr};

  // Performance monitoring (optional)
  IPerformanceMonitor* performance_monitor_{nullptr};

  // Audio thread buffers (allocated once in initialize)
  std::vector<float*> input_buffers_;
  std::vector<float*> output_buffers_;
  std::vector<float> input_storage_;  // Backing storage for input buffers
  std::vector<float> output_storage_; // Backing storage for output buffers

  // Thread safety
  mutable std::mutex mutex_;
};

} // namespace orpheus
