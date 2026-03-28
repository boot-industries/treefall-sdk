// SPDX-License-Identifier: MIT

#include "PreviewPlayer.h"

#include <algorithm>

#include <juce_core/juce_core.h>

//==============================================================================
PreviewPlayer::PreviewPlayer(AudioEngine* audioEngine, int buttonIndex,
                             const juce::String& sourceFilePath)
    : m_audioEngine(audioEngine), m_buttonIndex(buttonIndex), m_sourceFilePath(sourceFilePath) {
  if (!m_audioEngine) {
    DBG("PreviewPlayer: WARNING - AudioEngine is nullptr!");
    return;
  }

  if (m_buttonIndex < 0 || m_buttonIndex >= AudioEngine::MAX_CLIP_BUTTONS) {
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

  if (m_sourceFilePath.isNotEmpty()) {
    setAuditionSource(m_sourceFilePath);
  }
}

PreviewPlayer::~PreviewPlayer() {
  // Clear callbacks FIRST to prevent use-after-free
  onPlaybackStopped = nullptr;
  onPositionChanged = nullptr;

  // Stop position timer
  stopPositionTimer();
  releaseAuditionBuss();

  DBG("PreviewPlayer: Destroyed (button " << m_buttonIndex << ")");
}

void PreviewPlayer::releaseAuditionBuss() {
  if (!m_audioEngine || m_auditionHandle == 0) {
    return;
  }

  if (m_audioEngine->isCueBussPlaying(m_auditionHandle)) {
    m_audioEngine->stopCueBuss(m_auditionHandle);
  }

  m_audioEngine->releaseCueBuss(m_auditionHandle);
  m_auditionHandle = 0;
}

void PreviewPlayer::setAuditionSource(const juce::String& sourceFilePath) {
  if (sourceFilePath == m_sourceFilePath && (sourceFilePath.isEmpty() || m_auditionHandle != 0)) {
    return;
  }

  releaseAuditionBuss();
  m_sourceFilePath = sourceFilePath;

  if (!m_audioEngine || m_sourceFilePath.isEmpty()) {
    DBG("PreviewPlayer: Audition source cleared - using main grid clip fallback");
    return;
  }

  juce::File sourceFile(m_sourceFilePath);
  if (!sourceFile.existsAsFile()) {
    DBG("PreviewPlayer: WARNING - Audition source file missing: " << m_sourceFilePath);
    return;
  }

  m_auditionHandle = m_audioEngine->allocateCueBuss(m_sourceFilePath);
  if (m_auditionHandle == 0) {
    DBG("PreviewPlayer: WARNING - Failed to allocate cue buss for audition source: "
        << m_sourceFilePath);
    return;
  }

  const auto trimOut =
      m_trimOutSamples.load() > 0 ? m_trimOutSamples.load() : m_totalSamples;
  if (trimOut > 0) {
    m_audioEngine->updateCueBussMetadata(m_auditionHandle, m_trimInSamples.load(), trimOut,
                                         m_fadeInSeconds, m_fadeOutSeconds, m_fadeInCurve,
                                         m_fadeOutCurve);
  }
  m_audioEngine->setCueBussLoop(m_auditionHandle, m_loopEnabled);

  DBG("PreviewPlayer: Dedicated audition cue buss allocated for " << m_sourceFilePath);
}

bool PreviewPlayer::isUsingDedicatedAuditionBuss() const {
  return m_auditionHandle != 0;
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

    if (m_auditionHandle != 0) {
      m_audioEngine->updateCueBussMetadata(m_auditionHandle, trimInSamples, trimOutSamples,
                                           m_fadeInSeconds, m_fadeOutSeconds, m_fadeInCurve,
                                           m_fadeOutCurve);
    }
  }

  DBG("PreviewPlayer: Trim points set to [" << trimInSamples << ", " << trimOutSamples << "]");
}

void PreviewPlayer::setLoopEnabled(bool shouldLoop) {
  m_loopEnabled = shouldLoop;

  // Update main grid clip loop mode in AudioEngine (applies to LIVE playback)
  if (m_audioEngine) {
    m_audioEngine->setClipLoopMode(m_buttonIndex, shouldLoop);
    if (m_auditionHandle != 0) {
      m_audioEngine->setCueBussLoop(m_auditionHandle, shouldLoop);
    }
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
    if (m_auditionHandle != 0) {
      m_audioEngine->updateCueBussMetadata(m_auditionHandle, m_trimInSamples.load(),
                                           m_trimOutSamples.load(), fadeInSeconds, fadeOutSeconds,
                                           fadeInCurve, fadeOutCurve);
    }
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

  const bool useAuditionBuss = isUsingDedicatedAuditionBuss();
  bool started = false;

  if (useAuditionBuss) {
    started = m_audioEngine->isCueBussPlaying(m_auditionHandle)
                  ? m_audioEngine->restartCueBuss(m_auditionHandle)
                  : m_audioEngine->startCueBuss(m_auditionHandle);
  } else {
    // Start main grid clip (if already playing, SDK will handle seamlessly)
    started = m_audioEngine->startClip(m_buttonIndex);
  }

  if (started) {
    m_lastPlayingState = true;
    startPositionTimer(); // Start polling position for playhead updates
    DBG("PreviewPlayer: Started " << (useAuditionBuss ? "audition cue buss" : "main grid clip")
                                  << " (button " << m_buttonIndex << ")");
  } else {
    DBG("PreviewPlayer: Failed to start "
        << (useAuditionBuss ? "audition cue buss" : "main grid clip") << " (button "
        << m_buttonIndex << ")");
  }
}

void PreviewPlayer::stop() {
  stopPositionTimer(); // Stop polling position

  if (m_audioEngine) {
    if (isUsingDedicatedAuditionBuss()) {
      m_audioEngine->stopCueBuss(m_auditionHandle);
    } else {
      m_audioEngine->stopClip(m_buttonIndex);
    }
  }

  m_lastPlayingState = false;

  // Notify UI that playback stopped
  if (onPlaybackStopped) {
    onPlaybackStopped();
  }

  DBG("PreviewPlayer: Stopped " << (isUsingDedicatedAuditionBuss() ? "audition cue buss"
                                                                  : "main grid clip")
                                 << " (button " << m_buttonIndex << ")");
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
  const bool useAuditionBuss = isUsingDedicatedAuditionBuss();
  bool wasPlaying = useAuditionBuss ? m_audioEngine->isCueBussPlaying(m_auditionHandle)
                                    : m_audioEngine->isClipPlaying(m_buttonIndex);

  if (wasPlaying) {
    // Clip is playing - seek to new position (gap-free, sample-accurate)
    bool seeked = useAuditionBuss ? m_audioEngine->seekCueBuss(m_auditionHandle, samplePosition)
                                  : m_audioEngine->seekClip(m_buttonIndex, samplePosition);

    if (seeked) {
      m_lastPlayingState = true;
      startPositionTimer(); // Start polling position for playhead updates
      DBG("PreviewPlayer: Jogged to sample " << samplePosition << " ("
                                             << (useAuditionBuss ? "audition cue buss" : "main grid clip")
                                             << ", seamless gap-free seek)");
    } else {
      DBG("PreviewPlayer: Failed to seek to sample " << samplePosition << " (button "
                                                     << m_buttonIndex << ")");
    }
  } else {
    // Clip is stopped - start playback from clicked position
    // Strategy: Start clip normally, then immediately seek to clicked position
    bool started = useAuditionBuss ? m_audioEngine->startCueBuss(m_auditionHandle)
                                   : m_audioEngine->startClip(m_buttonIndex);

    if (started) {
      // Seek to clicked position (gap-free, sample-accurate)
      bool seeked = useAuditionBuss ? m_audioEngine->seekCueBuss(m_auditionHandle, samplePosition)
                                    : m_audioEngine->seekClip(m_buttonIndex, samplePosition);

      startPositionTimer(); // Start polling position for playhead updates
      m_lastPlayingState = true;

      if (seeked) {
        DBG("PreviewPlayer: Started and seeked to sample " << samplePosition << " ("
                                                           << (useAuditionBuss ? "audition cue buss"
                                                                              : "main grid clip")
                                                           << ", click-to-start)");
      } else {
        DBG("PreviewPlayer: Started but seek failed - playing from IN ("
            << (useAuditionBuss ? "audition cue buss" : "main grid clip") << ")");
      }
    } else {
      DBG("PreviewPlayer: Failed to start clip from sample " << samplePosition << " ("
                                                             << (useAuditionBuss ? "audition cue buss"
                                                                                : "main grid clip")
                                                             << ")");
    }
  }
}

bool PreviewPlayer::isPlaying() const {
  return getPlaybackSnapshot().isPlaying;
}

int64_t PreviewPlayer::getCurrentPosition() const {
  return getPlaybackSnapshot().currentPositionSamples;
}

occ::ui::PreviewPlaybackUiSnapshot PreviewPlayer::getPlaybackSnapshot() const {
  occ::ui::PreviewPlaybackUiSnapshot snapshot;

  if (!m_audioEngine)
    return snapshot;

  if (isUsingDedicatedAuditionBuss()) {
    snapshot.isPlaying = m_audioEngine->isCueBussPlaying(m_auditionHandle);
    snapshot.currentPositionSamples = m_audioEngine->getCueBussPosition(m_auditionHandle);
  } else {
    snapshot.isPlaying = m_audioEngine->isClipPlaying(m_buttonIndex);
    snapshot.currentPositionSamples = m_audioEngine->getClipPosition(m_buttonIndex);
  }
  return snapshot;
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
  const auto snapshot = getPlaybackSnapshot();
  if (!snapshot.isPlaying) {
    const bool wasPlaying = m_lastPlayingState;
    m_lastPlayingState = false;
    if (wasPlaying) {
      stopPositionTimer();
      if (onPlaybackStopped) {
        onPlaybackStopped();
      }
    }
    return;
  }

  m_lastPlayingState = true;

  const int64_t currentPos = snapshot.currentPositionSamples;

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
