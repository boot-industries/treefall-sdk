// SPDX-License-Identifier: MIT

#pragma once

#include <juce_core/juce_core.h>

// Forward declare SDK types (will be included in .cpp)
namespace orpheus {
    class ITransportController;
    class IAudioFileReader;
    class IAudioDriver;
    class ITransportCallback;
    class IAudioCallback;
}

// Forward declare internal callback adapter
class AudioEngineCallback;

//==============================================================================
/**
 * AudioEngine - Integration layer between JUCE and Orpheus SDK
 *
 * Responsibilities:
 * - Manage SDK module lifecycles (ITransportController, IAudioFileReader, IAudioDriver)
 * - Adapt JUCE's AudioIODeviceCallback to SDK's IAudioCallback
 * - Process SDK callbacks and post to JUCE message thread
 * - Provide simplified API for UI components
 *
 * Threading:
 * - Created and controlled from JUCE message thread
 * - Owns audio thread via IAudioDriver
 * - Uses lock-free communication (SDK provides this)
 *
 * Usage:
 * @code
 * auto engine = std::make_unique<AudioEngine>();
 * engine->initialize(48000, 512);  // Sample rate, buffer size
 * engine->start();
 * engine->loadClip("/path/to/audio.wav", buttonIndex);
 * engine->triggerClip(buttonIndex);
 * @endcode
 */
class AudioEngine
{
public:
    //==============================================================================
    AudioEngine();
    ~AudioEngine();

    //==============================================================================
    /**
     * Initialize audio engine with specified configuration
     *
     * @param sampleRate Sample rate in Hz (typically 48000)
     * @param bufferSize Buffer size in samples (typically 512)
     * @return true if initialization succeeded
     *
     * Call this before start(). Creates SDK components but doesn't start audio yet.
     */
    bool initialize(uint32_t sampleRate, uint16_t bufferSize);

    /**
     * Start audio processing
     *
     * @return true if audio started successfully
     *
     * Starts the audio driver's callback thread. Must call initialize() first.
     */
    bool start();

    /**
     * Stop audio processing
     *
     * Stops the audio driver and waits for audio thread to exit.
     * Safe to call from UI thread.
     */
    void stop();

    /**
     * Check if audio is currently running
     *
     * @return true if audio callback is active
     */
    bool isRunning() const;

    //==============================================================================
    // Clip Management (to be implemented in Week 7-8)

    /**
     * Load an audio clip from file
     *
     * @param filePath Absolute path to audio file (WAV/AIFF/FLAC)
     * @param buttonIndex Button index (0-959) to assign clip to
     * @return true if clip loaded successfully
     *
     * Loads clip metadata and stores reference. File is read on-demand in audio thread.
     */
    bool loadClip(const juce::String& filePath, int buttonIndex);

    /**
     * Trigger a clip to start playing
     *
     * @param buttonIndex Button index (0-959) of clip to play
     * @return true if command was queued successfully
     *
     * Sends lock-free command to audio thread. Returns immediately.
     */
    bool triggerClip(int buttonIndex);

    /**
     * Stop a currently playing clip
     *
     * @param buttonIndex Button index (0-959) of clip to stop
     * @return true if command was queued successfully
     *
     * Initiates 10ms fade-out. Clip removed from active list after fade completes.
     */
    bool stopClip(int buttonIndex);

    /**
     * Stop all currently playing clips (panic button)
     *
     * @return true if command was queued successfully
     */
    bool stopAllClips();

    //==============================================================================
    // Status Queries

    /**
     * Get current transport position
     *
     * @return Sample count since audio started
     *
     * Thread-safe atomic read. Can be called from UI thread.
     */
    int64_t getCurrentPosition() const;

    /**
     * Check if a clip is currently playing
     *
     * @param buttonIndex Button index (0-959) to query
     * @return true if clip is in Playing or Stopping state
     *
     * Note: May be 1 audio buffer stale (~10ms @ 48kHz/512)
     */
    bool isClipPlaying(int buttonIndex) const;

    /**
     * Get current CPU usage percentage
     *
     * @return CPU usage 0.0-100.0 (requires IPerformanceMonitor in Month 4-5)
     *
     * Returns 0.0 if performance monitor not available.
     */
    float getCpuUsage() const;

private:
    //==============================================================================
    // SDK Components (created in initialize())
    std::unique_ptr<orpheus::ITransportController> m_transportController;
    std::unique_ptr<orpheus::IAudioDriver> m_audioDriver;
    std::unique_ptr<AudioEngineCallback> m_audioCallback;
    // std::unique_ptr<orpheus::IRoutingMatrix> m_routingMatrix;  // Month 3-4
    // std::unique_ptr<orpheus::IPerformanceMonitor> m_perfMonitor;  // Month 4-5

    // Configuration
    uint32_t m_sampleRate = 0;
    uint16_t m_bufferSize = 0;
    bool m_initialized = false;

    // Clip metadata storage (buttonIndex → ClipHandle mapping)
    // std::unordered_map<int, ClipMetadata> m_clips;  // To be implemented

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
