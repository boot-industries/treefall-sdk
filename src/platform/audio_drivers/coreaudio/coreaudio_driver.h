// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_driver.h>

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <atomic>
#include <array>
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
  AudioIoTelemetry getTelemetry() const noexcept override;
  AudioDriverCapabilities getCapabilities() const override;
  AudioDriverRuntimeInfo getRuntimeInfo() const override;
  bool pollEvent(AudioDriverEvent& event) noexcept override;
  uint64_t droppedEventCount() const noexcept override;

  /// Set performance monitor for audio metrics tracking
  /// @param monitor Performance monitor instance (can be nullptr to disable)
  /// @note Thread-safe: Can be called before or after start()
  void setPerformanceMonitor(IPerformanceMonitor* monitor) override;

  /// Test-only helpers for deterministic backend failure accounting.
  void setInputRenderFailuresForTesting(uint64_t count) noexcept;
  void incrementInputRenderFailuresForTesting() noexcept;
  void publishEventForTesting(const AudioDriverEvent& event) noexcept;
  OSStatus renderForTesting(const AudioTimeStamp* timestamp, UInt32 frames,
                            AudioBufferList* output, IAudioCallback& callback) noexcept;

private:
  /// Audio Unit render callback (invoked on audio thread)
  static OSStatus renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                 UInt32 inNumberFrames, AudioBufferList* ioData);

  void recordInputRenderFailure() noexcept;
  struct EventSlot {
    std::atomic<uint64_t> sequence{0};
    AudioDriverEvent event{};
  };

  struct ListenerRegistration {
    CoreAudioDriver* driver{nullptr};
    AudioObjectID object{kAudioObjectUnknown};
    AudioObjectPropertyAddress address{};
    AudioDriverEventType event_type{AudioDriverEventType::BackendRestarted};
    std::atomic<bool> active{false};
  };

  static OSStatus propertyListener(AudioObjectID object, UInt32 address_count,
                                   const AudioObjectPropertyAddress* addresses,
                                   void* context);
  static void processListener(void* context);
  void enqueueEvent(const AudioDriverEvent& event) noexcept;
  void resetEventQueue() noexcept;
  bool queryMaximumCallbackFrames(AudioDeviceID device, uint32_t& frames) const noexcept;
  bool registerPropertyListeners();
  void removePropertyListeners();
  void addListener(AudioObjectID object, AudioObjectPropertySelector selector,
                   AudioObjectPropertyScope scope, AudioDriverEventType type, bool& success);

  /// Enumerate available audio devices
  /// @return Vector of device IDs
  std::vector<AudioDeviceID> enumerateDevices();

  /// Find a device by its persistent CoreAudio DeviceUID.
  /// @return Device ID or 0 if the UID is unknown
  AudioDeviceID findDeviceByUID(const std::string& device_uid);

  /// Get the system default device for a given selector (e.g.
  /// kAudioHardwarePropertyDefaultOutputDevice / ...DefaultInputDevice).
  /// @return Device ID or 0 on failure
  AudioDeviceID getDefaultDevice(AudioObjectPropertySelector selector);

  /// Resolve the configured physical input/output endpoints. Empty directional
  /// IDs select the corresponding system defaults; separate endpoints require
  /// a private aggregate. Resolution never falls back to a different endpoint.
  /// @return Device ID (possibly an aggregate) or 0 on failure
  AudioDeviceID resolveInputOutputDevice();

  /// Create a private, non-stacked Aggregate Device combining a capture
  /// sub-device and a playback sub-device so one AUHAL unit can drive both.
  /// Torn down in cleanupAudioUnit() via AudioHardwareDestroyAggregateDevice.
  /// @return Aggregate device ID, or 0 on failure
  AudioDeviceID createAggregateDevice(AudioDeviceID input_device_id,
                                      AudioDeviceID output_device_id);

  /// Check whether a device supports the requested direction.
  bool supportsDirection(AudioDeviceID device_id, AudioObjectPropertyScope scope) const;

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
  uint32_t maximum_callback_frames_{0};
  std::string selected_input_device_uid_;
  std::string selected_output_device_uid_;
  std::atomic<bool> force_next_discontinuity_{true};
  std::atomic<uint64_t> oversize_callbacks_{0};

  static constexpr uint64_t kEventCapacity = 64;
  std::array<EventSlot, kEventCapacity> event_slots_{};
  std::atomic<uint64_t> event_enqueue_position_{0};
  std::atomic<uint64_t> event_dequeue_position_{0};
  std::atomic<uint64_t> dropped_events_{0};
  dispatch_queue_t listener_queue_{nullptr};
  std::array<ListenerRegistration, 8> listeners_{};
  size_t listener_count_{0};

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
