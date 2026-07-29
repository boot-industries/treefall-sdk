// SPDX-License-Identifier: MIT
#include "coreaudio_sample_rate_monitor.h"

#include <algorithm>

namespace orpheus {

OSStatus CoreAudioSampleRatePropertyApi::addPropertyListener(
    AudioObjectID device_id, const AudioObjectPropertyAddress* address,
    AudioObjectPropertyListenerProc listener, void* context) noexcept {
  return AudioObjectAddPropertyListener(device_id, address, listener, context);
}

OSStatus CoreAudioSampleRatePropertyApi::removePropertyListener(
    AudioObjectID device_id, const AudioObjectPropertyAddress* address,
    AudioObjectPropertyListenerProc listener, void* context) noexcept {
  return AudioObjectRemovePropertyListener(device_id, address, listener, context);
}

OSStatus CoreAudioSampleRatePropertyApi::getPropertyData(AudioObjectID device_id,
                                                         const AudioObjectPropertyAddress* address,
                                                         UInt32* size, void* data) noexcept {
  return AudioObjectGetPropertyData(device_id, address, 0, nullptr, size, data);
}

OSStatus CoreAudioSampleRatePropertyApi::setPropertyData(AudioObjectID device_id,
                                                         const AudioObjectPropertyAddress* address,
                                                         UInt32 size, const void* data) noexcept {
  return AudioObjectSetPropertyData(device_id, address, 0, nullptr, size, data);
}

CoreAudioSampleRateMonitor::CoreAudioSampleRateMonitor(
    ICoreAudioSampleRatePropertyApi& property_api, uint32_t expected_sample_rate,
    std::vector<AudioDeviceID> device_ids)
    : property_api_(property_api),
      expected_sample_rate_(static_cast<Float64>(expected_sample_rate)),
      device_ids_(std::move(device_ids)) {
  std::sort(device_ids_.begin(), device_ids_.end());
  device_ids_.erase(std::unique(device_ids_.begin(), device_ids_.end()), device_ids_.end());
  device_ids_.erase(std::remove(device_ids_.begin(), device_ids_.end(), AudioDeviceID{0}),
                    device_ids_.end());
}

CoreAudioSampleRateMonitor::~CoreAudioSampleRateMonitor() {
  stop();
}

bool CoreAudioSampleRateMonitor::start() noexcept {
  if (registered_count_ != 0 || device_ids_.empty()) {
    return registered_count_ != 0;
  }

  const AudioObjectPropertyAddress address = nominalSampleRateAddress();
  for (const AudioDeviceID device_id : device_ids_) {
    if (property_api_.addPropertyListener(device_id, &address, &propertyChanged, this) != noErr) {
      stop();
      return false;
    }
    ++registered_count_;
  }
  return true;
}

void CoreAudioSampleRateMonitor::stop() noexcept {
  const AudioObjectPropertyAddress address = nominalSampleRateAddress();
  while (registered_count_ != 0) {
    --registered_count_;
    property_api_.removePropertyListener(device_ids_[registered_count_], &address, &propertyChanged,
                                         this);
  }
  pending_.store(false, std::memory_order_release);
  permits_rendering_.store(false, std::memory_order_release);
  pending_changed_.notify_all();
}

void CoreAudioSampleRateMonitor::requestCheck() noexcept {
  permits_rendering_.store(false, std::memory_order_release);
  pending_.store(true, std::memory_order_release);
  pending_changed_.notify_one();
}

void CoreAudioSampleRateMonitor::waitForChange() noexcept {
  std::unique_lock<std::mutex> lock(pending_mutex_);
  pending_changed_.wait(lock, [this] { return pending_.load(std::memory_order_acquire); });
}

CoreAudioSampleRatePollResult CoreAudioSampleRateMonitor::poll() noexcept {
  if (!pending_.load(std::memory_order_acquire)) {
    return CoreAudioSampleRatePollResult::NoChange;
  }

  const AudioObjectPropertyAddress address = nominalSampleRateAddress();
  bool restored = false;
  for (const AudioDeviceID device_id : device_ids_) {
    Float64 observed_rate = 0.0;
    UInt32 size = sizeof(observed_rate);
    if (property_api_.getPropertyData(device_id, &address, &size, &observed_rate) != noErr ||
        size != sizeof(observed_rate)) {
      return CoreAudioSampleRatePollResult::QueryFailed;
    }

    if (observed_rate == expected_sample_rate_) {
      continue;
    }

    if (property_api_.setPropertyData(device_id, &address, sizeof(expected_sample_rate_),
                                      &expected_sample_rate_) != noErr) {
      return CoreAudioSampleRatePollResult::ReinitializationRequired;
    }

    observed_rate = 0.0;
    size = sizeof(observed_rate);
    if (property_api_.getPropertyData(device_id, &address, &size, &observed_rate) != noErr ||
        size != sizeof(observed_rate)) {
      return CoreAudioSampleRatePollResult::QueryFailed;
    }
    if (observed_rate != expected_sample_rate_) {
      return CoreAudioSampleRatePollResult::ReinitializationRequired;
    }
    restored = true;
  }

  pending_.store(false, std::memory_order_release);
  permits_rendering_.store(true, std::memory_order_release);
  return restored ? CoreAudioSampleRatePollResult::RateRestored
                  : CoreAudioSampleRatePollResult::NoChange;
}

bool CoreAudioSampleRateMonitor::permitsRendering() const noexcept {
  return permits_rendering_.load(std::memory_order_acquire);
}

bool CoreAudioSampleRateMonitor::isPending() const noexcept {
  return pending_.load(std::memory_order_acquire);
}

OSStatus CoreAudioSampleRateMonitor::propertyChanged(AudioObjectID, UInt32,
                                                     const AudioObjectPropertyAddress*,
                                                     void* context) noexcept {
  auto* monitor = static_cast<CoreAudioSampleRateMonitor*>(context);
  monitor->requestCheck();
  return noErr;
}

AudioObjectPropertyAddress CoreAudioSampleRateMonitor::nominalSampleRateAddress() noexcept {
  return {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
          kAudioObjectPropertyElementMain};
}

} // namespace orpheus
