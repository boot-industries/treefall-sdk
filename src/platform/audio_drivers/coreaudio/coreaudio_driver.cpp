// SPDX-License-Identifier: MIT
#include "coreaudio_driver.h"
#include "../../../core/common/realtime_counter.h"
#include <orpheus/performance_monitor.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

namespace orpheus {

CoreAudioDriver::CoreAudioDriver() = default;

CoreAudioDriver::~CoreAudioDriver() {
  stop();
  cleanupAudioUnit();
}

SessionGraphError CoreAudioDriver::initialize(const AudioDriverConfig& config) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_running_.load(std::memory_order_acquire)) {
      return SessionGraphError::NotReady;
    }
  }

  // A previous terminal runtime outcome leaves a joined-but-still-owned
  // monitor. Tear it down before replacing the route.
  stopSampleRateMonitor();

  std::lock_guard<std::mutex> lock(mutex_);
  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }

  // Validate configuration
  if (config.sample_rate == 0 || config.buffer_size == 0 || config.num_outputs == 0) {
    return SessionGraphError::InvalidParameter;
  }

  // Clean up any existing AudioUnit
  cleanupAudioUnit();

  config_ = config;
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  input_storage_.clear();
  input_buffers_.clear();
  input_abl_storage_.clear();

  // Resolve both requested directions before creating the AudioUnit. A
  // non-empty directional ID is a persistent HAL DeviceUID; resolution errors
  // are terminal and must never silently select a default endpoint.
  device_id_ = resolveInputOutputDevice();
  if (device_id_ == 0) {
    return SessionGraphError::InvalidParameter;
  }

  // Set up AudioUnit
  SessionGraphError result = setupAudioUnit(device_id_);
  if (result != SessionGraphError::OK) {
    cleanupAudioUnit();
    return result;
  }
  createSampleRateMonitor();

  // Pre-allocate audio buffers (no allocations in audio callback)
  uint32_t num_outputs = config_.num_outputs;
  uint32_t buffer_size = config_.buffer_size;

  output_storage_.resize(num_outputs * buffer_size);
  output_buffers_.resize(num_outputs);
  for (uint32_t ch = 0; ch < num_outputs; ++ch) {
    output_buffers_[ch] = &output_storage_[ch * buffer_size];
  }

  // Input buffers (if needed)
  if (config_.num_inputs > 0) {
    input_storage_.resize(config_.num_inputs * buffer_size);
    input_buffers_.resize(config_.num_inputs);
    for (uint32_t ch = 0; ch < config_.num_inputs; ++ch) {
      input_buffers_[ch] = &input_storage_[ch * buffer_size];
    }

    // Pre-allocate the AudioBufferList used to pull captured input in the
    // render callback. One AudioBuffer per input channel (planar), so its
    // trailing flexible array holds num_inputs entries. Allocated here, never
    // on the audio thread.
    const size_t abl_bytes =
        sizeof(AudioBufferList) + (config_.num_inputs - 1) * sizeof(AudioBuffer);
    input_abl_storage_.assign(abl_bytes, 0);
  }

  input_render_failures_.store(0, std::memory_order_release);
  runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
  return SessionGraphError::OK;
}

SessionGraphError CoreAudioDriver::start(IAudioCallback* callback) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!audio_unit_) {
    return SessionGraphError::NotReady;
  }

  if (is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::NotReady;
  }

  if (!callback) {
    return SessionGraphError::InvalidParameter;
  }

  // A terminal monitor outcome stops the worker without joining it. Reap that
  // completed worker before replacing std::thread on a later start().
  if (sample_rate_monitor_thread_.joinable()) {
    sample_rate_monitor_thread_.join();
  }

  // Listener registration and the initial rate verification happen before the
  // AU is started. A device that already rejects the requested rate therefore
  // never reaches processAudio() with a mismatched stream format.
  if (!startSampleRateMonitorLocked()) {
    return SessionGraphError::InternalError;
  }

  callback_target_.replaceAndDrain(callback);
  // Every successful start opens a new callback-admission epoch.
  expected_stream_sample_ = 0;
  stream_timeline_initialized_ = false;
  is_running_.store(true, std::memory_order_release);

  OSStatus status = AudioOutputUnitStart(audio_unit_);
  if (status != noErr) {
    is_running_.store(false, std::memory_order_release);
    callback_target_.replaceAndDrain(nullptr);
    sample_rate_monitor_->stop();
    return SessionGraphError::InternalError;
  }

  sample_rate_monitor_active_.store(true, std::memory_order_release);
  try {
    sample_rate_monitor_thread_ = std::thread(&CoreAudioDriver::sampleRateMonitorLoop, this);
  } catch (...) {
    sample_rate_monitor_active_.store(false, std::memory_order_release);
    AudioOutputUnitStop(audio_unit_);
    callback_target_.replaceAndDrain(nullptr);
    is_running_.store(false, std::memory_order_release);
    sample_rate_monitor_->stop();
    return SessionGraphError::InternalError;
  }

  return SessionGraphError::OK;
}

SessionGraphError CoreAudioDriver::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopRenderingLocked();
  }
  stopSampleRateMonitor();
  return SessionGraphError::OK;
}

bool CoreAudioDriver::isRunning() const {
  return is_running_.load(std::memory_order_acquire);
}

const AudioDriverConfig& CoreAudioDriver::getConfig() const {
  return config_;
}

std::string CoreAudioDriver::getDriverName() const {
  return "CoreAudio";
}

uint32_t CoreAudioDriver::getLatencySamples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  // Query the physical routes, not the private aggregate wrapper. Consumer
  // outputs (Bluetooth/AirPods in particular) report their transport delay on
  // the output sub-device, and that report can change while a route is active.
  return queryDeviceLatency();
}

AudioIoTelemetry CoreAudioDriver::getTelemetry() const noexcept {
  return {input_render_failures_.load(std::memory_order_acquire),
          runtime_outcome_.load(std::memory_order_acquire)};
}

void CoreAudioDriver::setInputRenderFailuresForTesting(uint64_t count) noexcept {
  input_render_failures_.store(count, std::memory_order_release);
}

void CoreAudioDriver::incrementInputRenderFailuresForTesting() noexcept {
  recordInputRenderFailure();
}


void CoreAudioDriver::recordInputRenderFailure() noexcept {
  detail::publishSaturatingIncrement(input_render_failures_);
}

AudioDriverCapabilities CoreAudioDriver::getCapabilities() const {
  AudioDriverCapabilities caps;
  caps.backend = AudioBackend::CoreAudio;
  caps.platform = AudioPlatform::macOS;
  caps.min_output_channels = config_.num_outputs == 0 ? 0 : 1;
  caps.max_output_channels = config_.num_outputs;
  caps.min_input_channels = 0;
  caps.max_input_channels = config_.num_inputs;
  caps.native_sample_rates.push_back(config_.sample_rate);
  caps.native_buffer_sizes.push_back(config_.buffer_size);
  const std::string input_endpoint =
      config_.input_device_id.empty() ? "coreaudio:default-input" : config_.input_device_id;
  const std::string output_endpoint =
      config_.output_device_id.empty() ? "coreaudio:default-output" : config_.output_device_id;
  for (uint16_t channel = 0; channel < config_.num_inputs; ++channel) {
    caps.input_channel_ids.push_back(input_endpoint + ":input:" + std::to_string(channel));
  }
  for (uint16_t channel = 0; channel < config_.num_outputs; ++channel) {
    caps.output_channel_ids.push_back(output_endpoint + ":output:" + std::to_string(channel));
  }
  caps.supports_exclusive_mode = false;
  caps.supports_shared_mode = true;
  caps.supports_device_hot_swap = true;
  caps.supports_input = config_.num_inputs > 0;
  caps.supports_multichannel_output = config_.num_outputs > 2;
  caps.reports_hardware_latency = getLatencySamples() > 0;
  return caps;
}

void CoreAudioDriver::setPerformanceMonitor(IPerformanceMonitor* monitor) {
  performance_monitor_target_.replaceAndDrain(monitor);
}

// Static audio callback
OSStatus CoreAudioDriver::renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                         const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                         UInt32 inNumberFrames, AudioBufferList* ioData) {
  (void)inBusNumber;

  auto* driver = static_cast<CoreAudioDriver*>(inRefCon);
  assert(driver != nullptr);

  // Zero output buffers before trying admission so a callback that races stop()
  // always returns silence without reading any driver-owned callback state.
  for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
    std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
  }

  auto callback_lease = driver->callback_target_.tryAcquire();
  if (!callback_lease) {
    return noErr;
  }

  // Clamp frames to our allocated buffer size
  uint32_t frames_to_process = std::min(static_cast<uint32_t>(inNumberFrames),
                                        static_cast<uint32_t>(driver->config_.buffer_size));
  uint32_t num_channels = driver->config_.num_outputs;

  // Zero our output buffers before callback
  for (uint32_t ch = 0; ch < num_channels; ++ch) {
    std::memset(driver->output_buffers_[ch], 0, frames_to_process * sizeof(float));
  }

  // Zero our input buffers before capture so a failed/absent AudioUnitRender
  // hands the host silence rather than stale samples.
  const uint32_t num_input_channels = driver->config_.num_inputs;
  for (uint32_t ch = 0; ch < num_input_channels; ++ch) {
    std::memset(driver->input_buffers_[ch], 0, frames_to_process * sizeof(float));
  }

  // CRITICAL: Check if we're still running before invoking callback
  // This prevents use-after-free if callback is invoked during/after stop()
  if (!driver->is_running_.load(std::memory_order_acquire)) {
    return noErr; // Driver is stopping, output silence
  }

  // A nominal-rate notification closes this gate before the control worker
  // touches CoreAudio. Never hand a host processAudio() a block whose AU
  // format may no longer match the hardware clock.
  if (!driver->sample_rate_monitor_ || !driver->sample_rate_monitor_->permitsRendering()) {
    return noErr;
  }

  IAudioCallback* callback = callback_lease.get();

  // Pull captured input (bus 1) into our planar input_buffers_ before invoking
  // the host callback. A HAL output render callback fires for the *output* bus
  // and must explicitly render the input bus; captured samples never arrive in
  // ioData. RT-safe: the AudioBufferList and its backing storage are
  // pre-allocated in initialize(); no allocation, lock, or I/O on this path.
  if (num_input_channels > 0 && !driver->input_abl_storage_.empty()) {
    auto* inputABL = reinterpret_cast<AudioBufferList*>(driver->input_abl_storage_.data());
    inputABL->mNumberBuffers = num_input_channels;
    for (uint32_t ch = 0; ch < num_input_channels; ++ch) {
      inputABL->mBuffers[ch].mNumberChannels = 1; // planar: one channel per buffer
      inputABL->mBuffers[ch].mDataByteSize = frames_to_process * sizeof(float);
      inputABL->mBuffers[ch].mData = driver->input_buffers_[ch];
    }

    // inputBus = 1. On failure the pre-zeroed buffers stand in as silence, so
    // we never propagate a hard error up the audio graph for a transient
    // miss -- but a persistent failure (e.g. capturing against a device with
    // no input channels) is exactly what left this class of bug invisible;
    // count it (lock-free, RT-safe) so a host or test can tell "always
    // silent" apart from "never actually captured."
    OSStatus render_status = AudioUnitRender(driver->audio_unit_, ioActionFlags, inTimeStamp,
                                             /*inputBus=*/1, frames_to_process, inputABL);
    if (render_status != noErr) {
      driver->recordInputRenderFailure();
    }
  }

  // Invoke user callback (lock-free). Timing is opt-in for diagnostics builds;
  // production callbacks should not pay for callback-duration instrumentation.
  const float** input_ptrs = driver->input_buffers_.empty()
                                 ? nullptr
                                 : const_cast<const float**>(driver->input_buffers_.data());
  float** output_ptrs = driver->output_buffers_.data();

  const bool sample_time_valid =
      inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampSampleTimeValid) != 0;
  const int64_t reported_sample_time =
      sample_time_valid ? static_cast<int64_t>(std::llround(inTimeStamp->mSampleTime)) : 0;
  const bool device_position_available = sample_time_valid && reported_sample_time >= 0;
  const int64_t stream_time =
      device_position_available ? reported_sample_time : driver->expected_stream_sample_;
  const bool discontinuity = !driver->stream_timeline_initialized_ || !device_position_available ||
                             stream_time != driver->expected_stream_sample_;
  const bool host_time_valid =
      inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid) != 0;

  AudioProcessBlock block;
  block.input_buffers = input_ptrs;
  block.output_buffers = output_ptrs;
  block.num_input_channels = static_cast<uint16_t>(num_input_channels);
  block.num_output_channels = static_cast<uint16_t>(num_channels);
  block.num_frames = frames_to_process;
  block.device_sample_position =
      device_position_available ? static_cast<uint64_t>(reported_sample_time) : 0;
  block.host_time_nanoseconds =
      host_time_valid ? AudioConvertHostTimeToNanos(inTimeStamp->mHostTime) : 0;
  block.discontinuity = discontinuity;

#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
  auto monitor_lease = driver->performance_monitor_target_.tryAcquire();
  const UInt64 callback_start =
      monitor_lease ? AudioGetCurrentHostTime() : UInt64{0};
#endif
  callback->processAudio(block);
  driver->expected_stream_sample_ = stream_time + frames_to_process;
  driver->stream_timeline_initialized_ = true;
#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
  if (auto* monitor = monitor_lease.get()) {
    const UInt64 callback_end = AudioGetCurrentHostTime();
    const UInt64 duration_ns = AudioConvertHostTimeToNanos(callback_end - callback_start);
    const uint64_t callback_duration_us = static_cast<uint64_t>(duration_ns / 1000u);
    const uint64_t buffer_duration_us =
        (static_cast<uint64_t>(frames_to_process) * 1'000'000ull) / driver->config_.sample_rate;
    const uint32_t active_clips = callback->activeClipCount();
    monitor->recordAudioCallback(callback_duration_us, buffer_duration_us, active_clips,
                                 driver->config_.sample_rate, frames_to_process);
  }
#endif

  // Copy planar output buffers to CoreAudio non-interleaved buffers
  for (uint32_t ch = 0; ch < num_channels && ch < ioData->mNumberBuffers; ++ch) {
    float* src = driver->output_buffers_[ch];
    auto* dst = static_cast<float*>(ioData->mBuffers[ch].mData);
    std::memcpy(dst, src, frames_to_process * sizeof(float));
  }

  return noErr;
}

std::vector<AudioDeviceID> CoreAudioDriver::enumerateDevices() {
  std::vector<AudioDeviceID> devices;

  AudioObjectPropertyAddress propertyAddress = {kAudioHardwarePropertyDevices,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};

  UInt32 dataSize = 0;
  OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0,
                                                   nullptr, &dataSize);

  if (status != noErr || dataSize == 0) {
    return devices;
  }

  UInt32 deviceCount = dataSize / sizeof(AudioDeviceID);
  devices.resize(deviceCount);

  status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr,
                                      &dataSize, devices.data());

  if (status != noErr) {
    devices.clear();
  }

  return devices;
}

AudioDeviceID CoreAudioDriver::getDefaultDevice(AudioObjectPropertySelector selector) {
  AudioObjectPropertyAddress propertyAddress = {selector, kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};

  AudioDeviceID deviceID = 0;
  UInt32 dataSize = sizeof(AudioDeviceID);

  OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0,
                                               nullptr, &dataSize, &deviceID);

  return (status == noErr && deviceID != kAudioObjectUnknown) ? deviceID : 0;
}

namespace {

/// Copy a device's persistent UID string (caller releases). Used to describe
/// sub-devices to AudioHardwareCreateAggregateDevice, which identifies them
/// by UID rather than the (session-scoped) AudioDeviceID.
CFStringRef copyDeviceUID(AudioDeviceID device_id) {
  AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyDeviceUID,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
  CFStringRef uid = nullptr;
  UInt32 dataSize = sizeof(CFStringRef);
  OSStatus status =
      AudioObjectGetPropertyData(device_id, &propertyAddress, 0, nullptr, &dataSize, &uid);
  return (status == noErr) ? uid : nullptr;
}

/// Query a device's clock domain. Devices sharing a non-zero domain are
/// synchronized to the same underlying hardware clock (e.g. a MacBook's
/// built-in microphone and built-in speakers both run off the machine's
/// single audio clock) -- 0 or a mismatch means the OS cannot prove they
/// share a clock and drift compensation is genuinely needed to bridge them.
UInt32 getClockDomain(AudioDeviceID device_id) {
  AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyClockDomain,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};
  UInt32 domain = 0;
  UInt32 dataSize = sizeof(UInt32);
  AudioObjectGetPropertyData(device_id, &propertyAddress, 0, nullptr, &dataSize, &domain);
  return domain;
}

UInt32 getUInt32Property(AudioDeviceID device_id, AudioObjectPropertySelector selector,
                         AudioObjectPropertyScope scope) {
  if (device_id == 0) {
    return 0;
  }
  AudioObjectPropertyAddress address = {selector, scope, kAudioObjectPropertyElementMain};
  UInt32 value = 0;
  UInt32 size = sizeof(value);
  return AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, &value) == noErr ? value
                                                                                             : 0;
}

UInt32 getChannelCount(AudioDeviceID device_id, AudioObjectPropertyScope scope) {
  if (device_id == 0) {
    return 0;
  }
  AudioObjectPropertyAddress address = {kAudioDevicePropertyStreamConfiguration, scope,
                                        kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &address, 0, nullptr, &size) != noErr ||
      size < sizeof(AudioBufferList)) {
    return 0;
  }

  std::vector<uint8_t> storage(size);
  auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
  if (AudioObjectGetPropertyData(device_id, &address, 0, nullptr, &size, buffers) != noErr) {
    return 0;
  }

  UInt32 channels = 0;
  for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) {
    channels += buffers->mBuffers[index].mNumberChannels;
  }
  return channels;
}

UInt32 getStreamLatency(AudioDeviceID device_id, AudioObjectPropertyScope scope) {
  AudioObjectPropertyAddress streams_address = {kAudioDevicePropertyStreams, scope,
                                                kAudioObjectPropertyElementMain};
  UInt32 size = 0;
  if (AudioObjectGetPropertyDataSize(device_id, &streams_address, 0, nullptr, &size) != noErr ||
      size == 0) {
    return 0;
  }

  std::vector<AudioStreamID> streams(size / sizeof(AudioStreamID));
  if (AudioObjectGetPropertyData(device_id, &streams_address, 0, nullptr, &size, streams.data()) !=
      noErr) {
    return 0;
  }

  UInt32 latency = 0;
  for (AudioStreamID stream : streams) {
    latency = std::max(latency, getUInt32Property(stream, kAudioStreamPropertyLatency,
                                                  kAudioObjectPropertyScopeGlobal));
  }
  return latency;
}

} // namespace

AudioDeviceID CoreAudioDriver::findDeviceByUID(const std::string& device_uid) {
  if (device_uid.empty()) {
    return 0;
  }

  CFStringRef requested =
      CFStringCreateWithCString(kCFAllocatorDefault, device_uid.c_str(), kCFStringEncodingUTF8);
  if (!requested) {
    return 0;
  }

  AudioDeviceID matched = 0;
  for (const AudioDeviceID candidate : enumerateDevices()) {
    CFStringRef candidateUID = copyDeviceUID(candidate);
    if (candidateUID && CFStringCompare(candidateUID, requested, 0) == kCFCompareEqualTo) {
      matched = candidate;
    }
    if (candidateUID) {
      CFRelease(candidateUID);
    }
    if (matched != 0) {
      break;
    }
  }
  CFRelease(requested);
  return matched;
}

bool CoreAudioDriver::supportsDirection(AudioDeviceID device_id,
                                        AudioObjectPropertyScope scope) const {
  return getChannelCount(device_id, scope) > 0;
}

AudioDeviceID CoreAudioDriver::resolveInputOutputDevice() {
  input_channel_offset_ = 0;
  const AudioDeviceID outputID = config_.output_device_id.empty()
                                     ? getDefaultDevice(kAudioHardwarePropertyDefaultOutputDevice)
                                     : findDeviceByUID(config_.output_device_id);
  if (outputID == 0 || !supportsDirection(outputID, kAudioObjectPropertyScopeOutput)) {
    return 0;
  }

  if (config_.num_inputs == 0) {
    if (!config_.input_device_id.empty()) {
      return 0;
    }
    input_device_id_ = 0;
    output_device_id_ = outputID;
    return outputID;
  }

  const AudioDeviceID inputID = config_.input_device_id.empty()
                                    ? getDefaultDevice(kAudioHardwarePropertyDefaultInputDevice)
                                    : findDeviceByUID(config_.input_device_id);
  const UInt32 inputChannelCount = getChannelCount(inputID, kAudioObjectPropertyScopeInput);
  if (inputID == 0 || inputChannelCount < config_.num_inputs) {
    return 0;
  }

  input_device_id_ = inputID;
  output_device_id_ = outputID;
  if (inputID == outputID) {
    input_channel_offset_ = 0;
    return outputID;
  }

  // Aggregate input channels retain sub-device order. A duplex playback
  // endpoint contributes its capture channels before the dedicated input
  // endpoint, so map past them rather than silently metering/recording the
  // playback device's first (often disconnected) input.
  input_channel_offset_ = getChannelCount(outputID, kAudioObjectPropertyScopeInput);
  const AudioDeviceID aggregateID = createAggregateDevice(inputID, outputID);
  if (aggregateID == 0) {
    input_device_id_ = 0;
    output_device_id_ = 0;
    return 0;
  }
  aggregate_device_id_ = aggregateID;
  return aggregateID;
}

AudioDeviceID CoreAudioDriver::createAggregateDevice(AudioDeviceID input_device_id,
                                                     AudioDeviceID output_device_id) {
  CFStringRef inputUID = copyDeviceUID(input_device_id);
  CFStringRef outputUID = copyDeviceUID(output_device_id);
  if (!inputUID || !outputUID) {
    if (inputUID)
      CFRelease(inputUID);
    if (outputUID)
      CFRelease(outputUID);
    return 0;
  }

  // Drift compensation resamples a non-master sub-device to track the
  // master's clock -- necessary (and worth its latency cost) when the two
  // devices run off genuinely independent hardware clocks, but pure
  // overhead when they don't. Built-in devices on the same Mac (e.g. the
  // microphone and speakers here) commonly share one hardware clock domain,
  // in which case forcing compensation on adds real, otherwise-avoidable
  // capture latency for no correctness benefit. Query it and only enable
  // compensation on the (non-master) input sub-device when the domains
  // actually differ.
  const UInt32 inputClockDomain = getClockDomain(input_device_id);
  const UInt32 outputClockDomain = getClockDomain(output_device_id);
  const bool sameClockDomain = inputClockDomain != 0 && inputClockDomain == outputClockDomain;
  const int driftCompensation = sameClockDomain ? 0 : 1;

  CFMutableDictionaryRef inputSubDevice = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(inputSubDevice, CFSTR(kAudioSubDeviceUIDKey), inputUID);
  CFNumberRef driftCompensationValue =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &driftCompensation);
  CFDictionarySetValue(inputSubDevice, CFSTR(kAudioSubDeviceDriftCompensationKey),
                       driftCompensationValue);
  if (!sameClockDomain) {
    const int maxQuality = kAudioSubDeviceDriftCompensationMaxQuality;
    CFNumberRef qualityValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &maxQuality);
    CFDictionarySetValue(inputSubDevice, CFSTR(kAudioSubDeviceDriftCompensationQualityKey),
                         qualityValue);
    CFRelease(qualityValue);
  }
  CFRelease(driftCompensationValue);

  // The master (clock source) sub-device is never compensated against
  // itself.
  CFMutableDictionaryRef outputSubDevice = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(outputSubDevice, CFSTR(kAudioSubDeviceUIDKey), outputUID);

  CFMutableArrayRef subDeviceList =
      CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
  CFArrayAppendValue(subDeviceList, outputSubDevice);
  CFArrayAppendValue(subDeviceList, inputSubDevice);

  // Unique per driver instance so two overlapping CoreAudioDriver instances
  // (unusual, but not forbidden by this class) never collide on UID.
  std::ostringstream uidStream;
  uidStream << "com.orpheus.sdk.aggregate." << static_cast<const void*>(this);
  CFStringRef aggregateUID = CFStringCreateWithCString(kCFAllocatorDefault, uidStream.str().c_str(),
                                                       kCFStringEncodingUTF8);

  CFMutableDictionaryRef aggregateDict = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  // Private + non-stacked: never listed in System Settings -> Sound, and
  // exists only for this driver instance's lifetime (destroyed in
  // cleanupAudioUnit() via AudioHardwareDestroyAggregateDevice).
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceNameKey),
                       CFSTR("Orpheus SDK I/O Bridge"));
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceUIDKey), aggregateUID);
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceSubDeviceListKey), subDeviceList);
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceMasterSubDeviceKey), outputUID);
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceIsPrivateKey), kCFBooleanTrue);
  CFDictionarySetValue(aggregateDict, CFSTR(kAudioAggregateDeviceIsStackedKey), kCFBooleanFalse);

  AudioDeviceID aggregateID = kAudioObjectUnknown;
  OSStatus status = AudioHardwareCreateAggregateDevice(aggregateDict, &aggregateID);

  CFRelease(inputSubDevice);
  CFRelease(outputSubDevice);
  CFRelease(subDeviceList);
  CFRelease(aggregateDict);
  CFRelease(aggregateUID);
  CFRelease(inputUID);
  CFRelease(outputUID);

  return (status == noErr) ? aggregateID : 0;
}

uint32_t CoreAudioDriver::queryDeviceLatency() const {
  uint64_t latency = 0;

  const auto add_route = [&](AudioDeviceID device_id, AudioObjectPropertyScope scope) {
    latency += getUInt32Property(device_id, kAudioDevicePropertyLatency, scope);
    latency += getUInt32Property(device_id, kAudioDevicePropertySafetyOffset, scope);
    latency += getStreamLatency(device_id, scope);
  };

  if (config_.num_inputs > 0) {
    add_route(input_device_id_, kAudioObjectPropertyScopeInput);
  }
  if (config_.num_outputs > 0) {
    add_route(output_device_id_, kAudioObjectPropertyScopeOutput);
  }

  // CoreAudio's round-trip formula counts the active I/O buffer once. The
  // requested size is only advisory, so an unavailable readback means latency
  // is undetected — never substitute an estimate.
  const UInt32 buffer_frames = getUInt32Property(device_id_, kAudioDevicePropertyBufferFrameSize,
                                                 kAudioObjectPropertyScopeGlobal);
  if (buffer_frames == 0) {
    return 0;
  }
  latency += buffer_frames;

  // Includes converters and aggregate-device drift compensation not represented
  // by the physical endpoint properties.
  if (audio_unit_ != nullptr) {
    Float64 seconds = 0.0;
    UInt32 size = sizeof(seconds);
    if (AudioUnitGetProperty(audio_unit_, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0,
                             &seconds, &size) == noErr &&
        seconds > 0.0) {
      latency +=
          static_cast<uint64_t>(std::llround(seconds * static_cast<Float64>(config_.sample_rate)));
    }
  }

  return static_cast<uint32_t>(std::min<uint64_t>(latency, std::numeric_limits<uint32_t>::max()));
}

SessionGraphError CoreAudioDriver::setupAudioUnit(AudioDeviceID device_id) {
  // Create AudioComponentDescription for HAL Output
  AudioComponentDescription desc = {};
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_HALOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags = 0;
  desc.componentFlagsMask = 0;

  // Find AudioComponent
  AudioComponent component = AudioComponentFindNext(nullptr, &desc);
  if (!component) {
    return SessionGraphError::InternalError;
  }

  // Create AudioUnit instance
  OSStatus status = AudioComponentInstanceNew(component, &audio_unit_);
  if (status != noErr || !audio_unit_) {
    return SessionGraphError::InternalError;
  }

  // Enable input IO (bus 1) only when the host asked for capture. Pure-playback
  // hosts (num_inputs == 0) keep the historical output-only behavior. Bus 1 is
  // the AudioUnit's input element; kAudioUnitSubType_HALOutput supports both
  // capture (bus 1) and playback (bus 0) on a single unit.
  UInt32 enableIO = config_.num_inputs > 0 ? 1 : 0;
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));

  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  // Enable output
  enableIO = 1;
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_EnableIO,
                                kAudioUnitScope_Output, 0, &enableIO, sizeof(enableIO));

  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  // Set current device
  status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_CurrentDevice,
                                kAudioUnitScope_Global, 0, &device_id, sizeof(AudioDeviceID));

  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  // CRITICAL FIX: Set the DEVICE's nominal sample rate to match our requested rate
  // This must be done BEFORE setting the AudioUnit's stream format
  Float64 requestedSampleRate = static_cast<Float64>(config_.sample_rate);
  AudioObjectPropertyAddress deviceSRAddr = {kAudioDevicePropertyNominalSampleRate,
                                             kAudioObjectPropertyScopeGlobal,
                                             kAudioObjectPropertyElementMain};
  status = AudioObjectSetPropertyData(device_id, &deviceSRAddr, 0, nullptr, sizeof(Float64),
                                      &requestedSampleRate);
  if (status != noErr) {
    // Don't fail completely, but this will cause playback speed issues.
  }

  // Set stream format (planar float32)
  AudioStreamBasicDescription streamFormat = {};
  streamFormat.mSampleRate = config_.sample_rate;
  streamFormat.mFormatID = kAudioFormatLinearPCM;
  streamFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
  streamFormat.mBytesPerPacket = sizeof(float);
  streamFormat.mFramesPerPacket = 1;
  streamFormat.mBytesPerFrame = sizeof(float);
  streamFormat.mChannelsPerFrame = config_.num_outputs;
  streamFormat.mBitsPerChannel = 32;

  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                0, &streamFormat, sizeof(streamFormat));

  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  // Set the input stream format when capture is enabled. This is the format the
  // AudioUnit delivers to our AudioUnitRender pull on bus 1, so it goes on
  // kAudioUnitScope_Output, element 1 (the output side of the input element).
  // Mirror the output ASBD (planar float32) but with the input channel count so
  // AudioUnitRender fills our planar input_buffers_ directly, no deinterleave.
  if (config_.num_inputs > 0) {
    AudioStreamBasicDescription inputFormat = streamFormat;
    inputFormat.mChannelsPerFrame = config_.num_inputs;

    status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 1, &inputFormat, sizeof(inputFormat));

    if (status != noErr) {
      return SessionGraphError::InternalError;
    }
    std::vector<SInt32> inputChannelMap(config_.num_inputs);
    for (uint32_t channel = 0; channel < config_.num_inputs; ++channel) {
      inputChannelMap[channel] = static_cast<SInt32>(input_channel_offset_ + channel);
    }
    status = AudioUnitSetProperty(audio_unit_, kAudioOutputUnitProperty_ChannelMap,
                                  kAudioUnitScope_Output, 1, inputChannelMap.data(),
                                  static_cast<UInt32>(inputChannelMap.size() * sizeof(SInt32)));
    if (status != noErr) {
      return SessionGraphError::InternalError;
    }
  }

  // Set buffer size (if supported)
  UInt32 bufferFrames = config_.buffer_size;
  status = AudioUnitSetProperty(audio_unit_, kAudioDevicePropertyBufferFrameSize,
                                kAudioUnitScope_Global, 0, &bufferFrames, sizeof(bufferFrames));

  // Note: Buffer size setting may fail on some devices, but continue anyway

  // Set render callback
  AURenderCallbackStruct callbackStruct = {};
  callbackStruct.inputProc = &CoreAudioDriver::renderCallback;
  callbackStruct.inputProcRefCon = this;

  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_SetRenderCallback,
                                kAudioUnitScope_Input, 0, &callbackStruct, sizeof(callbackStruct));

  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  // Initialize AudioUnit
  status = AudioUnitInitialize(audio_unit_);
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  return SessionGraphError::OK;
}

void CoreAudioDriver::createSampleRateMonitor() {
  std::vector<AudioDeviceID> monitored_devices;
  monitored_devices.reserve(3);
  monitored_devices.push_back(device_id_);
  monitored_devices.push_back(output_device_id_);
  if (config_.num_inputs > 0) {
    monitored_devices.push_back(input_device_id_);
  }
  sample_rate_monitor_ = std::make_unique<CoreAudioSampleRateMonitor>(
      sample_rate_property_api_, config_.sample_rate, std::move(monitored_devices));
}

bool CoreAudioDriver::startSampleRateMonitorLocked() {
  std::lock_guard<std::mutex> monitor_lock(sample_rate_monitor_mutex_);
  if (!sample_rate_monitor_ || !sample_rate_monitor_->start()) {
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateListenerFailure,
                           std::memory_order_release);
    return false;
  }

  sample_rate_monitor_->requestCheck();
  const CoreAudioSampleRatePollResult result = sample_rate_monitor_->poll();
  switch (result) {
  case CoreAudioSampleRatePollResult::NoChange:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
    return true;
  case CoreAudioSampleRatePollResult::RateRestored:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateRestored,
                           std::memory_order_release);
    return true;
  case CoreAudioSampleRatePollResult::ReinitializationRequired:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateReinitializationRequired,
                           std::memory_order_release);
    break;
  case CoreAudioSampleRatePollResult::QueryFailed:
    runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateQueryFailure,
                           std::memory_order_release);
    break;
  }

  sample_rate_monitor_->stop();
  return false;
}

void CoreAudioDriver::stopSampleRateMonitor() {
  sample_rate_monitor_active_.store(false, std::memory_order_release);
  {
    std::lock_guard<std::mutex> monitor_lock(sample_rate_monitor_mutex_);
    if (sample_rate_monitor_) {
      sample_rate_monitor_->stop();
      sample_rate_monitor_->requestCheck();
    }
  }
  if (sample_rate_monitor_thread_.joinable()) {
    sample_rate_monitor_thread_.join();
  }
}

void CoreAudioDriver::sampleRateMonitorLoop() {
  while (sample_rate_monitor_active_.load(std::memory_order_acquire)) {
    sample_rate_monitor_->waitForChange();
    if (!sample_rate_monitor_active_.load(std::memory_order_acquire)) {
      break;
    }

    CoreAudioSampleRatePollResult result;
    {
      std::lock_guard<std::mutex> monitor_lock(sample_rate_monitor_mutex_);
      if (!sample_rate_monitor_active_.load(std::memory_order_acquire)) {
        break;
      }
      result = sample_rate_monitor_->poll();
      if (result == CoreAudioSampleRatePollResult::ReinitializationRequired ||
          result == CoreAudioSampleRatePollResult::QueryFailed) {
        sample_rate_monitor_->stop();
      }
    }

    switch (result) {
    case CoreAudioSampleRatePollResult::NoChange:
      runtime_outcome_.store(AudioDriverRuntimeOutcome::Healthy, std::memory_order_release);
      continue;
    case CoreAudioSampleRatePollResult::RateRestored:
      runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateRestored,
                             std::memory_order_release);
      continue;
    case CoreAudioSampleRatePollResult::ReinitializationRequired:
      runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateReinitializationRequired,
                             std::memory_order_release);
      break;
    case CoreAudioSampleRatePollResult::QueryFailed:
      runtime_outcome_.store(AudioDriverRuntimeOutcome::SampleRateQueryFailure,
                             std::memory_order_release);
      break;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopRenderingLocked();
    }
    sample_rate_monitor_active_.store(false, std::memory_order_release);
  }
}

void CoreAudioDriver::stopRenderingLocked() {
  is_running_.store(false, std::memory_order_release);
  if (audio_unit_) {
    AudioOutputUnitStop(audio_unit_);
  }
  callback_target_.replaceAndDrain(nullptr);
}

void CoreAudioDriver::cleanupAudioUnit() {
  // Callers stop and join the control worker before destroying the route.
  sample_rate_monitor_.reset();
  if (audio_unit_) {
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
    audio_unit_ = nullptr;
  }

  if (aggregate_device_id_ != 0) {
    AudioHardwareDestroyAggregateDevice(aggregate_device_id_);
    aggregate_device_id_ = 0;
  }

  device_id_ = 0;
  input_device_id_ = 0;
  output_device_id_ = 0;
  input_channel_offset_ = 0;
}

// Factory function
std::unique_ptr<IAudioDriver> createCoreAudioDriver() {
  return std::make_unique<CoreAudioDriver>();
}

} // namespace orpheus
