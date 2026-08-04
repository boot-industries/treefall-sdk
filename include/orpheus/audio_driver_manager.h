// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace orpheus {

// Forward declaration
class IAudioDriver;

/// Audio device information
struct AudioDeviceInfo {
  std::string deviceId;                       ///< Unique device identifier
  std::string name;                           ///< Human-readable device name
  std::string driverType;                     ///< "CoreAudio", "ASIO", "WASAPI", "ALSA", "Dummy"
  uint32_t minChannels;                       ///< Minimum output channels
  uint32_t maxChannels;                       ///< Maximum output channels
  std::vector<uint32_t> supportedSampleRates; ///< e.g., {44100, 48000, 96000}
  std::vector<uint32_t> supportedBufferSizes; ///< e.g., {128, 256, 512, 1024}
  bool isDefaultDevice;                       ///< true if system default
};

/// Audio driver manager for device enumeration and selection
///
/// This interface provides runtime audio device enumeration, configuration,
/// and hot-swap capabilities for the Orpheus SDK.
///
/// Thread Safety:
/// - enumerateDevices(), getDeviceInfo(), setActiveDevice(): control/UI thread only
/// - getCurrentDevice(), getCurrentSampleRate(), getCurrentBufferSize(): thread-safe
/// - setDeviceChangeCallback(): control/UI thread only
/// Platform Support:
/// - macOS: CoreAudio device enumeration
/// - Windows: WASAPI shared-mode device enumeration and playback
/// - Linux: no released device backend; dummy/offline only
/// - All platforms: Dummy driver (for testing)
class IAudioDriverManager {
public:
  virtual ~IAudioDriverManager() = default;

  /// Enumerate all available audio devices
  ///
  /// This method queries the system for all available audio output devices,
  /// including the dummy driver (always available for testing).
  ///
  /// @return Vector of device info structures
  /// @note This may block briefly (10-100ms) while querying hardware
  /// @note Dummy driver is always included as first device
  virtual std::vector<AudioDeviceInfo> enumerateDevices() = 0;

  /// Get detailed information about specific device
  ///
  /// @param deviceId Device identifier from enumerateDevices()
  /// @return Device info, or std::nullopt if device not found
  /// @note Control thread/UI thread only; device enumeration may query hardware.
  virtual std::optional<AudioDeviceInfo> getDeviceInfo(const std::string& deviceId) = 0;

  /// Set active audio device (transactional hot-swap)
  ///
  /// A candidate is created and initialized before the current driver is
  /// stopped. Candidate or stop failure preserves the current driver and
  /// configuration. On success ownership/configuration commit atomically, then
  /// the change callback is invoked after the manager lock is released.
  ///
  /// @param deviceId Device identifier
  /// @param sampleRate Desired sample rate (must be in supportedSampleRates)
  /// @param bufferSize Desired buffer size (must be in supportedBufferSizes)
  /// @return SessionGraphError::OK on success
  /// @warning May cause brief audio dropout (~100ms) after a successful switch.
  virtual SessionGraphError setActiveDevice(const std::string& deviceId, uint32_t sampleRate,
                                            uint32_t bufferSize) = 0;

  /// Get currently active device
  ///
  /// @return Device ID, or std::nullopt if no device active
  /// @note Thread-safe, can be called from any thread
  virtual std::optional<std::string> getCurrentDevice() const = 0;

  /// Get current sample rate
  ///
  /// @return Current sample rate in Hz (e.g., 48000)
  /// @note Thread-safe, can be called from any thread
  virtual uint32_t getCurrentSampleRate() const = 0;

  /// Get current buffer size
  ///
  /// @return Current buffer size in frames (e.g., 512)
  /// @note Thread-safe, can be called from any thread
  virtual uint32_t getCurrentBufferSize() const = 0;

  /// Register callback for device changes (hot-plug events)
  ///
  /// This callback is invoked when audio devices are added or removed
  /// from the system (e.g., USB audio interface plugged/unplugged).
  ///
  /// @param callback Function called when devices added/removed
  /// @note Callback is invoked on UI thread
  /// @note Only one callback can be registered at a time
  /// @note Pass nullptr to unregister callback
  virtual void setDeviceChangeCallback(std::function<void()> callback) = 0;

  /// Get the currently active audio driver instance
  ///
  /// @return Borrowed pointer to the active driver, or nullptr if no device active
  /// @note The pointer is thread-safe to obtain but is not a lifetime lease.
  ///       Synchronize its use with setActiveDevice() and do not retain it
  ///       across a possible replacement.
  virtual IAudioDriver* getActiveDriver() = 0;
};

/// Create driver manager instance
///
/// This factory function creates a platform-specific audio driver manager
/// that enumerates and manages audio devices on the current platform.
///
/// @return Unique pointer to driver manager instance
ORPHEUS_API std::unique_ptr<IAudioDriverManager> createAudioDriverManager();

} // namespace orpheus
