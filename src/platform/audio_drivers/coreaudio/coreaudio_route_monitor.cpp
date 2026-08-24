// SPDX-License-Identifier: MIT
#include "coreaudio_route_monitor.h"

#include <algorithm>

namespace orpheus {

CoreAudioRouteMonitor::CoreAudioRouteMonitor(ICoreAudioSampleRatePropertyApi& property_api,
                                             uint32_t expected_sample_rate,
                                             uint32_t expected_buffer_frames,
                                             std::vector<CoreAudioRouteDevice> devices,
                                             std::vector<CoreAudioRouteStream> streams)
    : property_api_(property_api),
      expected_sample_rate_(static_cast<Float64>(expected_sample_rate)),
      expected_buffer_frames_(expected_buffer_frames), devices_(std::move(devices)),
      streams_(std::move(streams)) {
  std::sort(devices_.begin(), devices_.end(),
            [](const CoreAudioRouteDevice& lhs, const CoreAudioRouteDevice& rhs) {
              return lhs.device_id < rhs.device_id;
            });
  std::vector<CoreAudioRouteDevice> unique_devices;
  unique_devices.reserve(devices_.size());
  for (const CoreAudioRouteDevice& device : devices_) {
    if (device.device_id == 0) {
      continue;
    }
    if (!unique_devices.empty() && unique_devices.back().device_id == device.device_id) {
      unique_devices.back().monitors_input =
          unique_devices.back().monitors_input || device.monitors_input;
      unique_devices.back().monitors_output =
          unique_devices.back().monitors_output || device.monitors_output;
      continue;
    }
    unique_devices.push_back(device);
  }
  devices_ = std::move(unique_devices);

  std::sort(streams_.begin(), streams_.end(),
            [](const CoreAudioRouteStream& lhs, const CoreAudioRouteStream& rhs) {
              return lhs.stream_id < rhs.stream_id;
            });
  streams_.erase(std::unique(streams_.begin(), streams_.end(),
                             [](const CoreAudioRouteStream& lhs, const CoreAudioRouteStream& rhs) {
                               return lhs.stream_id == rhs.stream_id;
                             }),
                 streams_.end());

  registrations_.reserve(devices_.size() * 3 + streams_.size() * 2);
  for (const CoreAudioRouteDevice& device : devices_) {
    registrations_.push_back({device.device_id, deviceAddress(kAudioDevicePropertyDeviceIsAlive)});
    registrations_.push_back(
        {device.device_id, deviceAddress(kAudioDevicePropertyNominalSampleRate)});
    registrations_.push_back(
        {device.device_id, deviceAddress(kAudioDevicePropertyBufferFrameSize)});
  }
  for (const CoreAudioRouteStream& stream : streams_) {
    registrations_.push_back({stream.stream_id, streamAddress(kAudioStreamPropertyVirtualFormat)});
    registrations_.push_back({stream.stream_id, streamAddress(kAudioStreamPropertyPhysicalFormat)});
  }
}

CoreAudioRouteMonitor::~CoreAudioRouteMonitor() {
  stop();
}

bool CoreAudioRouteMonitor::start() noexcept {
  if (registered_count_ != 0 || registrations_.empty()) {
    return registered_count_ != 0;
  }

  state_.store(0, std::memory_order_release);
  for (const ListenerRegistration& registration : registrations_) {
    if (property_api_.addPropertyListener(registration.object_id, &registration.address,
                                          &propertyChanged, this) != noErr) {
      stop();
      return false;
    }
    ++registered_count_;
  }
  return true;
}

void CoreAudioRouteMonitor::stop() noexcept {
  while (registered_count_ != 0) {
    --registered_count_;
    const ListenerRegistration& registration = registrations_[registered_count_];
    property_api_.removePropertyListener(registration.object_id, &registration.address,
                                         &propertyChanged, this);
  }
  state_.fetch_or(kStoppedBit | kPendingBit, std::memory_order_release);
  pending_changed_.notify_all();
}

void CoreAudioRouteMonitor::requestCheck() noexcept {
  uint64_t state = state_.load(std::memory_order_acquire);
  uint64_t requested_state = 0;
  do {
    requested_state = (state + kGenerationIncrement) | kPendingBit;
  } while (!state_.compare_exchange_weak(state, requested_state, std::memory_order_acq_rel,
                                         std::memory_order_acquire));
  pending_changed_.notify_one();
}

void CoreAudioRouteMonitor::waitForChange() noexcept {
  std::unique_lock<std::mutex> lock(pending_mutex_);
  pending_changed_.wait(
      lock, [this] { return (state_.load(std::memory_order_acquire) & kPendingBit) != 0; });
}

CoreAudioRoutePollResult CoreAudioRouteMonitor::poll() noexcept {
  uint64_t serviced_state = state_.load(std::memory_order_acquire);
  if ((serviced_state & kPendingBit) == 0) {
    return CoreAudioRoutePollResult::NoChange;
  }
  if ((serviced_state & kStoppedBit) != 0) {
    return CoreAudioRoutePollResult::NoChange;
  }

  const auto alive_address = deviceAddress(kAudioDevicePropertyDeviceIsAlive);
  const auto rate_address = deviceAddress(kAudioDevicePropertyNominalSampleRate);
  const auto buffer_address = deviceAddress(kAudioDevicePropertyBufferFrameSize);
  bool rate_restored = false;
  bool aggregate_unavailable = false;

  for (const CoreAudioRouteDevice& device : devices_) {
    UInt32 alive = 0;
    if (!readUInt32(property_api_, device.device_id, alive_address, alive)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (alive == 0) {
      if (device.monitors_input && !device.monitors_output) {
        return CoreAudioRoutePollResult::InputUnavailable;
      }
      if (device.monitors_output && !device.monitors_input) {
        return CoreAudioRoutePollResult::OutputUnavailable;
      }
      aggregate_unavailable = true;
      continue;
    }

    Float64 observed_rate = 0.0;
    if (!readFloat64(property_api_, device.device_id, rate_address, observed_rate)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (observed_rate != expected_sample_rate_) {
      if (property_api_.setPropertyData(device.device_id, &rate_address,
                                        sizeof(expected_sample_rate_),
                                        &expected_sample_rate_) != noErr ||
          !readFloat64(property_api_, device.device_id, rate_address, observed_rate)) {
        return CoreAudioRoutePollResult::ReinitializationRequired;
      }
      if (observed_rate != expected_sample_rate_) {
        return CoreAudioRoutePollResult::ReinitializationRequired;
      }
      rate_restored = true;
    }

    UInt32 observed_buffer_frames = 0;
    if (!readUInt32(property_api_, device.device_id, buffer_address, observed_buffer_frames)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (observed_buffer_frames != expected_buffer_frames_) {
      return CoreAudioRoutePollResult::ReinitializationRequired;
    }
  }
  if (aggregate_unavailable) {
    return CoreAudioRoutePollResult::RouteUnavailable;
  }

  for (const CoreAudioRouteStream& stream : streams_) {
    const auto virtual_address = streamAddress(kAudioStreamPropertyVirtualFormat);
    const auto physical_address = streamAddress(kAudioStreamPropertyPhysicalFormat);
    AudioStreamBasicDescription observed_virtual_format{};
    AudioStreamBasicDescription observed_physical_format{};
    if (!readFormat(property_api_, stream.stream_id, virtual_address, observed_virtual_format) ||
        !readFormat(property_api_, stream.stream_id, physical_address, observed_physical_format)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (!streamLayoutsEqual(observed_virtual_format, stream.expected_virtual_format) ||
        !streamLayoutsEqual(observed_physical_format, stream.expected_physical_format)) {
      return CoreAudioRoutePollResult::FormatChanged;
    }
  }

  if (!state_.compare_exchange_strong(serviced_state, serviced_state & ~kPendingBit,
                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
    return CoreAudioRoutePollResult::NoChange;
  }
  return rate_restored ? CoreAudioRoutePollResult::RateRestored
                       : CoreAudioRoutePollResult::NoChange;
}

bool CoreAudioRouteMonitor::permitsRendering() const noexcept {
  const uint64_t state = state_.load(std::memory_order_acquire);
  return (state & (kPendingBit | kStoppedBit)) == 0;
}

OSStatus CoreAudioRouteMonitor::propertyChanged(AudioObjectID, UInt32,
                                                const AudioObjectPropertyAddress*,
                                                void* context) noexcept {
  auto* monitor = static_cast<CoreAudioRouteMonitor*>(context);
  monitor->requestCheck();
  return noErr;
}

AudioObjectPropertyAddress
CoreAudioRouteMonitor::deviceAddress(AudioObjectPropertySelector selector) noexcept {
  return {selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
}

AudioObjectPropertyAddress
CoreAudioRouteMonitor::streamAddress(AudioObjectPropertySelector selector) noexcept {
  return {selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
}

bool CoreAudioRouteMonitor::streamLayoutsEqual(const AudioStreamBasicDescription& lhs,
                                               const AudioStreamBasicDescription& rhs) noexcept {
  // mSampleRate is intentionally excluded: device nominal rate is authoritative and verified
  // before stream layout.
  return lhs.mFormatID == rhs.mFormatID && lhs.mFormatFlags == rhs.mFormatFlags &&
         lhs.mBytesPerPacket == rhs.mBytesPerPacket &&
         lhs.mFramesPerPacket == rhs.mFramesPerPacket && lhs.mBytesPerFrame == rhs.mBytesPerFrame &&
         lhs.mChannelsPerFrame == rhs.mChannelsPerFrame &&
         lhs.mBitsPerChannel == rhs.mBitsPerChannel && lhs.mReserved == rhs.mReserved;
}

bool CoreAudioRouteMonitor::readUInt32(ICoreAudioSampleRatePropertyApi& property_api,
                                       AudioObjectID object_id,
                                       const AudioObjectPropertyAddress& address,
                                       UInt32& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

bool CoreAudioRouteMonitor::readFloat64(ICoreAudioSampleRatePropertyApi& property_api,
                                        AudioObjectID object_id,
                                        const AudioObjectPropertyAddress& address,
                                        Float64& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

bool CoreAudioRouteMonitor::readFormat(ICoreAudioSampleRatePropertyApi& property_api,
                                       AudioObjectID object_id,
                                       const AudioObjectPropertyAddress& address,
                                       AudioStreamBasicDescription& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

} // namespace orpheus
