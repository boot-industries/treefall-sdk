// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace orpheus {

class IPerformanceMonitor;

/// Audio driver configuration
struct AudioDriverConfig {
  uint32_t sample_rate = 48000; ///< Sample rate in Hz
  uint16_t buffer_size = 512;   ///< Buffer size in frames
  uint16_t num_inputs = 2;      ///< Number of input channels
  std::string device_id;        ///< Stable backend device ID (empty = default)
  uint16_t num_outputs = 2;     ///< Number of output channels
  std::string device_name;      ///< Device name (empty = default device)
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
  bool supports_exclusive_mode = false;
  bool supports_shared_mode = true;
  bool supports_device_hot_swap = false;
  bool supports_input = false;
  bool supports_multichannel_output = false;
  bool reports_hardware_latency = false;
};

/// Audio driver callback interface
/// Called on the audio thread - must be lock-free
class IAudioCallback {
public:
  virtual ~IAudioCallback() = default;

  /// Process audio callback (audio thread)
  /// @param input_buffers Array of input channel buffers (may be nullptr if num_inputs == 0)
  /// @param output_buffers Array of output channel buffers (never nullptr)
  /// @param num_channels Number of channels (matches config)
  /// @param num_frames Number of frames to process
  virtual void processAudio(const float** input_buffers, float** output_buffers,
                            size_t num_channels, size_t num_frames) = 0;

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

  /// Get current device latency in samples
  /// @return Total round-trip latency (input + output)
  virtual uint32_t getLatencySamples() const = 0;

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
