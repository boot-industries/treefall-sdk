// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/transport_controller.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/routing_matrix.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace orpheus {

// Forward declarations
namespace core {
class SessionGraph;
} // namespace core

/// Command for audio thread (lock-free queue)
struct TransportCommand {
  enum class Type : uint8_t { Start, Stop, StopAll, StopGroup };

  Type type;
  ClipHandle handle;
  uint8_t groupIndex; // For StopGroup command
};

/// Active clip state (in audio thread)
struct ActiveClip {
  ClipHandle handle;
  int64_t startSample;    // When clip started playing
  int64_t currentSample;  // Current position within clip audio
  int64_t trimInSamples;  // Trim IN point (from metadata)
  int64_t trimOutSamples; // Trim OUT point (from metadata)
  float fadeOutGain;      // 1.0 = normal, 0.0 = fully faded
  bool isStopping;        // true if fade-out in progress
  IAudioFileReader* audioReader; // Cached pointer to audio file reader (owned by m_audioFiles)
  uint16_t numChannels;   // Number of channels in audio file
};

/// Transport controller implementation
class TransportController : public ITransportController {
public:
  TransportController(core::SessionGraph* sessionGraph, uint32_t sampleRate);
  ~TransportController() override;

  // ITransportController interface
  SessionGraphError startClip(ClipHandle handle) override;
  SessionGraphError stopClip(ClipHandle handle) override;
  SessionGraphError stopAllClips() override;
  SessionGraphError stopAllInGroup(uint8_t groupIndex) override;
  PlaybackState getClipState(ClipHandle handle) const override;
  bool isClipPlaying(ClipHandle handle) const override;
  TransportPosition getCurrentPosition() const override;
  void setCallback(ITransportCallback* callback) override;

  /// Process audio (called from audio thread)
  /// @param outputBuffers Output buffers (one per channel)
  /// @param numChannels Number of output channels
  /// @param numFrames Number of frames to process
  void processAudio(float** outputBuffers, size_t numChannels, size_t numFrames);

  /// Process callbacks on UI thread
  /// Must be called periodically from UI thread to dispatch transport events
  void processCallbacks();

  /// Register audio file for a clip (UI thread)
  /// @param handle Clip handle
  /// @param file_path Path to audio file
  /// @return Error code
  SessionGraphError registerClipAudio(ClipHandle handle, const std::string& file_path);

private:
  /// Process pending commands from UI thread
  void processCommands();

  /// Find active clip by handle
  /// @return Pointer to active clip, or nullptr if not found
  ActiveClip* findActiveClip(ClipHandle handle);

  /// Add a clip to active list (audio thread only)
  void addActiveClip(ClipHandle handle);

  /// Remove a clip from active list (audio thread only)
  void removeActiveClip(ClipHandle handle);

  /// Post callback to UI thread
  void postCallback(std::function<void()> callback);

  // Configuration
  core::SessionGraph* m_sessionGraph;
  uint32_t m_sampleRate;
  ITransportCallback* m_callback; // User-provided callback

  // Lock-free command queue (UI → Audio thread)
  static constexpr size_t MAX_COMMANDS = 256;
  std::array<TransportCommand, MAX_COMMANDS> m_commands;
  std::atomic<size_t> m_commandWriteIndex{0};
  std::atomic<size_t> m_commandReadIndex{0};

  // Active clips (audio thread only, no locks needed)
  static constexpr size_t MAX_ACTIVE_CLIPS = 32;
  std::array<ActiveClip, MAX_ACTIVE_CLIPS> m_activeClips;
  size_t m_activeClipCount{0};

  // Transport position (audio thread writes, UI thread reads)
  std::atomic<int64_t> m_currentSample{0};

  // Callback queue (Audio → UI thread)
  std::mutex m_callbackMutex;
  std::queue<std::function<void()>> m_callbackQueue;

  // Fade parameters
  static constexpr float FADE_OUT_DURATION_MS = 10.0f;
  size_t m_fadeOutSamples; // Calculated from sample rate

  // Audio file registry (UI thread access, mutex protected)
  struct AudioFileEntry {
    std::unique_ptr<orpheus::IAudioFileReader> reader;
    AudioFileMetadata metadata;
  };
  std::mutex m_audioFilesMutex;
  std::unordered_map<ClipHandle, AudioFileEntry> m_audioFiles;

  // Pre-allocated audio read buffer (audio thread only)
  // Large enough for max frames * max channels (e.g., 2048 frames * 8 channels)
  static constexpr size_t MAX_READ_BUFFER_SIZE = 2048 * 8;
  std::vector<float> m_audioReadBuffer;

  // Routing matrix for final mix (audio thread processes, UI thread configures)
  std::unique_ptr<IRoutingMatrix> m_routingMatrix;

  // Per-clip channel buffers (audio thread only, pre-allocated)
  // Each active clip gets its own channel buffer for routing
  static constexpr size_t MAX_BUFFER_FRAMES = 2048;
  std::vector<std::vector<float>> m_clipChannelBuffers; // [MAX_ACTIVE_CLIPS][MAX_BUFFER_FRAMES]
  std::vector<float*> m_clipChannelPointers; // Pointers for processRouting()
};

} // namespace orpheus
