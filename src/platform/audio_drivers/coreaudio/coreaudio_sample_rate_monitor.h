// SPDX-License-Identifier: MIT
#pragma once

#include <CoreAudio/CoreAudio.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

namespace orpheus {

/// Result of servicing a nominal sample-rate notification off the render path.
enum class CoreAudioSampleRatePollResult : uint8_t {
  NoChange,
  RateRestored,
  ReinitializationRequired,
  QueryFailed,
};

/// Injectable CoreAudio property access used to make listener lifecycle and
/// recovery behavior deterministic without changing a real device's clock.
class ICoreAudioSampleRatePropertyApi {
public:
  virtual ~ICoreAudioSampleRatePropertyApi() = default;

  virtual OSStatus addPropertyListener(AudioObjectID device_id,
                                       const AudioObjectPropertyAddress* address,
                                       AudioObjectPropertyListenerProc listener,
                                       void* context) noexcept = 0;
  virtual OSStatus removePropertyListener(AudioObjectID device_id,
                                          const AudioObjectPropertyAddress* address,
                                          AudioObjectPropertyListenerProc listener,
                                          void* context) noexcept = 0;
  virtual OSStatus getPropertyData(AudioObjectID device_id,
                                   const AudioObjectPropertyAddress* address, UInt32* size,
                                   void* data) noexcept = 0;
  virtual OSStatus setPropertyData(AudioObjectID device_id,
                                   const AudioObjectPropertyAddress* address, UInt32 size,
                                   const void* data) noexcept = 0;
};

/// Production adapter for CoreAudio's process-global property functions.
class CoreAudioSampleRatePropertyApi final : public ICoreAudioSampleRatePropertyApi {
public:
  OSStatus addPropertyListener(AudioObjectID device_id, const AudioObjectPropertyAddress* address,
                               AudioObjectPropertyListenerProc listener,
                               void* context) noexcept override;
  OSStatus removePropertyListener(AudioObjectID device_id,
                                  const AudioObjectPropertyAddress* address,
                                  AudioObjectPropertyListenerProc listener,
                                  void* context) noexcept override;
  OSStatus getPropertyData(AudioObjectID device_id, const AudioObjectPropertyAddress* address,
                           UInt32* size, void* data) noexcept override;
  OSStatus setPropertyData(AudioObjectID device_id, const AudioObjectPropertyAddress* address,
                           UInt32 size, const void* data) noexcept override;
};

/// Watches every physical route which can invalidate the configured AU format.
/// The listener performs only atomic notification; callers service pending work
/// from a control thread via poll().
class CoreAudioSampleRateMonitor final {
public:
  CoreAudioSampleRateMonitor(ICoreAudioSampleRatePropertyApi& property_api,
                             uint32_t expected_sample_rate, std::vector<AudioDeviceID> device_ids);
  ~CoreAudioSampleRateMonitor();

  CoreAudioSampleRateMonitor(const CoreAudioSampleRateMonitor&) = delete;
  CoreAudioSampleRateMonitor& operator=(const CoreAudioSampleRateMonitor&) = delete;

  /// Registers one listener per unique route device. Rolls back on failure.
  bool start() noexcept;
  /// Symmetric listener cleanup. Safe to repeat.
  void stop() noexcept;

  /// Forces a control-thread verification, including before output is started.
  void requestCheck() noexcept;
  /// Blocks until a listener notification or requestCheck().
  void waitForChange() noexcept;
  /// Reasserts the configured rate and verifies every monitored device.
  CoreAudioSampleRatePollResult poll() noexcept;

  /// The render callback must return silence while rate validation is pending.
  bool permitsRendering() const noexcept;
  bool isPending() const noexcept;

private:
  static OSStatus propertyChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*,
                                  void* context) noexcept;
  static AudioObjectPropertyAddress nominalSampleRateAddress() noexcept;

  static constexpr uint64_t kPendingBit = 1;
  static constexpr uint64_t kStoppedBit = 2;
  static constexpr uint64_t kGenerationIncrement = 4;

  ICoreAudioSampleRatePropertyApi& property_api_;
  const Float64 expected_sample_rate_;
  std::vector<AudioDeviceID> device_ids_;
  // A single state word makes closing and reopening the render gate conditional
  // on the exact listener-generation that poll() serviced.
  std::atomic<uint64_t> state_{kStoppedBit};
  std::condition_variable pending_changed_;
  std::mutex pending_mutex_;
  size_t registered_count_{0};
};

} // namespace orpheus
