// SPDX-License-Identifier: MIT
#include "coreaudio_driver.h"

#include <orpheus/performance_monitor.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

namespace orpheus {

CoreAudioDriver::CoreAudioDriver() = default;

CoreAudioDriver::~CoreAudioDriver() {
  if (is_running_.load(std::memory_order_acquire)) {
    stop();
  }
  cleanupAudioUnit();
}

SessionGraphError CoreAudioDriver::initialize(const AudioDriverConfig& config) {
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

  // Store configuration
  config_ = config;

  // Find device
  device_id_ = findDevice(config.device_name);
  if (device_id_ == 0) {
    return SessionGraphError::InvalidParameter;
  }

  // Set up AudioUnit
  SessionGraphError result = setupAudioUnit(device_id_);
  if (result != SessionGraphError::OK) {
    cleanupAudioUnit();
    return result;
  }

  // Query and store latency
  latency_samples_.store(queryDeviceLatency(device_id_), std::memory_order_release);

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

  callback_ = callback;

  // Start AudioUnit
  OSStatus status = AudioOutputUnitStart(audio_unit_);
  if (status != noErr) {
    return SessionGraphError::InternalError;
  }

  is_running_.store(true, std::memory_order_release);
  return SessionGraphError::OK;
}

SessionGraphError CoreAudioDriver::stop() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!is_running_.load(std::memory_order_acquire)) {
    return SessionGraphError::OK; // Already stopped
  }

  is_running_.store(false, std::memory_order_release);

  if (audio_unit_) {
    // AudioOutputUnitStop() is not documented as a synchronous render-callback
    // drain. Track callbacks we can observe and wait only until they exit.
    AudioOutputUnitStop(audio_unit_);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    while (callbacks_in_flight_.load(std::memory_order_acquire) != 0 &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
  }

  callback_ = nullptr;

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
  return latency_samples_.load(std::memory_order_acquire);
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
  caps.supports_exclusive_mode = false;
  caps.supports_shared_mode = true;
  caps.supports_device_hot_swap = true;
  caps.supports_input = config_.num_inputs > 0;
  caps.supports_multichannel_output = config_.num_outputs > 2;
  caps.reports_hardware_latency = getLatencySamples() > 0;
  return caps;
}

void CoreAudioDriver::setPerformanceMonitor(IPerformanceMonitor* monitor) {
  performance_monitor_.store(monitor, std::memory_order_release);
}

// Static audio callback
OSStatus CoreAudioDriver::renderCallback(void* inRefCon, AudioUnitRenderActionFlags* ioActionFlags,
                                         const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber,
                                         UInt32 inNumberFrames, AudioBufferList* ioData) {
  (void)inBusNumber;

  auto* driver = static_cast<CoreAudioDriver*>(inRefCon);
  assert(driver != nullptr);

  struct CallbackScope {
    explicit CallbackScope(CoreAudioDriver& d) : driver(d) {
      driver.callbacks_in_flight_.fetch_add(1, std::memory_order_acq_rel);
    }
    ~CallbackScope() {
      driver.callbacks_in_flight_.fetch_sub(1, std::memory_order_acq_rel);
    }
    CoreAudioDriver& driver;
  } callback_scope(*driver);

  // Zero output buffers first
  for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
    std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
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

  IAudioCallback* callback = driver->callback_;
  if (!callback) {
    return noErr; // No callback set, output silence
  }

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
    // we never propagate a hard error up the audio graph for a transient miss.
    AudioUnitRender(driver->audio_unit_, ioActionFlags, inTimeStamp, /*inputBus=*/1,
                    frames_to_process, inputABL);
  }

  // Invoke user callback (lock-free). Timing is opt-in for diagnostics builds;
  // production callbacks should not pay for hot-path instrumentation.
  const float** input_ptrs = driver->input_buffers_.empty()
                                 ? nullptr
                                 : const_cast<const float**>(driver->input_buffers_.data());
  float** output_ptrs = driver->output_buffers_.data();

#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
  const UInt64 callback_start = AudioGetCurrentHostTime();
#endif
  callback->processAudio(input_ptrs, output_ptrs, num_channels, frames_to_process);
#if defined(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING)
  const UInt64 callback_end = AudioGetCurrentHostTime();

  if (auto* monitor = driver->performance_monitor_.load(std::memory_order_acquire)) {
    const UInt64 duration_ns = AudioConvertHostTimeToNanos(callback_end - callback_start);
    uint64_t callback_duration_us = static_cast<uint64_t>(duration_ns / 1000u);
    uint64_t buffer_duration_us =
        (static_cast<uint64_t>(frames_to_process) * 1'000'000ull) / driver->config_.sample_rate;

    // TODO: Get active clip count from transport controller (for now, use 0)
    uint32_t active_clips = 0;

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

AudioDeviceID CoreAudioDriver::findDevice(const std::string& device_name) {
  // If no device name specified, use default output device
  if (device_name.empty()) {
    AudioObjectPropertyAddress propertyAddress = {kAudioHardwarePropertyDefaultOutputDevice,
                                                  kAudioObjectPropertyScopeGlobal,
                                                  kAudioObjectPropertyElementMain};

    AudioDeviceID deviceID = 0;
    UInt32 dataSize = sizeof(AudioDeviceID);

    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0,
                                                 nullptr, &dataSize, &deviceID);

    if (status == noErr && deviceID != kAudioObjectUnknown) {
      return deviceID;
    }

    return 0;
  }

  // Search for device by name
  auto devices = enumerateDevices();
  for (AudioDeviceID deviceID : devices) {
    std::string name = getDeviceName(deviceID);
    if (name == device_name) {
      return deviceID;
    }
  }

  return 0; // Not found
}

std::string CoreAudioDriver::getDeviceName(AudioDeviceID device_id) {
  AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyDeviceNameCFString,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};

  CFStringRef cfName = nullptr;
  UInt32 dataSize = sizeof(CFStringRef);

  OSStatus status =
      AudioObjectGetPropertyData(device_id, &propertyAddress, 0, nullptr, &dataSize, &cfName);

  if (status != noErr || !cfName) {
    return "";
  }

  // Convert CFString to std::string
  char buffer[256];
  Boolean success = CFStringGetCString(cfName, buffer, sizeof(buffer), kCFStringEncodingUTF8);
  CFRelease(cfName);

  return success ? std::string(buffer) : "";
}

uint32_t CoreAudioDriver::queryDeviceLatency(AudioDeviceID device_id) {
  AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyLatency,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMain};

  UInt32 latency = 0;
  UInt32 dataSize = sizeof(UInt32);

  OSStatus status =
      AudioObjectGetPropertyData(device_id, &propertyAddress, 0, nullptr, &dataSize, &latency);

  if (status != noErr) {
    // If we can't query latency, estimate based on buffer size
    return config_.buffer_size * 2; // Conservative estimate (double buffer)
  }

  // Add buffer size to device latency
  return latency + config_.buffer_size;
}

SessionGraphError CoreAudioDriver::setupAudioUnit(AudioDeviceID device_id) {
  FILE* diagFile = fopen("/tmp/coreaudio_init.log", "a");
  if (diagFile) {
    fprintf(diagFile, "CoreAudio: setupAudioUnit() called, device_id=%u\n", device_id);
    fclose(diagFile);
  }

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
    FILE* diagFile = fopen("/tmp/coreaudio_init.log", "a");
    if (diagFile) {
      fprintf(diagFile, "CoreAudio: WARNING - Failed to set device sample rate (status: %d)\n",
              (int)status);
      fclose(diagFile);
    }
    // Don't fail completely, but this will cause playback speed issues
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

  // DIAGNOSTIC: Verify actual sample rate after initialization
  AudioStreamBasicDescription actualFormat = {};
  UInt32 formatSize = sizeof(actualFormat);
  status = AudioUnitGetProperty(audio_unit_, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
                                0, &actualFormat, &formatSize);

  // Also query the DEVICE's nominal sample rate (not just the AudioUnit)
  Float64 deviceSampleRate = 0.0;
  UInt32 sizeOfSampleRate = sizeof(Float64);
  AudioObjectPropertyAddress querySRAddr = {kAudioDevicePropertyNominalSampleRate,
                                            kAudioObjectPropertyScopeGlobal,
                                            kAudioObjectPropertyElementMain};
  OSStatus deviceSRStatus = AudioObjectGetPropertyData(device_id, &querySRAddr, 0, nullptr,
                                                       &sizeOfSampleRate, &deviceSampleRate);

  FILE* f = fopen("/tmp/coreaudio_init.log", "a");
  if (f) {
    fprintf(f, "CoreAudio: Requested sample rate: %u Hz\n", config_.sample_rate);
    if (status == noErr) {
      fprintf(f, "CoreAudio: AudioUnit sample rate: %.1f Hz\n", actualFormat.mSampleRate);
    } else {
      fprintf(f, "CoreAudio: Failed to query AudioUnit sample rate (status: %d)\n", (int)status);
    }
    if (deviceSRStatus == noErr) {
      fprintf(f, "CoreAudio: DEVICE nominal sample rate: %.1f Hz\n", deviceSampleRate);
      if (deviceSampleRate != static_cast<double>(config_.sample_rate)) {
        fprintf(f,
                "CoreAudio: ***CRITICAL*** DEVICE RATE MISMATCH! Ratio: %.6f (this causes slow "
                "playback!)\n",
                static_cast<double>(config_.sample_rate) / deviceSampleRate);
      }
    } else {
      fprintf(f, "CoreAudio: Failed to query device sample rate (status: %d)\n",
              (int)deviceSRStatus);
    }
    fflush(f); // Ensure it's written immediately
    fclose(f);
  }

  return SessionGraphError::OK;
}

void CoreAudioDriver::cleanupAudioUnit() {
  if (audio_unit_) {
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
    audio_unit_ = nullptr;
  }

  device_id_ = 0;
}

// Factory function
std::unique_ptr<IAudioDriver> createCoreAudioDriver() {
  return std::make_unique<CoreAudioDriver>();
}

} // namespace orpheus
