// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/transport_controller.h> // For SessionGraphError

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace orpheus {

/// Audio driver configuration
struct AudioDriverConfig {
  uint32_t sample_rate = 48000; ///< Sample rate in Hz
  uint16_t buffer_size = 512;   ///< Buffer size in frames
  uint16_t num_inputs = 2;      ///< Number of input channels
  uint16_t num_outputs = 2;     ///< Number of output channels
  std::string device_name;      ///< Device name (empty = default device)
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
};

/// Factory function for dummy audio driver (for testing)
/// @return New dummy audio driver instance
std::unique_ptr<IAudioDriver> createDummyAudioDriver();

/// Factory function for CoreAudio driver (macOS only)
/// @return New CoreAudio driver instance
#ifdef __APPLE__
std::unique_ptr<IAudioDriver> createCoreAudioDriver();
#endif

} // namespace orpheus
