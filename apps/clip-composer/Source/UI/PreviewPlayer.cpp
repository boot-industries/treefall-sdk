// SPDX-License-Identifier: MIT

#include "PreviewPlayer.h"

//==============================================================================
PreviewPlayer::PreviewPlayer(AudioEngine* audioEngine, int buttonIndex)
    : m_audioEngine(audioEngine), m_buttonIndex(buttonIndex) {
  if (!m_audioEngine) {
    DBG("PreviewPlayer: WARNING - AudioEngine is nullptr!");
    return;
  }

  if (m_buttonIndex < 0 || m_buttonIndex >= 48) {
    DBG("PreviewPlayer: WARNING - Invalid button index: " << m_buttonIndex);
    return;
  }

  // Read file metadata from main grid clip (already loaded by MainComponent)
  auto metadata = m_audioEngine->getClipMetadata(m_buttonIndex);
  if (metadata.has_value()) {
    m_sampleRate = static_cast<int>(metadata->sample_rate);
    m_numChannels = static_cast<int>(metadata->num_channels);
    m_totalSamples = static_cast<int64_t>(metadata->duration_samples);

    DBG("PreviewPlayer: Initialized for button " << m_buttonIndex << " (" << m_sampleRate << " Hz, "
                                                 << m_numChannels << " ch, " << m_totalSamples
                                                 << " samples)");
  } else {
    DBG("PreviewPlayer: WARNING - No metadata available for button " << m_buttonIndex);
  }

  // Initialize trim points to full file
  m_trimInSamples = 0;
  m_trimOutSamples = m_totalSamples;
}

PreviewPlayer::~PreviewPlayer() {
  // Clear callbacks FIRST to prevent use-after-free
  onPlaybackStopped = nullptr;
  onPositionChanged = nullptr;

  // Stop position timer
  stopPositionTimer();

  DBG("PreviewPlayer: Destroyed (button " << m_buttonIndex << ")");
}

//==============================================================================
void PreviewPlayer::setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples) {
  DBG("PreviewPlayer::setTrimPoints() CALLED - IN: " << trimInSamples << ", OUT: " << trimOutSamples
                                                     << ", buttonIndex: " << m_buttonIndex);

  // Only clamp to file boundaries if metadata is loaded (m_totalSamples > 0)
  if (m_totalSamples > 0) {
    trimInSamples = std::clamp(trimInSamples, int64_t(0), m_totalSamples);
    trimOutSamples = std::clamp(trimOutSamples, int64_t(0), m_totalSamples);
  }

  m_trimInSamples = trimInSamples;
  m_trimOutSamples = trimOutSamples;

  // Update main grid clip metadata in AudioEngine (applies to LIVE playback)
  if (m_audioEngine) {
    bool updated = m_audioEngine->updateClipMetadata(m_buttonIndex, trimInSamples, trimOutSamples,
                                                     m_fadeInSeconds, m_fadeOutSeconds,
                                                     m_fadeInCurve, m_fadeOutCurve);

    if (updated) {
      DBG("PreviewPlayer: Updated main grid clip metadata (button " << m_buttonIndex << ")");
    } else {
      DBG("PreviewPlayer: WARNING - Failed to update main grid clip metadata (button "
          << m_buttonIndex << ")");
    }
  }

  DBG("PreviewPlayer: Trim points set to [" << trimInSamples << ", " << trimOutSamples << "]");
}

void PreviewPlayer::setLoopEnabled(bool shouldLoop) {
  m_loopEnabled = shouldLoop;

  // Update main grid clip loop mode in AudioEngine (applies to LIVE playback)
  if (m_audioEngine) {
    m_audioEngine->setClipLoopMode(m_buttonIndex, shouldLoop);
  }

  DBG("PreviewPlayer: Loop " << (shouldLoop ? "enabled" : "disabled") << " (button "
                             << m_buttonIndex << ")");
}

void PreviewPlayer::setFades(float fadeInSeconds, float fadeOutSeconds,
                             const juce::String& fadeInCurve, const juce::String& fadeOutCurve) {
  m_fadeInSeconds = fadeInSeconds;
  m_fadeOutSeconds = fadeOutSeconds;
  m_fadeInCurve = fadeInCurve;
  m_fadeOutCurve = fadeOutCurve;

  // Update main grid clip metadata in AudioEngine (applies to LIVE playback)
  if (m_audioEngine) {
    m_audioEngine->updateClipMetadata(m_buttonIndex, m_trimInSamples, m_trimOutSamples,
                                      fadeInSeconds, fadeOutSeconds, fadeInCurve, fadeOutCurve);
  }

  DBG("PreviewPlayer: Fades set to IN=" << fadeInSeconds << "s, OUT=" << fadeOutSeconds
                                        << "s (button " << m_buttonIndex << ")");
}

//==============================================================================
void PreviewPlayer::play() {
  if (!m_audioEngine) {
    DBG("PreviewPlayer: Cannot play - AudioEngine is nullptr");
    return;
  }

  // Start main grid clip (if already playing, SDK will handle seamlessly)
  bool started = m_audioEngine->startClip(m_buttonIndex);
  if (started) {
    startPositionTimer(); // Start polling position for playhead updates
    DBG("PreviewPlayer: Started main grid clip (button " << m_buttonIndex << ")");

    // Notify MainComponent to sync grid button visual state
    if (onPlayStateChanged) {
      onPlayStateChanged(true);
    }
  } else {
    DBG("PreviewPlayer: Failed to start main grid clip (button " << m_buttonIndex << ")");
  }
}

void PreviewPlayer::stop() {
  stopPositionTimer(); // Stop polling position

  if (m_audioEngine) {
    m_audioEngine->stopClip(m_buttonIndex);
  }

  // Notify UI that playback stopped
  if (onPlaybackStopped) {
    onPlaybackStopped();
  }

  // Notify MainComponent to sync grid button visual state
  if (onPlayStateChanged) {
    onPlayStateChanged(false);
  }

  DBG("PreviewPlayer: Stopped main grid clip (button " << m_buttonIndex << ")");
}

void PreviewPlayer::jumpTo(int64_t samplePosition) {
  // Clamp to trim range (load atomic values)
  int64_t trimIn = m_trimInSamples.load();
  int64_t trimOut = m_trimOutSamples.load();
  samplePosition = std::clamp(samplePosition, trimIn, trimOut);

  if (!m_audioEngine) {
    DBG("PreviewPlayer: Cannot jump - AudioEngine is nullptr");
    return;
  }

  // Click-to-jog: Use SDK's seekClip() for gap-free, sample-accurate seeking
  // This is SINGLE COMMAND per action (fixes transport spam issue)
  bool wasPlaying = m_audioEngine->isClipPlaying(m_buttonIndex);

  // Seek to target position (works whether playing or stopped)
  bool seeked = m_audioEngine->seekClip(m_buttonIndex, samplePosition);

  if (seeked) {
    // If not already playing, start playback from seeked position
    if (!wasPlaying) {
      m_audioEngine->startClip(m_buttonIndex);
    }

    startPositionTimer(); // Start polling position for playhead updates
    DBG("PreviewPlayer: Jogged to sample " << samplePosition << " (button " << m_buttonIndex
                                           << ", seamless gap-free seek)");
  } else {
    DBG("PreviewPlayer: Failed to jog to sample " << samplePosition << " (button " << m_buttonIndex
                                                  << ")");
  }
}

bool PreviewPlayer::isPlaying() const {
  if (!m_audioEngine)
    return false;

  return m_audioEngine->isClipPlaying(m_buttonIndex);
}

int64_t PreviewPlayer::getCurrentPosition() const {
  if (!m_audioEngine)
    return 0;

  // Use AudioEngine's sample-accurate position tracking
  return m_audioEngine->getClipPosition(m_buttonIndex);
}

//==============================================================================
void PreviewPlayer::startPositionTimer() {
  // Start JUCE timer at 75 FPS (broadcast standard, matches 75fps timecode)
  startTimer(13); // 75 FPS (13.33ms, rounds to 13ms)
}

void PreviewPlayer::stopPositionTimer() {
  // Stop JUCE timer
  stopTimer();
}

void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    // Timer is running but clip stopped - stop timer
    stopTimer();
    return;
  }

  // Query SDK for sample-accurate position (75 FPS polling)
  int64_t currentPos = getCurrentPosition();

  // DIAGNOSTIC: Warn if position escapes trim boundaries (SDK should prevent this)
  // DO NOT clamp - UI must always show actual SDK position (never lie to user)
  int64_t trimIn = m_trimInSamples.load();
  int64_t trimOut = m_trimOutSamples.load();

  if (currentPos < trimIn) {
    DBG("PreviewPlayer: WARNING - Position " << currentPos << " escaped below IN point " << trimIn
                                             << " (SDK should enforce boundaries)");
  }
  if (currentPos > trimOut) {
    DBG("PreviewPlayer: WARNING - Position " << currentPos << " escaped above OUT point " << trimOut
                                             << " (SDK should enforce boundaries)");
  }

  // Update UI playhead with ACTUAL position (even if outside bounds)
  // This allows user to see when SDK boundary enforcement fails
  if (onPositionChanged && currentPos >= 0) {
    onPositionChanged(currentPos);
  }
}
