// SPDX-License-Identifier: MIT

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <orpheus/transport_controller.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_driver.h>
#include <memory>
#include <functional>
#include <optional>

// Forward declare concrete class for extended API
namespace orpheus {
class TransportController;
}

/**
 * AudioEngine - Integrates Orpheus SDK with Clip Composer
 *
 * This class serves as the bridge between JUCE UI and Orpheus SDK's
 * real-time audio infrastructure.
 *
 * Architecture:
 * - Owns ITransportController, IAudioDriver
 * - Manages clip loading via IAudioFileReader
 * - Posts callbacks to UI thread via juce::MessageManager
 * - Thread-safe command posting from UI
 *
 * Threading Model:
 * - Construction/destruction: UI thread
 * - startClip(), stopClip(), etc.: UI thread (lock-free)
 * - Audio callback: Audio thread (real-time safe)
 * - SDK callbacks: Posted to UI thread via MessageManager
 */
class AudioEngine : public orpheus::ITransportCallback,
                    public orpheus::IAudioCallback
{
public:
    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;

    //==============================================================================
    // Initialization

    /// Initialize audio engine with sample rate
    /// @param sampleRate Sample rate in Hz (e.g., 48000)
    /// @return true on success
    bool initialize(uint32_t sampleRate = 48000);

    /// Start audio processing
    /// @return true on success
    bool start();

    /// Stop audio processing
    void stop();

    /// Check if engine is running
    bool isRunning() const;

    //==============================================================================
    // Clip Management (UI Thread)

    /// Load audio file to clip slot (registers with transport controller)
    /// @param buttonIndex Button index (0-47 for MVP)
    /// @param filePath Absolute path to audio file
    /// @return true on success
    bool loadClip(int buttonIndex, const juce::String& filePath);

    /// Get audio file metadata for loaded clip
    /// @param buttonIndex Button index
    /// @return Metadata if clip loaded, empty optional otherwise
    std::optional<orpheus::AudioFileMetadata> getClipMetadata(int buttonIndex) const;

    /// Unload clip from slot
    /// @param buttonIndex Button index
    void unloadClip(int buttonIndex);

    //==============================================================================
    // Playback Control (UI Thread, Lock-Free)

    /// Start playing a clip
    /// @param buttonIndex Button index
    /// @return true if command posted successfully
    bool startClip(int buttonIndex);

    /// Stop playing a clip (with fade-out)
    /// @param buttonIndex Button index
    /// @return true if command posted successfully
    bool stopClip(int buttonIndex);

    /// Stop all playing clips
    void stopAllClips();

    /// PANIC - immediate mute (no fade-out)
    void panicStop();

    //==============================================================================
    // State Queries (Any Thread)

    /// Check if clip is playing
    /// @param buttonIndex Button index
    /// @return true if playing
    bool isClipPlaying(int buttonIndex) const;

    /// Get clip playback state
    /// @param buttonIndex Button index
    /// @return Playback state
    orpheus::PlaybackState getClipState(int buttonIndex) const;

    /// Get current transport position
    /// @return Transport position (samples, seconds, beats)
    orpheus::TransportPosition getCurrentPosition() const;

    //==============================================================================
    // Callbacks from UI

    /// Set callback for clip events
    std::function<void(int buttonIndex, orpheus::PlaybackState state)> onClipStateChanged;

    /// Set callback for buffer underruns (audio dropouts)
    std::function<void()> onBufferUnderrunDetected;

    //==============================================================================
    // ITransportCallback overrides (Posted to UI Thread)
    void onClipStarted(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
    void onClipStopped(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
    void onClipLooped(orpheus::ClipHandle handle, orpheus::TransportPosition position) override;
    void onBufferUnderrun(orpheus::TransportPosition position) override;

    //==============================================================================
    // IAudioCallback override (Audio Thread, Real-Time Safe)
    void processAudio(const float** input_buffers, float** output_buffers,
                      size_t num_channels, size_t num_frames) override;

private:
    //==============================================================================
    // Helper methods
    orpheus::ClipHandle getClipHandle(int buttonIndex) const;
    int getButtonIndexFromHandle(orpheus::ClipHandle handle) const;

    //==============================================================================
    // SDK Components
    // Note: Using concrete class for extended API (registerClipAudio, processAudio, etc.)
    std::unique_ptr<orpheus::TransportController> m_transportController;
    std::unique_ptr<orpheus::IAudioDriver> m_audioDriver;

    // Clip handle mapping (buttonIndex → ClipHandle)
    std::array<orpheus::ClipHandle, 48> m_clipHandles; // MVP: 48 buttons

    // Clip metadata cache (for UI queries)
    std::array<std::optional<orpheus::AudioFileMetadata>, 48> m_clipMetadata;

    // Engine state
    uint32_t m_sampleRate = 48000;
    bool m_initialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
