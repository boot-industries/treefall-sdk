// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace orpheus {

class IPerformanceMonitor;

/// Audio driver configuration.
///
/// Backend endpoint identifiers are stable backend IDs. CoreAudio consumes
/// `kAudioDevicePropertyDeviceUID` values; an empty direction selects that
/// direction's current system default.
struct AudioDriverConfig {
  uint32_t sample_rate = 48000; ///< Sample rate in Hz
  uint16_t buffer_size = 512;   ///< Buffer size in frames
  uint16_t num_inputs = 2;      ///< Number of input channels
  std::string input_device_id;  ///< Stable input endpoint ID (empty = default)
  uint16_t num_outputs = 2;     ///< Number of output channels
  std::string output_device_id; ///< Stable output endpoint ID (empty = default)
  std::string device_name;      ///< Optional host-visible display name
};

/// Factory-visible backend I/O diagnostics.
struct AudioIoTelemetry {
  uint64_t input_render_failures = 0; ///< Cumulative failed capture renders
};

/// Runtime lifecycle and capacity events. Events contain scalar snapshots only
/// and are polled by one host control thread.
enum class AudioDriverEventType : uint8_t {
  DeviceRemoved,
  DefaultRouteChanged,
  FormatChanged,
  BufferCapacityChanged,
  InterruptionBegan,
  InterruptionEnded,
  BackendRestarted,
  BackendUnderrun,
  CapacityExceeded,
};

struct AudioDriverEvent {
  AudioDriverEventType type{AudioDriverEventType::BackendRestarted};
  uint32_t sample_rate{0};
  uint32_t maximum_frames{0};
  uint64_t host_time_nanoseconds{0};
  SessionGraphError error{SessionGraphError::OK};
};

static_assert(std::is_trivially_copyable_v<AudioDriverEvent>);

struct AudioDriverRuntimeInfo {
  std::string selected_input_device_id{};
  std::string selected_output_device_id{};
  uint32_t negotiated_sample_rate{0};
  uint32_t maximum_callback_frames{0};
  uint16_t input_channels{0};
  uint16_t output_channels{0};
  uint32_t latency_samples{0};
  bool supports_runtime_events{false};
};

/// Audio backend family.
enum class AudioBackend : uint8_t {
  Unknown,
  Dummy,
  CoreAudio,
  WASAPI,
  ASIO,
  ALSA,
  JACK,
  PipeWire,
  RemoteIO
};

/// Platform family for an audio backend.
enum class AudioPlatform : uint8_t { Unknown, macOS, Windows, Linux, iOS };

/// Runtime capabilities for an audio driver/backend.
///
/// Hosts should prefer this over backend-name string parsing when deciding
/// whether low-latency, multichannel, exclusive/shared, or hot-swap workflows
/// are available on the active platform.
struct AudioDriverCapabilities {
  AudioBackend backend = AudioBackend::Unknown;
  AudioPlatform platform = AudioPlatform::Unknown;
  uint32_t min_output_channels = 0;
  uint32_t max_output_channels = 0;
  uint32_t min_input_channels = 0;
  uint32_t max_input_channels = 0;
  std::vector<uint32_t> native_sample_rates;
  std::vector<uint32_t> native_buffer_sizes;
  std::vector<std::string> input_channel_ids;
  std::vector<std::string> output_channel_ids;
  bool supports_exclusive_mode = false;
  bool supports_shared_mode = true;
  bool supports_device_hot_swap = false;
  bool supports_input = false;
  bool supports_multichannel_output = false;
  bool reports_hardware_latency = false;
};

/// One realtime audio callback block.
///
/// Channel buffers are planar. Input and output channel counts are independent:
/// hosts must not infer one from the other. device_sample_position identifies
/// the first frame in the block on the backend timeline. A value of zero means
/// the backend cannot provide a position. host_time_nanoseconds is a monotonic
/// host-clock timestamp for that frame; zero likewise means unavailable.
/// A discontinuity marks a device reset, gap, or non-contiguous stream position.
struct AudioProcessBlock {
  const float* const* input_buffers = nullptr;
  float* const* output_buffers = nullptr;
  uint16_t num_input_channels = 0;
  uint16_t num_output_channels = 0;
  uint32_t num_frames = 0;
  uint64_t device_sample_position = 0;
  uint64_t host_time_nanoseconds = 0;
  bool discontinuity = false;
};

/// Audio driver callback interface
/// Called on the audio thread - must be lock-free
class IAudioCallback {
public:
  virtual ~IAudioCallback() = default;

  /// Process one planar audio block on the backend realtime thread.
  ///
  /// The block and its pointed-to buffers remain valid only for this call.
  /// Implementations must not allocate, lock, block, perform I/O, or retain any
  /// pointer from the block.
  virtual void processAudio(const AudioProcessBlock& block) noexcept = 0;

  /// Realtime-safe active-voice diagnostic supplied by the host callback.
  virtual uint32_t activeClipCount() const noexcept {
    return 0;
  }
};

/// Audio driver interface
/// Abstracts platform-specific audio I/O (CoreAudio, WASAPI, ASIO, dummy)
class IAudioDriver {
public:
  virtual ~IAudioDriver() = default;

  /// Initialize the audio driver
  /// @param config Driver configuration
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError initialize(const AudioDriverConfig& config) = 0;

  /// Start audio processing
  /// @param callback Callback interface for audio processing (must not be nullptr)
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError start(IAudioCallback* callback) = 0;

  /// Stop audio processing
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError stop() = 0;

  /// Check if driver is currently running
  virtual bool isRunning() const = 0;

  /// Get current configuration
  virtual const AudioDriverConfig& getConfig() const = 0;

  /// Get driver name (e.g., "Dummy", "CoreAudio", "WASAPI")
  virtual std::string getDriverName() const = 0;

  /// Get current device latency in samples.
  /// @return Total round-trip latency (input + output)
  virtual uint32_t getLatencySamples() const = 0;

  /// Get lock-free backend I/O diagnostics. The default remains zero for
  /// drivers that do not expose backend failures.
  virtual AudioIoTelemetry getTelemetry() const noexcept {
    return {};
  }

  /// Negotiated endpoint and callback-capacity snapshot. The conservative
  /// default reports configured values and no runtime event support.
  virtual AudioDriverRuntimeInfo getRuntimeInfo() const {
    AudioDriverRuntimeInfo info;
    info.negotiated_sample_rate = getConfig().sample_rate;
    info.maximum_callback_frames = getConfig().buffer_size;
    info.input_channels = getConfig().num_inputs;
    info.output_channels = getConfig().num_outputs;
    info.latency_samples = getLatencySamples();
    return info;
  }

  /// One control-thread consumer polls fixed-size runtime events.
  virtual bool pollEvent(AudioDriverEvent& event) noexcept {
    (void)event;
    return false;
  }

  virtual uint64_t droppedEventCount() const noexcept {
    return 0;
  }

  /// Get runtime backend/device capabilities.
  ///
  /// Default implementation is intentionally conservative so older/custom
  /// driver implementations remain source-compatible until they can report
  /// richer platform details.
  virtual AudioDriverCapabilities getCapabilities() const {
    AudioDriverCapabilities caps;
    caps.min_output_channels = getConfig().num_outputs;
    caps.max_output_channels = getConfig().num_outputs;
    caps.min_input_channels = getConfig().num_inputs;
    caps.max_input_channels = getConfig().num_inputs;
    caps.native_sample_rates.push_back(getConfig().sample_rate);
    caps.native_buffer_sizes.push_back(getConfig().buffer_size);
    caps.supports_input = getConfig().num_inputs > 0;
    caps.supports_multichannel_output = getConfig().num_outputs > 2;
    caps.reports_hardware_latency = getLatencySamples() > 0;
    return caps;
  }

  /// Attach a non-owning realtime performance sink; nullptr detaches it.
  virtual void setPerformanceMonitor(IPerformanceMonitor* monitor) {
    (void)monitor;
  }
};

/// Factory function for dummy audio driver (for testing)
/// @return New dummy audio driver instance
ORPHEUS_API std::unique_ptr<IAudioDriver> createDummyAudioDriver();

/// Factory function for CoreAudio driver (macOS only)
/// @return New CoreAudio driver instance
#ifdef __APPLE__
ORPHEUS_API std::unique_ptr<IAudioDriver> createCoreAudioDriver();
#endif

#ifdef _WIN32
/// Factory function for shared-mode WASAPI driver.
ORPHEUS_API std::unique_ptr<IAudioDriver> createWASAPIAudioDriver();
#endif

} // namespace orpheus
