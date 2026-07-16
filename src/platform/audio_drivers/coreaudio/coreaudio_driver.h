// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <cstdint>
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
  AudioDriverCapabilities getCapabilities() const override;

  /// Set performance monitor for audio metrics tracking
  /// @param monitor Performance monitor instance (can be nullptr to disable)
  /// @note Thread-safe: Can be called before or after start()
  void setPerformanceMonitor(IPerformanceMonitor* monitor) override;

  /// Count of AudioUnitRender calls on the capture bus that returned a
  /// non-noErr status since the last initialize(). A structural regression
  /// guard: input_buffers_ are always non-null once allocated (pre-zeroed),
  /// so a null-buffer check alone cannot distinguish "captured real silence"
  /// from "AudioUnitRender has failed on every single block" (e.g. capturing
  /// against a device with no input channels -- see
  /// resolveInputOutputDevice()). Thread-safe; read from any thread.
  uint64_t getInputRenderFailureCount() const;

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

  /// Get the system default device for a given selector (e.g.
  /// kAudioHardwarePropertyDefaultOutputDevice / ...DefaultInputDevice).
  /// @return Device ID or 0 on failure
  AudioDeviceID getDefaultDevice(AudioObjectPropertySelector selector);

  /// Resolve the device to open when the host requests capture
  /// (config_.num_inputs > 0) and no device name was specified. The default
  /// input and default output devices are frequently *different* HAL
  /// AudioDeviceIDs (e.g. a MacBook's built-in microphone and built-in
  /// speakers) even though a single AUHAL unit can only address one
  /// kAudioOutputUnitProperty_CurrentDevice. When they differ, bridges the
  /// two with a private CoreAudio Aggregate Device so bus 0 (playback) and
  /// bus 1 (capture) each reach real hardware; falls back to the default
  /// output device alone if aggregation fails.
  /// @return Device ID (possibly an aggregate) or 0 on failure
  AudioDeviceID resolveInputOutputDevice();

  /// Create a private, non-stacked Aggregate Device combining a capture
  /// sub-device and a playback sub-device so one AUHAL unit can drive both.
  /// Torn down in cleanupAudioUnit() via AudioHardwareDestroyAggregateDevice.
  /// @return Aggregate device ID, or 0 on failure
  AudioDeviceID createAggregateDevice(AudioDeviceID input_device_id,
                                      AudioDeviceID output_device_id);

  /// Get device name from device ID
  /// @param device_id Device ID
  /// @return Device name or empty string on failure
  std::string getDeviceName(AudioDeviceID device_id);

  /// Query the complete capture-to-playback latency of the active physical
  /// routes, including device latency, safety offsets, actual I/O buffer
  /// depth, and AudioUnit processing latency. Queried live so a consumer
  /// route's reported delay is refreshed for every host take.
  /// @return Round-trip latency in samples, or 0 when it cannot be detected
  uint32_t queryDeviceLatency() const;

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
  // Set only when resolveInputOutputDevice() had to bridge separate default
  // input/output devices; torn down in cleanupAudioUnit().
  AudioDeviceID aggregate_device_id_{0};
  // Physical endpoints behind device_id_. These differ when device_id_ is the
  // private aggregate used to bridge separate default input/output devices.
  AudioDeviceID input_device_id_{0};
  AudioDeviceID output_device_id_{0};
  std::atomic<bool> is_running_{false};
  std::atomic<uint64_t> input_render_failures_{0};

  // Callback
  IAudioCallback* callback_{nullptr};

  // Performance monitoring (optional, only read by diagnostics callback builds)
  std::atomic<IPerformanceMonitor*> performance_monitor_{nullptr};
  std::atomic<uint32_t> callbacks_in_flight_{0};
  int64_t expected_stream_sample_{0};
  bool stream_timeline_initialized_{false};

  // Audio thread buffers (allocated once in initialize)
  std::vector<float*> input_buffers_;
  std::vector<float*> output_buffers_;
  std::vector<float> input_storage_;  // Backing storage for input buffers
  std::vector<float> output_storage_; // Backing storage for output buffers

  // Pre-allocated AudioBufferList used to pull captured input in the render
  // callback via AudioUnitRender (bus 1). Sized in initialize() when
  // num_inputs > 0 so the audio thread never allocates. Backed by raw bytes
  // because AudioBufferList has a trailing flexible array of mNumberBuffers
  // AudioBuffer entries; its mData pointers alias input_storage_ (planar).
  std::vector<uint8_t> input_abl_storage_;

  // Thread safety
  mutable std::mutex mutex_;
};

} // namespace orpheus
