// SPDX-License-Identifier: MIT
#include "coreaudio_route_monitor.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace orpheus {

CoreAudioRouteMonitor::CoreAudioRouteMonitor(ICoreAudioPropertyApi& property_api,
                                             uint32_t expected_sample_rate,
                                             uint32_t expected_buffer_frames,
                                             std::vector<CoreAudioRouteDevice> devices,
                                             std::vector<CoreAudioRouteStream> streams)
    : property_api_(property_api),
      expected_sample_rate_(static_cast<Float64>(expected_sample_rate)),
      expected_buffer_frames_(expected_buffer_frames), devices_(std::move(devices)),
      streams_(std::move(streams)) {
  std::vector<CoreAudioRouteDevice> unique_devices;
  unique_devices.reserve(devices_.size());
  for (const CoreAudioRouteDevice& device : devices_) {
    if (device.device_id == 0) {
      continue;
    }
    const auto existing = std::find_if(unique_devices.begin(), unique_devices.end(),
                                       [&](const CoreAudioRouteDevice& candidate) {
                                         return candidate.device_id == device.device_id;
                                       });
    if (existing == unique_devices.end()) {
      unique_devices.push_back(device);
    } else {
      existing->monitors_input = existing->monitors_input || device.monitors_input;
      existing->monitors_output = existing->monitors_output || device.monitors_output;
      existing->is_private_aggregate =
          existing->is_private_aggregate || device.is_private_aggregate;
    }
  }
  devices_ = std::move(unique_devices);

  std::vector<CoreAudioRouteStream> unique_streams;
  unique_streams.reserve(streams_.size());
  for (const CoreAudioRouteStream& stream : streams_) {
    if (stream.stream_id == 0) {
      continue;
    }
    const auto existing = std::find_if(unique_streams.begin(), unique_streams.end(),
                                       [&](const CoreAudioRouteStream& candidate) {
                                         return candidate.stream_id == stream.stream_id;
                                       });
    if (existing == unique_streams.end()) {
      unique_streams.push_back(stream);
    }
  }
  streams_ = std::move(unique_streams);

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
  if (registered_count_ != 0) {
    return state_.load(std::memory_order_acquire) == State::Active;
  }
  if (registrations_.empty()) {
    return false;
  }

  state_.store(State::Active, std::memory_order_release);
  pending_.store(false, std::memory_order_release);
  generation_.fetch_add(1, std::memory_order_acq_rel);
  generation_.notify_one();

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
  state_.store(State::Stopped, std::memory_order_release);
  pending_.store(true, std::memory_order_release);
  signalStateChange();
  while (registered_count_ != 0) {
    --registered_count_;
    const ListenerRegistration& registration = registrations_[registered_count_];
    property_api_.removePropertyListener(registration.object_id, &registration.address,
                                         &propertyChanged, this);
  }
}

void CoreAudioRouteMonitor::requestCheck() noexcept {
  if (state_.load(std::memory_order_acquire) == State::Active) {
    pending_.store(true, std::memory_order_release);
  }
  signalStateChange();
}

void CoreAudioRouteMonitor::closeAdmission() noexcept {
  State expected = State::Active;
  state_.compare_exchange_strong(expected, State::Terminal, std::memory_order_acq_rel,
                                 std::memory_order_acquire);
  pending_.store(true, std::memory_order_release);
  signalStateChange();
}

void CoreAudioRouteMonitor::waitForChange() noexcept {
  uint64_t observed = generation_.load(std::memory_order_acquire);
  while (state_.load(std::memory_order_acquire) == State::Active &&
         !pending_.load(std::memory_order_acquire)) {
    generation_.wait(observed, std::memory_order_acquire);
    observed = generation_.load(std::memory_order_acquire);
  }
}

CoreAudioRoutePollResult CoreAudioRouteMonitor::poll() noexcept {
  if (state_.load(std::memory_order_acquire) != State::Active ||
      !pending_.exchange(false, std::memory_order_acq_rel)) {
    return CoreAudioRoutePollResult::NoChange;
  }

  const auto alive_address = deviceAddress(kAudioDevicePropertyDeviceIsAlive);
  const auto rate_address = deviceAddress(kAudioDevicePropertyNominalSampleRate);
  const auto buffer_address = deviceAddress(kAudioDevicePropertyBufferFrameSize);

  const auto check_device =
      [&](const CoreAudioRouteDevice& device) noexcept -> CoreAudioRoutePollResult {
    UInt32 alive = 0;
    if (!readUInt32(property_api_, device.device_id, alive_address, alive)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (alive == 0) {
      if (device.monitors_output) {
        return CoreAudioRoutePollResult::OutputUnavailable;
      }
      if (device.monitors_input) {
        return CoreAudioRoutePollResult::InputUnavailable;
      }
      return CoreAudioRoutePollResult::BackendFailure;
    }

    const Float64 expected_rate = device.expected_sample_rate != 0
                                      ? static_cast<Float64>(device.expected_sample_rate)
                                      : expected_sample_rate_;
    Float64 observed_rate = 0.0;
    if (!readFloat64(property_api_, device.device_id, rate_address, observed_rate)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (observed_rate != expected_rate) {
      return CoreAudioRoutePollResult::SampleRateChanged;
    }

    UInt32 observed_buffer_frames = 0;
    if (!readUInt32(property_api_, device.device_id, buffer_address, observed_buffer_frames)) {
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (observed_buffer_frames != expected_buffer_frames_) {
      return CoreAudioRoutePollResult::BufferSizeChanged;
    }
    return CoreAudioRoutePollResult::NoChange;
  };

  // Output is the required direction. This ordering also makes same-device
  // duplex loss deterministic: the output result wins over input loss.
  for (const CoreAudioRouteDevice& device : devices_) {
    if (device.is_private_aggregate) {
      continue;
    }
    const CoreAudioRoutePollResult result = check_device(device);
    if (result != CoreAudioRoutePollResult::NoChange) {
      closeAdmission();
      return result;
    }
  }
  for (const CoreAudioRouteDevice& device : devices_) {
    if (!device.is_private_aggregate) {
      continue;
    }
    const CoreAudioRoutePollResult result = check_device(device);
    if (result != CoreAudioRoutePollResult::NoChange) {
      closeAdmission();
      return result;
    }
  }

  const auto virtual_address = streamAddress(kAudioStreamPropertyVirtualFormat);
  const auto physical_address = streamAddress(kAudioStreamPropertyPhysicalFormat);
  for (const CoreAudioRouteStream& stream : streams_) {
    AudioStreamBasicDescription observed_virtual_format{};
    AudioStreamBasicDescription observed_physical_format{};
    if (!readFormat(property_api_, stream.stream_id, virtual_address, observed_virtual_format) ||
        !readFormat(property_api_, stream.stream_id, physical_address, observed_physical_format)) {
      closeAdmission();
      return CoreAudioRoutePollResult::BackendFailure;
    }
    if (!streamLayoutsEqual(observed_virtual_format, stream.expected_virtual_format) ||
        !streamLayoutsEqual(observed_physical_format, stream.expected_physical_format)) {
      closeAdmission();
      return CoreAudioRoutePollResult::FormatChanged;
    }
  }

  return CoreAudioRoutePollResult::NoChange;
}

bool CoreAudioRouteMonitor::permitsRendering() const noexcept {
  return state_.load(std::memory_order_acquire) == State::Active;
}

bool CoreAudioRouteMonitor::isTerminal() const noexcept {
  return state_.load(std::memory_order_acquire) == State::Terminal;
}

OSStatus CoreAudioRouteMonitor::propertyChanged(AudioObjectID, UInt32,
                                                const AudioObjectPropertyAddress*,
                                                void* context) noexcept {
  static_cast<CoreAudioRouteMonitor*>(context)->requestCheck();
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

// poll() validates each device's nominal rate before checking streams, so only
// that verified device-rate check is authoritative for rate changes. A stream
// channel or other layout mutation remains terminal FormatChanged.
bool CoreAudioRouteMonitor::streamLayoutsEqual(const AudioStreamBasicDescription& lhs,
                                               const AudioStreamBasicDescription& rhs) noexcept {
  return lhs.mFormatID == rhs.mFormatID && lhs.mFormatFlags == rhs.mFormatFlags &&
         lhs.mBytesPerPacket == rhs.mBytesPerPacket &&
         lhs.mFramesPerPacket == rhs.mFramesPerPacket && lhs.mBytesPerFrame == rhs.mBytesPerFrame &&
         lhs.mChannelsPerFrame == rhs.mChannelsPerFrame &&
         lhs.mBitsPerChannel == rhs.mBitsPerChannel && lhs.mReserved == rhs.mReserved;
}

bool CoreAudioRouteMonitor::readUInt32(ICoreAudioPropertyApi& property_api, AudioObjectID object_id,
                                       const AudioObjectPropertyAddress& address,
                                       UInt32& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

bool CoreAudioRouteMonitor::readFloat64(ICoreAudioPropertyApi& property_api,
                                        AudioObjectID object_id,
                                        const AudioObjectPropertyAddress& address,
                                        Float64& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

bool CoreAudioRouteMonitor::readFormat(ICoreAudioPropertyApi& property_api, AudioObjectID object_id,
                                       const AudioObjectPropertyAddress& address,
                                       AudioStreamBasicDescription& value) noexcept {
  UInt32 size = sizeof(value);
  return property_api.getPropertyData(object_id, &address, &size, &value) == noErr &&
         size == sizeof(value);
}

void CoreAudioRouteMonitor::signalStateChange() noexcept {
  generation_.fetch_add(1, std::memory_order_acq_rel);
  generation_.notify_one();
}

} // namespace orpheus
