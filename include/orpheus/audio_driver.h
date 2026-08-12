// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orpheus {

class IPerformanceMonitor;

/// Physical-to-logical channel selections for a configured route.
struct AudioRouteChannelMap {
  std::vector<uint16_t> input_channels;
  std::vector<uint16_t> output_channels;
};
enum class AudioSampleRatePolicy : uint8_t {
  PreserveDeviceRate = 0,
  RequestExactRate = 1,
  RequestExactRateOrConvert = 2,
};

enum class AudioOutputChannelPolicy : uint8_t {
  RequireRequestedChannels = 0,
  AllowMonoFallback = 1,
};

enum class AudioRouteCompatibilityStatus : uint8_t {
  Compatible = 0,
  RequiresSampleRateChange = 1,
  InputUnavailable = 2,
  OutputUnavailable = 3,
  SampleRateUnsupported = 4,
  InvalidChannelMap = 5,
  PermissionDenied = 6,
  BackendFailure = 7,
  ProfileConflict = 8,
};

struct AudioRouteCompatibility {
  AudioRouteCompatibilityStatus status = AudioRouteCompatibilityStatus::BackendFailure;
  std::string resolved_input_device_id;
  std::string resolved_output_device_id;
  uint32_t requested_sample_rate = 0;
  uint32_t current_input_sample_rate = 0;
  uint32_t current_output_sample_rate = 0;
  bool input_rate_change_required = false;
  bool output_rate_change_required = false;
  bool input_is_running_somewhere = false;
  bool output_is_running_somewhere = false;
  uint32_t planned_input_client_rate = 0;
  uint32_t planned_output_client_rate = 0;
  uint16_t requested_output_channels = 0;
  uint16_t resolved_output_channels = 0;
  uint16_t input_virtual_format_channels = 0;
  uint16_t output_virtual_format_channels = 0;
  bool input_conversion_required = false;
  bool output_conversion_required = false;
  bool input_is_bluetooth = false;
  bool output_is_bluetooth = false;
  bool endpoints_related = false;
  bool requires_post_bind_reprobe = false;
  bool output_mono_fallback_planned = false;
  std::string detail;
};

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

  /// Physical-to-logical channel selection for each direction.
  /// An empty vector selects consecutive physical channels starting at zero.
  AudioRouteChannelMap channel_map{};
  AudioSampleRatePolicy sample_rate_policy = AudioSampleRatePolicy::PreserveDeviceRate;
  AudioOutputChannelPolicy output_channel_policy =
      AudioOutputChannelPolicy::RequireRequestedChannels;
};

/// Decomposed route latency reported by the backend.
///
/// `complete` is true only when all mandatory terms for the active route were
/// measured. A missing term is never estimated from buffer size or wall time.
struct AudioRouteLatency {
  uint32_t capture_frames = 0;
  uint32_t playback_frames = 0;
  uint32_t processing_frames = 0;
  bool complete = false;
};

/// Runtime outcome that can invalidate active audio I/O.
enum class AudioRouteRuntimeOutcome : uint8_t {
  Healthy = 0,
  SampleRateUnsupported = 1,
  SampleRateChangeFailed = 2,
  SampleRateChanged = 3,
  BufferSizeChanged = 4,
  FormatChanged = 5,
  ChannelMapInvalid = 6,
  InputRouteUnavailable = 7,
  OutputRouteUnavailable = 8,
  PermissionDenied = 9,
  BackendFailure = 10,
  ProfileConflict = 11,
  InputConversionFailed = 12,
  OutputConversionFailed = 13,
};

/// Control-thread snapshot of the physical route negotiated by a backend.
///
/// This is route identity and capability state, not a UI catalog row. IDs are
/// persistent backend IDs; CoreAudio reports physical endpoint UIDs even when
/// it uses a private aggregate as the AudioUnit device.
struct ActiveAudioRoute {
  std::string input_device_id;
  std::string output_device_id;
  std::vector<uint16_t> input_channels;
  std::vector<uint16_t> output_channels;
  uint16_t available_input_channels = 0;
  uint16_t available_output_channels = 0;
  uint32_t requested_sample_rate = 0;
  uint32_t actual_sample_rate = 0;
  uint32_t actual_buffer_frames = 0;
  AudioRouteLatency latency;
  bool input_alive = false;
  bool output_alive = false;
  uint32_t input_physical_sample_rate = 0;
  uint32_t output_physical_sample_rate = 0;
  uint32_t input_client_sample_rate = 0;
  uint32_t output_client_sample_rate = 0;
  uint16_t requested_output_channels = 0;
  uint16_t resolved_output_channels = 0;
  uint16_t input_virtual_format_channels = 0;
  uint16_t output_virtual_format_channels = 0;
  uint16_t input_client_format_channels = 0;
  uint16_t output_client_format_channels = 0;
  bool input_conversion_active = false;
  bool output_conversion_active = false;
  bool input_is_bluetooth = false;
  bool output_is_bluetooth = false;
  bool endpoints_related = false;
  bool output_mono_fallback = false;
};

/// Playback route lifecycle as observed by a backend.
///
/// `InputUnavailable` and `OutputUnavailable` identify a failed physical
/// endpoint when the backend can distinguish a direction. Terminal states
/// require an explicit initialize() before the route may render again.
enum class AudioRouteState : uint8_t {
  Inactive,
  Starting,
  Running,
  DegradedDuplex,
  ReconfigurationRequired,
  InputUnavailable,
  OutputUnavailable,
  PermissionDenied,
  Failed,
};

/// Direction-specific latency terms. Missing terms remain unknown; callers
/// must not substitute requested buffer sizes or estimates for them.
struct AudioLatencyBreakdown {
  std::optional<uint32_t> input_device_frames;
  std::optional<uint32_t> input_safety_offset_frames;
  std::optional<uint32_t> input_stream_frames;
  std::optional<uint32_t> input_converter_frames;
  std::optional<uint32_t> output_device_frames;
  std::optional<uint32_t> output_safety_offset_frames;
  std::optional<uint32_t> output_stream_frames;
  std::optional<uint32_t> output_converter_frames;
  std::optional<uint32_t> callback_buffer_frames;
  std::optional<uint32_t> aggregate_or_audio_unit_frames;
  std::optional<uint32_t> input_audio_unit_frames;
  std::optional<uint32_t> output_audio_unit_frames;
  bool complete = false;
};

/// Strict output-only route request. Empty output_device_id follows the
/// backend's current default output; non-empty IDs are never defaulted.
///
/// The driver creates no input route. Output channels identify distinct
/// physical output indices and must be valid for the selected endpoint.
struct AudioOutputRouteRequest {
  std::string output_device_id;
  std::vector<uint16_t> output_channel_map;
  uint32_t requested_sample_rate = 48000;
  uint32_t requested_buffer_size = 512;
};

/// Control-thread snapshot of selected and active I/O route state.
struct AudioIoRouteState {
  AudioRouteState state = AudioRouteState::Inactive;
  std::string selected_input_device_id;
  std::string selected_output_device_id;
  std::string active_input_device_id;
  std::string active_output_device_id;
  std::vector<uint16_t> active_input_channel_map;
  std::vector<uint16_t> active_output_channel_map;
  uint32_t requested_sample_rate = 0;
  uint32_t actual_sample_rate = 0;
  uint32_t requested_buffer_size = 0;
  uint32_t actual_buffer_size = 0;
  AudioLatencyBreakdown latency;
  std::string detail;
  uint32_t input_physical_sample_rate = 0;
  uint32_t output_physical_sample_rate = 0;
  uint32_t input_client_sample_rate = 0;
  uint32_t output_client_sample_rate = 0;
  uint16_t requested_output_channels = 0;
  uint16_t resolved_output_channels = 0;
  uint16_t input_virtual_format_channels = 0;
  uint16_t output_virtual_format_channels = 0;
  uint16_t input_client_format_channels = 0;
  uint16_t output_client_format_channels = 0;
  bool input_conversion_active = false;
  bool output_conversion_active = false;
  bool input_is_bluetooth = false;
  bool output_is_bluetooth = false;
  bool endpoints_related = false;
  bool output_mono_fallback = false;
};
/// Outcome of a backend runtime condition that can invalidate active audio I/O.
/// A terminal outcome means the driver has stopped rendering and requires an
/// explicit initialize() call with a host-selected configuration.
/// Factory-visible backend I/O diagnostics. Runtime outcomes are safe to poll
/// from a control thread and never invoke the host's realtime callback.
struct AudioIoTelemetry {
  uint64_t input_render_failures = 0; ///< Cumulative failed capture renders
  AudioRouteRuntimeOutcome route_outcome = AudioRouteRuntimeOutcome::Healthy;
  uint64_t input_fifo_overruns = 0;
  uint64_t input_fifo_underruns = 0;
  uint64_t input_conversion_failures = 0;
  uint64_t output_conversion_failures = 0;
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

/// Audio driver callback interface.
/// Called on the audio thread - must be lock-free.
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

/// Audio driver interface.
/// Abstracts platform-specific audio I/O (CoreAudio, WASAPI, ASIO, dummy).
class IAudioDriver {
public:
  virtual ~IAudioDriver() = default;

  virtual SessionGraphError initialize(const AudioDriverConfig& config) = 0;

  /// Start audio processing. Callback must not be nullptr.
  virtual SessionGraphError start(IAudioCallback* callback) = 0;

  /// Stop audio processing.
  virtual SessionGraphError stop() = 0;

  /// Check whether the driver is running.
  virtual bool isRunning() const = 0;

  /// Get current configuration.
  virtual const AudioDriverConfig& getConfig() const = 0;

  /// Get human-readable driver name.
  virtual std::string getDriverName() const = 0;

  /// Get total measured latency in samples, or zero when unavailable.
  virtual uint32_t getLatencySamples() const = 0;

  /// Get cumulative backend diagnostics. Control-thread only.
  virtual AudioIoTelemetry getTelemetry() const noexcept {
    return {};
  }

  /// Get backend capabilities.
  ///
  /// The default is conservative so custom drivers can adopt richer route
  /// reporting incrementally.
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

  /// Set optional realtime performance monitor.
  virtual void setPerformanceMonitor(IPerformanceMonitor* monitor) {
    (void)monitor;
  }

  /// Get the negotiated physical route. Control-thread only.
  ///
  /// Appended to preserve existing IAudioDriver virtual-table slots.
  virtual ActiveAudioRoute getActiveRoute() const {
    return {};
  }

  /// Initialize a strict playback-only output route with a physical map.
  /// Appended after the existing route contract to preserve all established
  /// virtual-table slots. Drivers without this capability return NotSupported.
  virtual SessionGraphError initializeAudioOutput(const AudioOutputRouteRequest& request) {
    (void)request;
    return SessionGraphError::NotSupported;
  }

  /// Get selected/active route state on the control thread.
  ///
  /// A terminal state describes the last observed route failure. Hosts must
  /// explicitly reinitialize rather than assuming automatic endpoint fallback.
  virtual AudioIoRouteState getAudioIoRouteState() const {
    return {};
  }

  virtual AudioRouteCompatibility probeRoute(const AudioDriverConfig& config) const {
    (void)config;
    return {};
  }
};
/// Factory function for dummy audio driver (for testing).
ORPHEUS_API std::unique_ptr<IAudioDriver> createDummyAudioDriver();

/// Factory function for CoreAudio driver (macOS only).
#ifdef __APPLE__
ORPHEUS_API std::unique_ptr<IAudioDriver> createCoreAudioDriver();
#endif

#ifdef _WIN32
/// Factory function for shared-mode WASAPI driver.
ORPHEUS_API std::unique_ptr<IAudioDriver> createWASAPIAudioDriver();
#endif

} // namespace orpheus
