// SPDX-License-Identifier: MIT

#include "ClipEditDialog.h"

//==============================================================================
ClipEditDialog::ClipEditDialog(AudioEngine* audioEngine, int buttonIndex)
    : m_audioEngine(audioEngine), m_buttonIndex(buttonIndex) {
  // Create preview player with AudioEngine reference and buttonIndex (controls main grid clip)
  m_previewPlayer = std::make_unique<PreviewPlayer>(m_audioEngine, m_buttonIndex);

  // Build Phase 1 UI (basic metadata)
  buildPhase1UI();

  // Build Phase 2 UI (In/Out points)
  buildPhase2UI();

  // Build Phase 3 UI (Fade times)
  buildPhase3UI();

  setSize(700, 800); // Expanded for all phases
}

ClipEditDialog::~ClipEditDialog() {
  // CRITICAL: Clear all callbacks BEFORE PreviewPlayer is destroyed
  // Prevents heap-use-after-free when audio thread tries to access destroyed dialog
  if (m_previewPlayer) {
    m_previewPlayer->onPositionChanged = nullptr;
    m_previewPlayer->onPlaybackStopped = nullptr;
    // NOTE: DO NOT stop playback here - Edit Dialog is just a view, not a controller
    // Main grid clip should continue playing when dialog closes
  }
  // PreviewPlayer destructor cleanup is safe (no Cue Buss to release)
}

//==============================================================================
void ClipEditDialog::setClipMetadata(const ClipMetadata& metadata) {
  m_metadata = metadata;

  // Update UI controls
  if (m_nameEditor)
    m_nameEditor->setText(m_metadata.displayName, false);

  if (m_filePathEditor)
    m_filePathEditor->setText(m_metadata.filePath, false);

  if (m_groupComboBox)
    m_groupComboBox->setSelectedId(m_metadata.clipGroup + 1, juce::dontSendNotification);

  // Set color swatch picker based on metadata color
  if (m_colorSwatchPicker) {
    m_colorSwatchPicker->setSelectedColor(m_metadata.color);
  }

  // Update trim info label
  updateTrimInfoLabel();

  // Update file info panel (SpotOn-style)
  if (m_fileInfoPanel) {
    double durationSeconds =
        static_cast<double>(m_metadata.durationSamples) / m_metadata.sampleRate;
    juce::String formatName =
        m_metadata.filePath.fromLastOccurrenceOf(".", false, true).toUpperCase();
    if (formatName.isEmpty())
      formatName = "Unknown";

    juce::String infoText = juce::String::formatted(
        "  Channels: %d  |  Sample Rate: %d Hz  |  Duration: %.2fs  |  Format: %s",
        m_metadata.numChannels, m_metadata.sampleRate, durationSeconds, formatName.toRawUTF8());
    m_fileInfoPanel->setText(infoText, juce::dontSendNotification);
  }

  // CRITICAL: Check if clip is already playing BEFORE loading waveform and preview player
  // This allows seamless continuity when Edit Dialog opens during playback
  bool wasAlreadyPlaying = false;
  if (m_previewPlayer) {
    wasAlreadyPlaying = m_previewPlayer->isPlaying();
    DBG("ClipEditDialog::setClipMetadata() - Clip was "
        << (wasAlreadyPlaying ? "ALREADY PLAYING" : "stopped") << " when dialog opened");
  }

  // Load waveform display and preview player
  if (m_waveformDisplay && m_metadata.filePath.isNotEmpty()) {
    juce::File audioFile(m_metadata.filePath);
    if (audioFile.existsAsFile()) {
      m_waveformDisplay->setAudioFile(audioFile);
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples > 0
                                                                     ? m_metadata.trimOutSamples
                                                                     : m_metadata.durationSamples);

      // Configure preview player (metadata already loaded from main grid clip via AudioEngine)
      // NOTE: setTrimPoints/setFades DO NOT stop/restart playback - they just update metadata
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples > 0
                                                                     ? m_metadata.trimOutSamples
                                                                     : m_metadata.durationSamples);
        m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                  m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
      }
    }
  }

  // Sync loop button to metadata
  if (m_loopButton) {
    m_loopButton->setToggleState(m_metadata.loopEnabled, juce::dontSendNotification);
  }

  // Sync preview player loop state
  if (m_previewPlayer) {
    m_previewPlayer->setLoopEnabled(m_metadata.loopEnabled);
  }

  // CRITICAL: If clip was already playing when dialog opened, start position timer
  // This ensures playhead continues updating in Edit Dialog without interrupting playback
  if (wasAlreadyPlaying && m_previewPlayer) {
    m_previewPlayer->startPositionTimer();
    DBG("ClipEditDialog::setClipMetadata() - Started position timer (clip was already playing)");
  }

  // Sync Stop Others button to metadata
  if (m_stopOthersButton) {
    m_stopOthersButton->setToggleState(m_metadata.stopOthersEnabled, juce::dontSendNotification);
  }

  // Phase 3: Initialize fade combos
  if (m_fadeInCombo) {
    // Map fade time to combo ID (0.0s, 0.1s-1.0s in 0.1s increments, 1.2s, 1.6s, 2.0s, 2.4s, 3.0s)
    if (m_metadata.fadeInSeconds <= 0.05)
      m_fadeInCombo->setSelectedId(1); // 0.0s
    else if (m_metadata.fadeInSeconds <= 0.15)
      m_fadeInCombo->setSelectedId(2); // 0.1s
    else if (m_metadata.fadeInSeconds <= 0.25)
      m_fadeInCombo->setSelectedId(3); // 0.2s
    else if (m_metadata.fadeInSeconds <= 0.35)
      m_fadeInCombo->setSelectedId(4); // 0.3s
    else if (m_metadata.fadeInSeconds <= 0.45)
      m_fadeInCombo->setSelectedId(5); // 0.4s
    else if (m_metadata.fadeInSeconds <= 0.55)
      m_fadeInCombo->setSelectedId(6); // 0.5s
    else if (m_metadata.fadeInSeconds <= 0.65)
      m_fadeInCombo->setSelectedId(7); // 0.6s
    else if (m_metadata.fadeInSeconds <= 0.75)
      m_fadeInCombo->setSelectedId(8); // 0.7s
    else if (m_metadata.fadeInSeconds <= 0.85)
      m_fadeInCombo->setSelectedId(9); // 0.8s
    else if (m_metadata.fadeInSeconds <= 0.95)
      m_fadeInCombo->setSelectedId(10); // 0.9s
    else if (m_metadata.fadeInSeconds <= 1.1)
      m_fadeInCombo->setSelectedId(11); // 1.0s
    else if (m_metadata.fadeInSeconds <= 1.4)
      m_fadeInCombo->setSelectedId(12); // 1.2s
    else if (m_metadata.fadeInSeconds <= 1.8)
      m_fadeInCombo->setSelectedId(13); // 1.6s
    else if (m_metadata.fadeInSeconds <= 2.2)
      m_fadeInCombo->setSelectedId(14); // 2.0s
    else if (m_metadata.fadeInSeconds <= 2.7)
      m_fadeInCombo->setSelectedId(15); // 2.4s
    else
      m_fadeInCombo->setSelectedId(16); // 3.0s
  }
  if (m_fadeOutCombo) {
    // Map fade time to combo ID (0.0s, 0.1s-1.0s in 0.1s increments, 1.2s, 1.6s, 2.0s, 2.4s, 3.0s)
    if (m_metadata.fadeOutSeconds <= 0.05)
      m_fadeOutCombo->setSelectedId(1); // 0.0s
    else if (m_metadata.fadeOutSeconds <= 0.15)
      m_fadeOutCombo->setSelectedId(2); // 0.1s
    else if (m_metadata.fadeOutSeconds <= 0.25)
      m_fadeOutCombo->setSelectedId(3); // 0.2s
    else if (m_metadata.fadeOutSeconds <= 0.35)
      m_fadeOutCombo->setSelectedId(4); // 0.3s
    else if (m_metadata.fadeOutSeconds <= 0.45)
      m_fadeOutCombo->setSelectedId(5); // 0.4s
    else if (m_metadata.fadeOutSeconds <= 0.55)
      m_fadeOutCombo->setSelectedId(6); // 0.5s
    else if (m_metadata.fadeOutSeconds <= 0.65)
      m_fadeOutCombo->setSelectedId(7); // 0.6s
    else if (m_metadata.fadeOutSeconds <= 0.75)
      m_fadeOutCombo->setSelectedId(8); // 0.7s
    else if (m_metadata.fadeOutSeconds <= 0.85)
      m_fadeOutCombo->setSelectedId(9); // 0.8s
    else if (m_metadata.fadeOutSeconds <= 0.95)
      m_fadeOutCombo->setSelectedId(10); // 0.9s
    else if (m_metadata.fadeOutSeconds <= 1.1)
      m_fadeOutCombo->setSelectedId(11); // 1.0s
    else if (m_metadata.fadeOutSeconds <= 1.4)
      m_fadeOutCombo->setSelectedId(12); // 1.2s
    else if (m_metadata.fadeOutSeconds <= 1.8)
      m_fadeOutCombo->setSelectedId(13); // 1.6s
    else if (m_metadata.fadeOutSeconds <= 2.2)
      m_fadeOutCombo->setSelectedId(14); // 2.0s
    else if (m_metadata.fadeOutSeconds <= 2.7)
      m_fadeOutCombo->setSelectedId(15); // 2.4s
    else
      m_fadeOutCombo->setSelectedId(16); // 3.0s
  }

  // Set fade curve combos
  if (m_fadeInCurveCombo) {
    if (m_metadata.fadeInCurve == "Linear")
      m_fadeInCurveCombo->setSelectedId(1, juce::dontSendNotification);
    else if (m_metadata.fadeInCurve == "EqualPower")
      m_fadeInCurveCombo->setSelectedId(2, juce::dontSendNotification);
    else if (m_metadata.fadeInCurve == "Exponential")
      m_fadeInCurveCombo->setSelectedId(3, juce::dontSendNotification);
  }

  if (m_fadeOutCurveCombo) {
    if (m_metadata.fadeOutCurve == "Linear")
      m_fadeOutCurveCombo->setSelectedId(1, juce::dontSendNotification);
    else if (m_metadata.fadeOutCurve == "EqualPower")
      m_fadeOutCurveCombo->setSelectedId(2, juce::dontSendNotification);
    else if (m_metadata.fadeOutCurve == "Exponential")
      m_fadeOutCurveCombo->setSelectedId(3, juce::dontSendNotification);
  }

  // Feature 5: Initialize gain slider
  if (m_gainSlider) {
    m_gainSlider->setValue(m_metadata.gainDb, juce::dontSendNotification);
    m_gainValueLabel->setText(juce::String(m_metadata.gainDb, 1) + " dB",
                              juce::dontSendNotification);
  }
}

juce::String ClipEditDialog::samplesToTimeString(int64_t samples, int sampleRate) {
  if (sampleRate <= 0)
    return "00:00:00.00";

  double totalSeconds = static_cast<double>(samples) / sampleRate;
  int hours = static_cast<int>(totalSeconds / 3600);
  int minutes = static_cast<int>((totalSeconds - hours * 3600) / 60);
  int seconds = static_cast<int>(totalSeconds) % 60;

  // Frames: 75fps (SpotOn standard)
  double fractionalSeconds = totalSeconds - static_cast<int>(totalSeconds);
  int frames = static_cast<int>(fractionalSeconds * 75.0);

  // Format as HH:MM:SS.FF
  return juce::String::formatted("%02d:%02d:%02d.%02d", hours, minutes, seconds, frames);
}

int64_t ClipEditDialog::timeStringToSamples(const juce::String& timeStr, int sampleRate) {
  // Parse HH:MM:SS.FF format (75fps)
  // Split by ':' first
  auto parts = juce::StringArray::fromTokens(timeStr, ":", "");
  if (parts.size() < 2)
    return 0;

  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int frames = 0;

  if (parts.size() == 3) {
    // HH:MM:SS.FF
    hours = parts[0].getIntValue();
    minutes = parts[1].getIntValue();
    // Split seconds and frames by '.'
    auto secParts = juce::StringArray::fromTokens(parts[2], ".", "");
    seconds = secParts[0].getIntValue();
    if (secParts.size() > 1)
      frames = secParts[1].getIntValue();
  } else if (parts.size() == 2) {
    // MM:SS.FF (no hours)
    minutes = parts[0].getIntValue();
    auto secParts = juce::StringArray::fromTokens(parts[1], ".", "");
    seconds = secParts[0].getIntValue();
    if (secParts.size() > 1)
      frames = secParts[1].getIntValue();
  }

  // Convert to total seconds (75fps)
  double totalSeconds = hours * 3600.0 + minutes * 60.0 + seconds + (frames / 75.0);
  return static_cast<int64_t>(totalSeconds * sampleRate);
}

void ClipEditDialog::updateTrimInfoLabel() {
  if (!m_trimInfoLabel)
    return;

  int64_t trimmedSamples = m_metadata.trimOutSamples - m_metadata.trimInSamples;

  if (trimmedSamples < 0) {
    m_trimInfoLabel->setText("Invalid trim range", juce::dontSendNotification);
    return;
  }

  if (m_metadata.sampleRate > 0) {
    // Use HH:MM:SS.FF format for duration display (SpotOn standard)
    juce::String durationText =
        "Duration: " + samplesToTimeString(trimmedSamples, m_metadata.sampleRate);
    m_trimInfoLabel->setText(durationText, juce::dontSendNotification);
  }

  // Update time editors
  if (m_trimInTimeEditor) {
    m_trimInTimeEditor->setText(
        samplesToTimeString(m_metadata.trimInSamples, m_metadata.sampleRate), false);
  }
  if (m_trimOutTimeEditor) {
    m_trimOutTimeEditor->setText(
        samplesToTimeString(m_metadata.trimOutSamples, m_metadata.sampleRate), false);
  }
}

void ClipEditDialog::updateZoomLabel() {
  if (!m_zoomLabel || !m_waveformDisplay)
    return;

  int zoomLevel = m_waveformDisplay->getZoomLevel();
  switch (zoomLevel) {
  case 0:
    m_zoomLabel->setText("1x", juce::dontSendNotification);
    break;
  case 1:
    m_zoomLabel->setText("2x", juce::dontSendNotification);
    break;
  case 2:
    m_zoomLabel->setText("4x", juce::dontSendNotification);
    break;
  case 3:
    m_zoomLabel->setText("8x", juce::dontSendNotification);
    break;
  case 4:
    m_zoomLabel->setText("16x", juce::dontSendNotification);
    break;
  }
}

void ClipEditDialog::enforceOutPointEditLaw() {
  // EDIT LAW: If OUT point is set to <= playhead position, jump playhead to IN and restart
  // This prevents playback from escaping the OUT boundary
  // The interruption is acceptable in an edit scenario - we guarantee playback never occurs >= OUT

  if (!m_previewPlayer || !m_previewPlayer->isPlaying())
    return; // Only enforce during playback

  int64_t currentPos = m_previewPlayer->getCurrentPosition();

  if (currentPos >= m_metadata.trimOutSamples) {
    // Playhead is at or past new OUT point - jump to IN and restart
    m_previewPlayer->play(); // Restarts from IN point
    DBG("ClipEditDialog: OUT point edit law enforced - playhead was >= OUT ("
        << currentPos << " >= " << m_metadata.trimOutSamples << "), jumped to IN and restarted");
  }
}

void ClipEditDialog::restartPlayback() {
  // Restart playback from current IN point (used for edit law enforcement)
  if (m_previewPlayer && m_previewPlayer->isPlaying()) {
    m_previewPlayer->play(); // play() restarts from IN point
  }
}

//==============================================================================
void ClipEditDialog::buildPhase1UI() {
  // Clip Name
  m_nameLabel = std::make_unique<juce::Label>("nameLabel", "Clip Name:");
  m_nameLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_nameLabel.get());

  m_nameEditor = std::make_unique<juce::TextEditor>();
  m_nameEditor->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::plain));
  m_nameEditor->setJustification(juce::Justification::centredLeft); // Vertically center text
  m_nameEditor->setMultiLine(false);                                // Single line only
  m_nameEditor->setScrollBarThickness(0);                           // No scrollbar
  m_nameEditor->setScrollToShowCursor(true);                        // Scroll to keep cursor visible
  m_nameEditor->onTextChange = [this]() { m_metadata.displayName = m_nameEditor->getText(); };
  m_nameEditor->setReturnKeyStartsNewLine(false); // Enter should NOT insert newline
  m_nameEditor->onReturnKey = [this]() {
    // Enter key = Save & Close (trigger OK button)
    if (m_okButton) {
      m_okButton->triggerClick();
    }
  };
  addAndMakeVisible(m_nameEditor.get());

  // File Path (read-only)
  m_filePathLabel = std::make_unique<juce::Label>("filePathLabel", "File Path:");
  m_filePathLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_filePathLabel.get());

  m_filePathEditor = std::make_unique<juce::TextEditor>();
  m_filePathEditor->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_filePathEditor->setReadOnly(true);
  m_filePathEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
  addAndMakeVisible(m_filePathEditor.get());

  // Color
  m_colorLabel = std::make_unique<juce::Label>("colorLabel", "Color:");
  m_colorLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_colorLabel.get());

  // Ableton-style color swatch picker
  m_colorSwatchPicker = std::make_unique<ColorSwatchPicker>();
  m_colorSwatchPicker->onColorSelected = [this](const juce::Colour& color) {
    m_metadata.color = color;

    // Real-time color update: Notify MainComponent to repaint button immediately (75fps)
    if (onColorChanged) {
      onColorChanged(color);
    }
  };
  addAndMakeVisible(m_colorSwatchPicker.get());

  // Clip Group
  m_groupLabel = std::make_unique<juce::Label>("groupLabel", "Group:");
  m_groupLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_groupLabel.get());

  m_groupComboBox = std::make_unique<juce::ComboBox>();
  m_groupComboBox->addItem("Group 1 (Blue)", 1);
  m_groupComboBox->addItem("Group 2 (Green)", 2);
  m_groupComboBox->addItem("Group 3 (Orange)", 3);
  m_groupComboBox->addItem("Group 4 (Red)", 4);
  m_groupComboBox->onChange = [this]() {
    m_metadata.clipGroup = m_groupComboBox->getSelectedId() - 1; // 0-3
  };
  addAndMakeVisible(m_groupComboBox.get());

  // Dialog buttons
  m_okButton = std::make_unique<juce::TextButton>("OK");
  m_okButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2ecc71)); // Green
  m_okButton->onClick = [this]() {
    if (onOkClicked)
      onOkClicked(m_metadata);
  };
  addAndMakeVisible(m_okButton.get());

  m_cancelButton = std::make_unique<juce::TextButton>("Cancel");
  m_cancelButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff95a5a6)); // Grey
  m_cancelButton->onClick = [this]() {
    if (onCancelClicked)
      onCancelClicked();
  };
  addAndMakeVisible(m_cancelButton.get());
}

void ClipEditDialog::buildPhase2UI() {
  // Waveform Display (real component)
  m_waveformDisplay = std::make_unique<WaveformDisplay>();
  addAndMakeVisible(m_waveformDisplay.get());

  // Preview Transport Controls (Professional icon-based buttons inspired by SpotOn and Merging
  // Ovation)

  // Skip to Start button (◄◄) - consistent padding and sizing
  m_skipToStartButton =
      std::make_unique<juce::DrawableButton>("SkipToStart", juce::DrawableButton::ImageFitted);
  {
    juce::Path skipToStartPath;
    // Vertical bar (far left) - 20% padding from edges
    skipToStartPath.addRectangle(0.20f, 0.30f, 0.10f, 0.40f);
    // First triangle (left)
    skipToStartPath.addTriangle(0.32f, 0.50f, 0.48f, 0.35f, 0.48f, 0.65f);
    // Second triangle (right)
    skipToStartPath.addTriangle(0.52f, 0.50f, 0.68f, 0.35f, 0.68f, 0.65f);

    auto skipToStartIcon = std::make_unique<juce::DrawablePath>();
    skipToStartIcon->setPath(skipToStartPath);
    skipToStartIcon->setFill(juce::Colours::white);
    m_skipToStartButton->setImages(skipToStartIcon.get());
    skipToStartIcon.release(); // DrawableButton takes ownership
  }
  m_skipToStartButton->setColour(juce::DrawableButton::backgroundColourId,
                                 juce::Colour(0xff3a3a3a));
  m_skipToStartButton->setColour(juce::DrawableButton::backgroundOnColourId,
                                 juce::Colour(0xff4a4a4a));
  m_skipToStartButton->onClick = [this]() {
    if (m_previewPlayer) {
      // Issue #9: Jump to IN point (keep play/pause state)
      bool wasPlaying = m_previewPlayer->isPlaying();
      m_previewPlayer->jumpTo(m_metadata.trimInSamples);

      // If was playing, resume playing; if paused, stay paused
      if (wasPlaying && !m_previewPlayer->isPlaying()) {
        m_previewPlayer->play();
      }

      DBG("ClipEditDialog: Skip to start (IN point) - " << (wasPlaying ? "resumed" : "paused"));
    }
  };
  addAndMakeVisible(m_skipToStartButton.get());

  // Play button (►) - consistent padding and sizing
  m_playButton = std::make_unique<juce::DrawableButton>("Play", juce::DrawableButton::ImageFitted);
  {
    juce::Path playPath;
    // Triangle with 20% padding (0.25-0.75 horizontal, 0.25-0.75 vertical)
    playPath.addTriangle(0.30f, 0.25f, 0.30f, 0.75f, 0.70f, 0.50f);

    auto playIcon = std::make_unique<juce::DrawablePath>();
    playIcon->setPath(playPath);
    playIcon->setFill(juce::Colours::white);
    m_playButton->setImages(playIcon.get());
    playIcon.release(); // DrawableButton takes ownership
  }
  m_playButton->setColour(juce::DrawableButton::backgroundColourId,
                          juce::Colour(0xff2ecc71)); // Green
  m_playButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colour(0xff27ae60));
  m_playButton->onClick = [this]() {
    if (m_previewPlayer) {
      // Play button ALWAYS restarts from IN point (that's its purpose)
      m_previewPlayer->play();
      DBG("ClipEditDialog: Play button - restarted from IN point");
    }
  };
  addAndMakeVisible(m_playButton.get());

  // Stop button (■) - consistent padding and sizing
  m_stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ImageFitted);
  {
    juce::Path stopPath;
    // Square with 20% padding (0.30-0.70 for a centered 40% square)
    stopPath.addRectangle(0.30f, 0.30f, 0.40f, 0.40f);

    auto stopIcon = std::make_unique<juce::DrawablePath>();
    stopIcon->setPath(stopPath);
    stopIcon->setFill(juce::Colours::white);
    m_stopButton->setImages(stopIcon.get());
    stopIcon.release(); // DrawableButton takes ownership
  }
  m_stopButton->setColour(juce::DrawableButton::backgroundColourId,
                          juce::Colour(0xffe74c3c)); // Red
  m_stopButton->setColour(juce::DrawableButton::backgroundOnColourId, juce::Colour(0xffc0392b));
  m_stopButton->onClick = [this]() {
    if (m_previewPlayer) {
      m_previewPlayer->stop();
      DBG("ClipEditDialog: Preview playback stopped");
    }
  };
  addAndMakeVisible(m_stopButton.get());

  // Skip to End button (►►) - consistent padding and sizing
  m_skipToEndButton =
      std::make_unique<juce::DrawableButton>("SkipToEnd", juce::DrawableButton::ImageFitted);
  {
    juce::Path skipToEndPath;
    // First triangle (left)
    skipToEndPath.addTriangle(0.32f, 0.35f, 0.32f, 0.65f, 0.48f, 0.50f);
    // Second triangle (right)
    skipToEndPath.addTriangle(0.52f, 0.35f, 0.52f, 0.65f, 0.68f, 0.50f);
    // Vertical bar (far right) - 20% padding from edges
    skipToEndPath.addRectangle(0.70f, 0.30f, 0.10f, 0.40f);

    auto skipToEndIcon = std::make_unique<juce::DrawablePath>();
    skipToEndIcon->setPath(skipToEndPath);
    skipToEndIcon->setFill(juce::Colours::white);
    m_skipToEndButton->setImages(skipToEndIcon.get());
    skipToEndIcon.release(); // DrawableButton takes ownership
  }
  m_skipToEndButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xff3a3a3a));
  m_skipToEndButton->setColour(juce::DrawableButton::backgroundOnColourId,
                               juce::Colour(0xff4a4a4a));
  m_skipToEndButton->onClick = [this]() {
    if (m_previewPlayer) {
      // Issue #9: Jump to 5 seconds before OUT point (keep play/pause state)
      bool wasPlaying = m_previewPlayer->isPlaying();

      // Calculate target: 5 seconds before OUT, or IN if clip < 5s
      int64_t fiveSecondsInSamples = 5 * m_metadata.sampleRate;
      int64_t targetPosition =
          std::max(m_metadata.trimInSamples, m_metadata.trimOutSamples - fiveSecondsInSamples);

      m_previewPlayer->jumpTo(targetPosition);

      // If was playing, resume playing; if paused, stay paused
      if (wasPlaying && !m_previewPlayer->isPlaying()) {
        m_previewPlayer->play();
      }

      DBG("ClipEditDialog: Skip to end (5s before OUT) - " << (wasPlaying ? "resumed" : "paused"));
    }
  };
  addAndMakeVisible(m_skipToEndButton.get());

  // Loop button (⟲ circular arrow) - DrawableButton with toggle mode
  m_loopButton = std::make_unique<juce::DrawableButton>("Loop", juce::DrawableButton::ImageFitted);
  {
    // Create a composite drawable with both arc and arrow
    auto loopComposite = std::make_unique<juce::DrawableComposite>();

    // Arc path (270 degrees, stroked not filled)
    juce::Path arcPath;
    arcPath.addCentredArc(0.50f, 0.50f, 0.20f, 0.20f, 0.0f, 0.0f, 4.7f, true);
    auto arcDrawable = std::make_unique<juce::DrawablePath>();
    arcDrawable->setPath(arcPath);
    arcDrawable->setStrokeFill(juce::Colours::white);
    arcDrawable->setStrokeThickness(0.06f);
    arcDrawable->setFill(juce::Colours::transparentBlack);
    loopComposite->addAndMakeVisible(arcDrawable.release());

    // Arrow head triangle (filled)
    juce::Path arrowPath;
    arrowPath.addTriangle(0.50f, 0.25f, 0.43f, 0.32f, 0.57f, 0.32f);
    auto arrowDrawable = std::make_unique<juce::DrawablePath>();
    arrowDrawable->setPath(arrowPath);
    arrowDrawable->setFill(juce::Colours::white);
    loopComposite->addAndMakeVisible(arrowDrawable.release());

    m_loopButton->setImages(loopComposite.get());
    loopComposite.release(); // DrawableButton takes ownership
  }
  m_loopButton->setClickingTogglesState(true); // Enable toggle mode
  m_loopButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(0xff3a3a3a));
  m_loopButton->setColour(juce::DrawableButton::backgroundOnColourId,
                          juce::Colour(0xff3498db)); // Blue when active (loop enabled)
  m_loopButton->onClick = [this]() {
    // Update metadata (source of truth)
    m_metadata.loopEnabled = m_loopButton->getToggleState();

    // Sync preview player
    if (m_previewPlayer) {
      m_previewPlayer->setLoopEnabled(m_metadata.loopEnabled);
    }

    DBG("ClipEditDialog: Loop " << (m_metadata.loopEnabled ? "enabled" : "disabled"));
  };
  addAndMakeVisible(m_loopButton.get());

  m_stopOthersButton = std::make_unique<juce::ToggleButton>("Stop Others");
  m_stopOthersButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
  m_stopOthersButton->onClick = [this]() {
    // Update metadata (source of truth)
    m_metadata.stopOthersEnabled = m_stopOthersButton->getToggleState();

    DBG("ClipEditDialog: Stop Others " << (m_metadata.stopOthersEnabled ? "enabled" : "disabled"));
  };
  addAndMakeVisible(m_stopOthersButton.get());

  m_transportPositionLabel = std::make_unique<juce::Label>("posLabel", "00:00:00.00");
  m_transportPositionLabel->setFont(
      juce::FontOptions("Inter", 32.0f, juce::Font::bold)); // Issue #11: Enlarged for readability
  m_transportPositionLabel->setJustificationType(juce::Justification::centred);
  m_transportPositionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(m_transportPositionLabel.get());

  // Wire up preview player callbacks
  if (m_previewPlayer) {
    m_previewPlayer->onPositionChanged = [this](int64_t samplePosition) {
      // Update position label
      if (m_transportPositionLabel) {
        juce::String timeString = samplesToTimeString(samplePosition, m_metadata.sampleRate);
        m_transportPositionLabel->setText(timeString, juce::dontSendNotification);
      }

      // Update waveform playhead
      if (m_waveformDisplay) {
        m_waveformDisplay->setPlayheadPosition(samplePosition);
      }
    };

    m_previewPlayer->onPlaybackStopped = [this]() {
      DBG("ClipEditDialog: Preview playback stopped (reached end or manual stop)");
    };
  }

  // Set up waveform click handlers for LOOP functionality
  m_waveformDisplay->onLeftClick = [this](int64_t samples) {
    // Cmd+Click: Set IN point
    // Validate: IN must be < OUT
    int64_t newInPoint = samples;
    if (newInPoint >= m_metadata.trimOutSamples) {
      newInPoint = std::max(int64_t(0), m_metadata.trimOutSamples - (m_metadata.sampleRate / 75));
    }

    m_metadata.trimInSamples = newInPoint;
    updateTrimInfoLabel();

    // CRITICAL: ONLY update waveform, which triggers onTrimPointsChanged callback
    // That callback handles preview player update (prevents double restart)
    m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

    // Enforce edit law: If playhead < new IN, jump to IN and restart
    if (m_previewPlayer && m_previewPlayer->isPlaying()) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();
      if (currentPos < m_metadata.trimInSamples) {
        m_previewPlayer->play(); // Restarts from new IN point
        DBG("ClipEditDialog: IN point edit law enforced - playhead was < IN ("
            << currentPos << " < " << m_metadata.trimInSamples << "), restarted from IN");
      }
    }

    DBG("ClipEditDialog: Cmd+Click - Set IN point to sample " << newInPoint);
  };

  m_waveformDisplay->onRightClick = [this](int64_t samples) {
    // Cmd+Shift+Click: Set OUT point
    // Validate: OUT must be > IN
    int64_t newOutPoint = samples;
    if (newOutPoint <= m_metadata.trimInSamples) {
      newOutPoint = std::min(m_metadata.durationSamples,
                             m_metadata.trimInSamples + (m_metadata.sampleRate / 75));
    }

    m_metadata.trimOutSamples = newOutPoint;
    updateTrimInfoLabel();

    // CRITICAL: ONLY update waveform, which triggers onTrimPointsChanged callback
    // That callback handles preview player update (prevents double restart)
    m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

    // Enforce edit law: If playhead >= new OUT, jump to IN and restart
    enforceOutPointEditLaw();

    DBG("ClipEditDialog: Cmd+Shift+Click - Set OUT point to sample " << newOutPoint);
  };

  m_waveformDisplay->onMiddleClick = [this](int64_t samples) {
    // Click-to-jog: Jump transport to clicked position
    // CRITICAL: Clamp to [IN, OUT] bounds to prevent playhead from escaping trim range
    if (m_previewPlayer) {
      int64_t clampedSamples =
          std::clamp(samples, m_metadata.trimInSamples, m_metadata.trimOutSamples);
      m_previewPlayer->jumpTo(clampedSamples);
      DBG("ClipEditDialog: Click-to-jog - jumped to sample "
          << clampedSamples << " (clamped from " << samples << " to [" << m_metadata.trimInSamples
          << ", " << m_metadata.trimOutSamples << "])");
    }
  };

  m_waveformDisplay->onTrimPointsChanged = [this](int64_t inSamples, int64_t outSamples) {
    // Update metadata and UI when handles are dragged
    m_metadata.trimInSamples = inSamples;
    m_metadata.trimOutSamples = outSamples;
    updateTrimInfoLabel();

    // Update preview player trim points
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(inSamples, outSamples);
    }
  };

  // Zoom controls: +/- buttons (4 levels: 1x, 2x, 4x, 8x)
  // Zoom always centers on playhead position for intuitive navigation
  m_zoomOutButton = std::make_unique<juce::TextButton>("-");
  m_zoomOutButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
  m_zoomOutButton->onClick = [this]() {
    int currentLevel = m_waveformDisplay->getZoomLevel();
    if (currentLevel > 0) {
      // Get playhead position to center zoom
      int64_t playheadPos = m_previewPlayer ? m_previewPlayer->getCurrentPosition() : 0;
      float playheadNormalized = 0.5f; // Default to center if no playhead
      if (m_metadata.durationSamples > 0) {
        playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
      }

      m_waveformDisplay->setZoomLevel(currentLevel - 1, playheadNormalized);
      updateZoomLabel();
    }
  };
  addAndMakeVisible(m_zoomOutButton.get());

  m_zoomLabel = std::make_unique<juce::Label>("zoomLabel", "1x");
  m_zoomLabel->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_zoomLabel->setJustificationType(juce::Justification::centred);
  m_zoomLabel->setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(m_zoomLabel.get());

  m_zoomInButton = std::make_unique<juce::TextButton>("+");
  m_zoomInButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
  m_zoomInButton->onClick = [this]() {
    int currentLevel = m_waveformDisplay->getZoomLevel();
    if (currentLevel < 4) { // Max zoom level is 4 (16x)
      // Get playhead position to center zoom
      int64_t playheadPos = m_previewPlayer ? m_previewPlayer->getCurrentPosition() : 0;
      float playheadNormalized = 0.5f; // Default to center if no playhead
      if (m_metadata.durationSamples > 0) {
        playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
      }

      m_waveformDisplay->setZoomLevel(currentLevel + 1, playheadNormalized);
      updateZoomLabel();
    }
  };
  addAndMakeVisible(m_zoomInButton.get());

  // Trim In Point
  m_trimInLabel = std::make_unique<juce::Label>("trimInLabel", "Trim In:");
  m_trimInLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_trimInLabel.get());

  // Time editor (MM:SS:FF - SpotOn format, 75fps)
  m_trimInTimeEditor = std::make_unique<juce::TextEditor>();
  m_trimInTimeEditor->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_trimInTimeEditor->setJustification(juce::Justification::centredLeft); // Vertically center text
  m_trimInTimeEditor->setText("00:00:00", false);
  m_trimInTimeEditor->onReturnKey = [this]() {
    int64_t newInPoint = timeStringToSamples(m_trimInTimeEditor->getText(), m_metadata.sampleRate);

    // CRITICAL: Constrain to valid range [0, OUT-1]
    if (newInPoint < 0) {
      newInPoint = 0;
    }
    if (newInPoint >= m_metadata.trimOutSamples) {
      newInPoint = std::max(int64_t(0), m_metadata.trimOutSamples - (m_metadata.sampleRate / 75));
    }

    m_metadata.trimInSamples = newInPoint;
    updateTrimInfoLabel();

    // CRITICAL: ONLY update waveform, which triggers onTrimPointsChanged callback
    // That callback handles preview player update and restart (prevents double restart)
    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // EDIT LAW #3: If playhead is now before new IN point, restart from IN
    if (m_previewPlayer && m_previewPlayer->isPlaying()) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();
      if (currentPos < m_metadata.trimInSamples) {
        m_previewPlayer->play(); // Restart from new IN point (edit law #3)
        DBG("ClipEditDialog: Time editor edit law enforced - playhead < IN ("
            << currentPos << " < " << m_metadata.trimInSamples << "), restarted from IN");
      }
    }
  };
  addAndMakeVisible(m_trimInTimeEditor.get());

  // Nudge buttons (< and > for rapid audition) - Issue #7: NudgeButton with hold-to-repeat
  m_trimInDecButton = std::make_unique<NudgeButton>("<");
  m_trimInDecButton->onClick = [this]() {
    // Decrement by 1 tick (1/75 second) - restarts playback from new IN (SpotOn behavior)
    int64_t decrement = m_metadata.sampleRate / 75;
    m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimInSamples - decrement);

    DBG("ClipEditDialog: < button clicked - New IN: " << m_metadata.trimInSamples);

    updateTrimInfoLabel();

    // Update waveform display (visual only, doesn't trigger callback)
    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // CRITICAL: Update trim points AND force restart (< > buttons ALWAYS restart for rapid
    // audition)
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      // Force restart from new IN point (rapid audition feature)
      if (m_previewPlayer->isPlaying()) {
        m_previewPlayer->play(); // Seamless restart

        // NOTE: No need to check if playhead < IN, because play() already restarts from IN
      }
    }
  };
  addAndMakeVisible(m_trimInDecButton.get());

  m_trimInIncButton = std::make_unique<NudgeButton>(">");
  m_trimInIncButton->onClick = [this]() {
    // Increment by 1 tick (1/75 second) - restarts playback from new IN (SpotOn behavior)
    int64_t increment = m_metadata.sampleRate / 75;
    m_metadata.trimInSamples = std::min(m_metadata.trimOutSamples - (m_metadata.sampleRate / 75),
                                        m_metadata.trimInSamples + increment);

    DBG("ClipEditDialog: > button clicked - New IN: " << m_metadata.trimInSamples);

    updateTrimInfoLabel();

    // Update waveform display (visual only, doesn't trigger callback)
    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // CRITICAL: Update trim points AND force restart (< > buttons ALWAYS restart for rapid
    // audition)
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      // Force restart from new IN point (rapid audition feature)
      if (m_previewPlayer->isPlaying()) {
        m_previewPlayer->play(); // Seamless restart
      }
    }
  };
  addAndMakeVisible(m_trimInIncButton.get());

  // SET button for IN point (capture current playback position - SpotOn-inspired)
  m_trimInHoldButton = std::make_unique<juce::TextButton>("SET");
  m_trimInHoldButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3498db)); // Blue
  m_trimInHoldButton->onClick = [this]() {
    if (m_previewPlayer) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();

      // Enforce edit law: IN must be < OUT
      if (currentPos >= m_metadata.trimOutSamples) {
        currentPos = std::max(int64_t(0), m_metadata.trimOutSamples - (m_metadata.sampleRate / 75));
      }

      m_metadata.trimInSamples = currentPos;
      updateTrimInfoLabel();

      // CRITICAL: ONLY update waveform, which will trigger onTrimPointsChanged callback
      // That callback handles preview player update (prevents double restart)
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      DBG("ClipEditDialog: SET - Set IN point to current position " << currentPos);
    }
  };
  addAndMakeVisible(m_trimInHoldButton.get());

  // CLEAR button for IN point (reset to 0)
  m_trimInClearButton = std::make_unique<juce::TextButton>("CLR");
  m_trimInClearButton->onClick = [this]() {
    m_metadata.trimInSamples = 0;

    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    DBG("ClipEditDialog: IN point cleared to 0");
  };
  addAndMakeVisible(m_trimInClearButton.get());

  // Trim Out Point
  m_trimOutLabel = std::make_unique<juce::Label>("trimOutLabel", "Trim Out:");
  m_trimOutLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_trimOutLabel.get());

  // Time editor (MM:SS:FF - SpotOn format, 75fps)
  m_trimOutTimeEditor = std::make_unique<juce::TextEditor>();
  m_trimOutTimeEditor->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_trimOutTimeEditor->setJustification(juce::Justification::centredLeft); // Vertically center text
  m_trimOutTimeEditor->setText("00:00:00", false);
  m_trimOutTimeEditor->onReturnKey = [this]() {
    int64_t newOutPoint =
        timeStringToSamples(m_trimOutTimeEditor->getText(), m_metadata.sampleRate);

    // CRITICAL: Constrain to valid range [IN+1, duration]
    if (newOutPoint <= m_metadata.trimInSamples) {
      newOutPoint = m_metadata.trimInSamples + (m_metadata.sampleRate / 75);
    }
    if (newOutPoint > m_metadata.durationSamples) {
      newOutPoint = m_metadata.durationSamples;
    }

    m_metadata.trimOutSamples = newOutPoint;
    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

      // CRITICAL: Enforce OUT point edit law (if playhead >= OUT, jump to IN and restart)
      enforceOutPointEditLaw();
    }
  };
  addAndMakeVisible(m_trimOutTimeEditor.get());

  // Nudge buttons - Issue #7: NudgeButton with hold-to-repeat
  m_trimOutDecButton = std::make_unique<NudgeButton>("<");
  m_trimOutDecButton->onClick = [this]() {
    // Decrement by 1 tick (1/75 second) - NO restart (SpotOn behavior)
    int64_t decrement = m_metadata.sampleRate / 75;
    m_metadata.trimOutSamples = std::max(m_metadata.trimInSamples + (m_metadata.sampleRate / 75),
                                         m_metadata.trimOutSamples - decrement);

    DBG("ClipEditDialog: OUT < button clicked - New OUT: " << m_metadata.trimOutSamples);

    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player trim points WITHOUT restarting playback
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

      // CRITICAL: Enforce OUT point edit law (if playhead >= OUT, jump to IN and restart)
      enforceOutPointEditLaw();
    }
  };
  addAndMakeVisible(m_trimOutDecButton.get());

  m_trimOutIncButton = std::make_unique<NudgeButton>(">");
  m_trimOutIncButton->onClick = [this]() {
    // Increment by 1 tick (1/75 second) - NO restart (SpotOn behavior)
    int64_t increment = m_metadata.sampleRate / 75;
    m_metadata.trimOutSamples =
        std::min(m_metadata.durationSamples, m_metadata.trimOutSamples + increment);

    DBG("ClipEditDialog: OUT > button clicked - New OUT: " << m_metadata.trimOutSamples);

    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player trim points WITHOUT restarting playback
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

      // NOTE: Incrementing OUT point extends the range, so playhead cannot be past new OUT
      // No need to clamp playhead when extending OUT range
    }
  };
  addAndMakeVisible(m_trimOutIncButton.get());

  // SET button for OUT point (capture current playback position - SpotOn-inspired)
  m_trimOutHoldButton = std::make_unique<juce::TextButton>("SET");
  m_trimOutHoldButton->setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xff3498db)); // Blue
  m_trimOutHoldButton->onClick = [this]() {
    if (m_previewPlayer) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();

      // Enforce edit law: OUT must be > IN
      if (currentPos <= m_metadata.trimInSamples) {
        currentPos = std::min(m_metadata.durationSamples,
                              m_metadata.trimInSamples + (m_metadata.sampleRate / 75));
      }

      m_metadata.trimOutSamples = currentPos;
      updateTrimInfoLabel();

      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      // Update preview player (no restart needed for OUT point)
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

      DBG("ClipEditDialog: SET - Set OUT point to current position " << currentPos);
    }
  };
  addAndMakeVisible(m_trimOutHoldButton.get());

  // CLEAR button for OUT point (reset to max duration)
  m_trimOutClearButton = std::make_unique<juce::TextButton>("CLR");
  m_trimOutClearButton->onClick = [this]() {
    m_metadata.trimOutSamples = m_metadata.durationSamples;

    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    DBG("ClipEditDialog: OUT point cleared to max duration (" << m_metadata.durationSamples
                                                              << " samples)");
  };
  addAndMakeVisible(m_trimOutClearButton.get());

  // Trim Info Label (shows duration in seconds)
  m_trimInfoLabel = std::make_unique<juce::Label>("trimInfoLabel", "Duration: --:--");
  m_trimInfoLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  m_trimInfoLabel->setColour(juce::Label::textColourId, juce::Colours::white);
  m_trimInfoLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff2a2a2a));
  m_trimInfoLabel->setColour(juce::Label::outlineColourId, juce::Colour(0xff555555));
  m_trimInfoLabel->setJustificationType(juce::Justification::centred);
  addAndMakeVisible(m_trimInfoLabel.get());

  // File Info Panel (SpotOn-style yellow background)
  m_fileInfoPanel = std::make_unique<juce::Label>("fileInfoPanel", "");
  m_fileInfoPanel->setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
  m_fileInfoPanel->setJustificationType(juce::Justification::centredLeft);
  m_fileInfoPanel->setColour(juce::Label::backgroundColourId, juce::Colour(0xfffff4cc)); // Yellow
  m_fileInfoPanel->setColour(juce::Label::textColourId, juce::Colours::black);
  addAndMakeVisible(m_fileInfoPanel.get());

  // Gain Control (Feature 5: -30dB to +10dB, default 0dB)
  m_gainLabel = std::make_unique<juce::Label>("gainLabel", "Gain:");
  m_gainLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_gainLabel.get());

  m_gainSlider =
      std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
  m_gainSlider->setRange(-30.0, 10.0, 1.0); // -30dB to +10dB in 1dB steps
  m_gainSlider->setValue(0.0);              // Default 0dB
  m_gainSlider->onValueChange = [this]() {
    // Update gain value label
    double gain = m_gainSlider->getValue();
    m_gainValueLabel->setText(juce::String(gain, 1) + " dB", juce::dontSendNotification);

    // Update metadata
    m_metadata.gainDb = gain;

    // TODO (User note): Wire to AudioEngine gain attenuation
  };
  addAndMakeVisible(m_gainSlider.get());

  m_gainValueLabel = std::make_unique<juce::Label>("gainValueLabel", "0.0 dB");
  m_gainValueLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::plain));
  m_gainValueLabel->setJustificationType(juce::Justification::centred);
  m_gainValueLabel->setEditable(true);
  m_gainValueLabel->onTextChange = [this]() {
    // Parse text input (accept single integer like "3" or decimal like "3.0")
    juce::String text = m_gainValueLabel->getText()
                            .trimCharactersAtStart("+-")
                            .upToFirstOccurrenceOf("dB", false, true)
                            .trim();
    double gain = text.getDoubleValue();
    gain = juce::jlimit(-30.0, 10.0, gain);
    m_gainSlider->setValue(gain, juce::dontSendNotification);
    m_gainValueLabel->setText(juce::String(gain, 1) + " dB", juce::dontSendNotification);
    m_metadata.gainDb = gain;
  };
  addAndMakeVisible(m_gainValueLabel.get());
}

void ClipEditDialog::buildPhase3UI() {
  // Fade In Section
  m_fadeInLabel = std::make_unique<juce::Label>("fadeInLabel", "Fade In:");
  m_fadeInLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_fadeInLabel.get());

  m_fadeInCombo = std::make_unique<juce::ComboBox>();
  m_fadeInCombo->addItem("0.0 s", 1);
  m_fadeInCombo->addItem("0.1 s", 2);
  m_fadeInCombo->addItem("0.2 s", 3);
  m_fadeInCombo->addItem("0.3 s", 4);
  m_fadeInCombo->addItem("0.4 s", 5);
  m_fadeInCombo->addItem("0.5 s", 6);
  m_fadeInCombo->addItem("0.6 s", 7);
  m_fadeInCombo->addItem("0.7 s", 8);
  m_fadeInCombo->addItem("0.8 s", 9);
  m_fadeInCombo->addItem("0.9 s", 10);
  m_fadeInCombo->addItem("1.0 s", 11);
  m_fadeInCombo->addItem("1.2 s", 12);
  m_fadeInCombo->addItem("1.6 s", 13);
  m_fadeInCombo->addItem("2.0 s", 14);
  m_fadeInCombo->addItem("2.4 s", 15);
  m_fadeInCombo->addItem("3.0 s", 16);
  m_fadeInCombo->setSelectedId(1, juce::dontSendNotification);
  m_fadeInCombo->onChange = [this]() {
    switch (m_fadeInCombo->getSelectedId()) {
    case 1:
      m_metadata.fadeInSeconds = 0.0;
      break;
    case 2:
      m_metadata.fadeInSeconds = 0.1;
      break;
    case 3:
      m_metadata.fadeInSeconds = 0.2;
      break;
    case 4:
      m_metadata.fadeInSeconds = 0.3;
      break;
    case 5:
      m_metadata.fadeInSeconds = 0.4;
      break;
    case 6:
      m_metadata.fadeInSeconds = 0.5;
      break;
    case 7:
      m_metadata.fadeInSeconds = 0.6;
      break;
    case 8:
      m_metadata.fadeInSeconds = 0.7;
      break;
    case 9:
      m_metadata.fadeInSeconds = 0.8;
      break;
    case 10:
      m_metadata.fadeInSeconds = 0.9;
      break;
    case 11:
      m_metadata.fadeInSeconds = 1.0;
      break;
    case 12:
      m_metadata.fadeInSeconds = 1.2;
      break;
    case 13:
      m_metadata.fadeInSeconds = 1.6;
      break;
    case 14:
      m_metadata.fadeInSeconds = 2.0;
      break;
    case 15:
      m_metadata.fadeInSeconds = 2.4;
      break;
    case 16:
      m_metadata.fadeInSeconds = 3.0;
      break;
    }
    // Update preview player fades
    if (m_previewPlayer) {
      m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
    }
  };
  addAndMakeVisible(m_fadeInCombo.get());

  m_fadeInCurveCombo = std::make_unique<juce::ComboBox>();
  m_fadeInCurveCombo->addItem("Linear", 1);
  m_fadeInCurveCombo->addItem("Equal Power", 2);
  m_fadeInCurveCombo->addItem("Exponential", 3);
  m_fadeInCurveCombo->setSelectedId(1, juce::dontSendNotification);
  m_fadeInCurveCombo->onChange = [this]() {
    switch (m_fadeInCurveCombo->getSelectedId()) {
    case 1:
      m_metadata.fadeInCurve = "Linear";
      break;
    case 2:
      m_metadata.fadeInCurve = "EqualPower";
      break;
    case 3:
      m_metadata.fadeInCurve = "Exponential";
      break;
    }
    // Update preview player fades
    if (m_previewPlayer) {
      m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
    }
  };
  addAndMakeVisible(m_fadeInCurveCombo.get());

  // Fade Out Section
  m_fadeOutLabel = std::make_unique<juce::Label>("fadeOutLabel", "Fade Out:");
  m_fadeOutLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_fadeOutLabel.get());

  m_fadeOutCombo = std::make_unique<juce::ComboBox>();
  m_fadeOutCombo->addItem("0.0 s", 1);
  m_fadeOutCombo->addItem("0.1 s", 2);
  m_fadeOutCombo->addItem("0.2 s", 3);
  m_fadeOutCombo->addItem("0.3 s", 4);
  m_fadeOutCombo->addItem("0.4 s", 5);
  m_fadeOutCombo->addItem("0.5 s", 6);
  m_fadeOutCombo->addItem("0.6 s", 7);
  m_fadeOutCombo->addItem("0.7 s", 8);
  m_fadeOutCombo->addItem("0.8 s", 9);
  m_fadeOutCombo->addItem("0.9 s", 10);
  m_fadeOutCombo->addItem("1.0 s", 11);
  m_fadeOutCombo->addItem("1.2 s", 12);
  m_fadeOutCombo->addItem("1.6 s", 13);
  m_fadeOutCombo->addItem("2.0 s", 14);
  m_fadeOutCombo->addItem("2.4 s", 15);
  m_fadeOutCombo->addItem("3.0 s", 16);
  m_fadeOutCombo->setSelectedId(1, juce::dontSendNotification);
  m_fadeOutCombo->onChange = [this]() {
    switch (m_fadeOutCombo->getSelectedId()) {
    case 1:
      m_metadata.fadeOutSeconds = 0.0;
      break;
    case 2:
      m_metadata.fadeOutSeconds = 0.1;
      break;
    case 3:
      m_metadata.fadeOutSeconds = 0.2;
      break;
    case 4:
      m_metadata.fadeOutSeconds = 0.3;
      break;
    case 5:
      m_metadata.fadeOutSeconds = 0.4;
      break;
    case 6:
      m_metadata.fadeOutSeconds = 0.5;
      break;
    case 7:
      m_metadata.fadeOutSeconds = 0.6;
      break;
    case 8:
      m_metadata.fadeOutSeconds = 0.7;
      break;
    case 9:
      m_metadata.fadeOutSeconds = 0.8;
      break;
    case 10:
      m_metadata.fadeOutSeconds = 0.9;
      break;
    case 11:
      m_metadata.fadeOutSeconds = 1.0;
      break;
    case 12:
      m_metadata.fadeOutSeconds = 1.2;
      break;
    case 13:
      m_metadata.fadeOutSeconds = 1.6;
      break;
    case 14:
      m_metadata.fadeOutSeconds = 2.0;
      break;
    case 15:
      m_metadata.fadeOutSeconds = 2.4;
      break;
    case 16:
      m_metadata.fadeOutSeconds = 3.0;
      break;
    }
    // Update preview player fades
    if (m_previewPlayer) {
      m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
    }
  };
  addAndMakeVisible(m_fadeOutCombo.get());

  m_fadeOutCurveCombo = std::make_unique<juce::ComboBox>();
  m_fadeOutCurveCombo->addItem("Linear", 1);
  m_fadeOutCurveCombo->addItem("Equal Power", 2);
  m_fadeOutCurveCombo->addItem("Exponential", 3);
  m_fadeOutCurveCombo->setSelectedId(1, juce::dontSendNotification);
  m_fadeOutCurveCombo->onChange = [this]() {
    switch (m_fadeOutCurveCombo->getSelectedId()) {
    case 1:
      m_metadata.fadeOutCurve = "Linear";
      break;
    case 2:
      m_metadata.fadeOutCurve = "EqualPower";
      break;
    case 3:
      m_metadata.fadeOutCurve = "Exponential";
      break;
    }
    // Update preview player fades
    if (m_previewPlayer) {
      m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
    }
  };
  addAndMakeVisible(m_fadeOutCurveCombo.get());
}

//==============================================================================
void ClipEditDialog::paint(juce::Graphics& g) {
  // Dark background
  g.fillAll(juce::Colour(0xff1a1a1a));

  // Styled outer border (professional 2px with subtle highlight)
  auto bounds = getLocalBounds().toFloat();
  g.setColour(juce::Colour(0xff3498db).withAlpha(0.6f)); // Blue accent
  g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);

  // Inner shadow/depth effect
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.drawRoundedRectangle(bounds.reduced(3.0f), 4.0f, 1.0f);

  // Title bar
  g.setColour(juce::Colour(0xff252525));
  g.fillRect(0, 0, getWidth(), 50);

  // Title text
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions("Inter", 20.0f, juce::Font::bold));
  g.drawText("Clip Edit", 20, 0, 400, 50, juce::Justification::centredLeft, false);
}

void ClipEditDialog::resized() {
  // Updated "Clip Edit" layout with reorganized sections
  const int GRID = 10; // 10px grid unit
  auto bounds = getLocalBounds();

  // Title bar (50px)
  bounds.removeFromTop(50);

  // Content area with padding
  auto contentArea = bounds.reduced(GRID * 2);

  // === CLIP NAME SECTION (at top, below header) ===
  // Clip Name label
  if (m_nameLabel) {
    m_nameLabel->setBounds(contentArea.removeFromTop(GRID * 2));
  }
  contentArea.removeFromTop(GRID / 2);

  // Clip Name field (full width)
  if (m_nameEditor) {
    m_nameEditor->setBounds(contentArea.removeFromTop(GRID * 3));
  }
  contentArea.removeFromTop(GRID * 1.5); // Increased: GRID -> GRID * 1.5

  // === FILE INFO PANEL + ZOOM CONTROLS (on same row) ===
  auto headerRow = contentArea.removeFromTop(GRID * 3);

  // File Info Panel (left side, takes most of the width)
  if (m_fileInfoPanel) {
    auto fileInfoArea = headerRow;
    // Reserve space for zoom controls on the right
    fileInfoArea.removeFromRight(GRID * 13); // Space for zoom controls + margin
    m_fileInfoPanel->setBounds(fileInfoArea);
  }

  // Zoom controls (top-right corner with left margin)
  if (m_zoomOutButton && m_zoomLabel && m_zoomInButton) {
    auto zoomArea = headerRow.removeFromRight(GRID * 12);
    zoomArea.removeFromLeft(GRID); // Left margin for spacing
    m_zoomOutButton->setBounds(zoomArea.removeFromLeft(GRID * 3));
    zoomArea.removeFromLeft(GRID / 2);
    m_zoomLabel->setBounds(zoomArea.removeFromLeft(GRID * 4));
    zoomArea.removeFromLeft(GRID / 2);
    m_zoomInButton->setBounds(zoomArea.removeFromLeft(GRID * 3));
  }

  contentArea.removeFromTop(GRID); // Spacing

  // === WAVEFORM SECTION ===
  if (m_waveformDisplay) {
    m_waveformDisplay->setBounds(contentArea.removeFromTop(GRID * 15));
  }
  contentArea.removeFromTop(GRID);

  // === TRANSPORT BAR (centered playback controls + vertically centered time display) ===
  if (m_skipToStartButton && m_playButton && m_stopButton && m_skipToEndButton && m_loopButton &&
      m_transportPositionLabel) {
    // Reserve space for transport section - will be centered between buttons and Duration
    const int TRANSPORT_HEIGHT = GRID * 10; // Increased to accommodate centered time
    auto transportRow = contentArea.removeFromTop(TRANSPORT_HEIGHT);

    // Transport buttons (top)
    auto buttonRow = transportRow.removeFromTop(GRID * 4);
    auto transportCenter = buttonRow.withSizeKeepingCentre(GRID * 35, GRID * 4);

    // Skip to Start button (◄◄)
    m_skipToStartButton->setBounds(transportCenter.removeFromLeft(GRID * 4));
    transportCenter.removeFromLeft(GRID);

    // Loop button
    m_loopButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Play button (►)
    m_playButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Stop button (■)
    m_stopButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Skip to End button (►►)
    m_skipToEndButton->setBounds(transportCenter.removeFromLeft(GRID * 4));

    // Transport position label (vertically centered - equidistant from buttons and Duration)
    // Using remaining height, center the time display
    auto labelRow = transportRow.withSizeKeepingCentre(GRID * 20, GRID * 5);
    m_transportPositionLabel->setBounds(labelRow);
  }
  contentArea.removeFromTop(GRID);

  // === TRIM SECTION (with vertical margin between button rows) ===
  auto trimSection = contentArea.removeFromTop(GRID * 13);

  // Calculate layout constants
  const int TIME_FIELD_WIDTH = GRID * 11;                           // Time input fields
  const int BUTTON_WIDTH = GRID * 5;                                // SET/CLR buttons
  const int NAV_BUTTON_WIDTH = GRID * 3;                            // < > buttons
  const int DURATION_WIDTH = GRID * 20;                             // Duration display box
  const int SECTION_SPACING = GRID * 2;                             // Space between sections
  const int SECTION_WIDTH = TIME_FIELD_WIDTH + GRID + BUTTON_WIDTH; // Total width per trim section

  // Row 1: Labels (Trim In on left, Trim Out on right)
  auto labelRow = trimSection.removeFromTop(GRID * 2);
  if (m_trimInLabel && m_trimOutLabel) {
    auto leftLabelArea = labelRow.removeFromLeft(SECTION_WIDTH);
    m_trimInLabel->setBounds(leftLabelArea);

    auto rightLabelArea = labelRow.removeFromRight(SECTION_WIDTH);
    m_trimOutLabel->setBounds(rightLabelArea);
  }
  trimSection.removeFromTop(GRID / 2);

  // Row 2: Time fields + SET buttons (same row) + Duration (center, aligned with this row)
  auto controlsRow = trimSection.removeFromTop(GRID * 3);

  // === TRIM IN (left): Time field + SET button (to the right) ===
  auto trimInArea = controlsRow.removeFromLeft(SECTION_WIDTH);

  if (m_trimInTimeEditor && m_trimInHoldButton) {
    m_trimInTimeEditor->setBounds(trimInArea.removeFromLeft(TIME_FIELD_WIDTH));
    trimInArea.removeFromLeft(GRID);                                        // Spacing
    m_trimInHoldButton->setBounds(trimInArea.removeFromLeft(BUTTON_WIDTH)); // SET to the right
  }

  controlsRow.removeFromLeft(SECTION_SPACING);

  // === TRIM OUT (right): SET button (to the left) + Time field ===
  auto trimOutArea = controlsRow.removeFromRight(SECTION_WIDTH);

  if (m_trimOutHoldButton && m_trimOutTimeEditor) {
    m_trimOutTimeEditor->setBounds(trimOutArea.removeFromRight(TIME_FIELD_WIDTH));
    trimOutArea.removeFromRight(GRID);                                         // Spacing
    m_trimOutHoldButton->setBounds(trimOutArea.removeFromRight(BUTTON_WIDTH)); // SET to the left
  }

  // Duration display (vertically aligned with SET buttons - same baseline)
  if (m_trimInfoLabel) {
    m_trimInfoLabel->setBounds(controlsRow.withSizeKeepingCentre(DURATION_WIDTH, GRID * 3));
  }

  trimSection.removeFromTop(GRID); // Vertical margin between rows

  // Row 3: Navigation buttons (< > to the left, CLR vertically aligned with SET)
  auto navRow = trimSection.removeFromTop(GRID * 3);

  // Trim In navigation (left side: < > buttons, then CLR aligned with SET above)
  if (m_trimInDecButton && m_trimInIncButton && m_trimInClearButton) {
    auto navLeft = navRow.removeFromLeft(SECTION_WIDTH);
    // Place < > buttons first (below time field)
    m_trimInDecButton->setBounds(navLeft.removeFromLeft(NAV_BUTTON_WIDTH));
    navLeft.removeFromLeft(GRID);
    m_trimInIncButton->setBounds(navLeft.removeFromLeft(NAV_BUTTON_WIDTH));
    // Skip remaining space to align CLR with SET button above (which is at TIME_FIELD_WIDTH + GRID)
    int currentX = NAV_BUTTON_WIDTH + GRID + NAV_BUTTON_WIDTH;
    int setButtonX = TIME_FIELD_WIDTH + GRID;
    int skipWidth = setButtonX - currentX;
    navLeft.removeFromLeft(skipWidth);
    m_trimInClearButton->setBounds(navLeft.removeFromLeft(BUTTON_WIDTH)); // CLR aligned with SET
  }

  navRow.removeFromLeft(SECTION_SPACING);

  // Trim Out navigation (right side: < > buttons, then CLR aligned with SET above)
  // BUG FIX 8: Corrected layout logic to make buttons visible
  if (m_trimOutDecButton && m_trimOutIncButton && m_trimOutClearButton) {
    auto navRight = navRow.removeFromRight(SECTION_WIDTH);
    // Place < > buttons from LEFT, then skip to align CLR with SET
    // Row 2 layout: [........SET][GRID][.......TIME.......]
    // Row 3 layout: [<][GRID][>][GRID][..CLR..] (aligned with SET)

    m_trimOutDecButton->setBounds(navRight.removeFromLeft(NAV_BUTTON_WIDTH));
    navRight.removeFromLeft(GRID);
    m_trimOutIncButton->setBounds(navRight.removeFromLeft(NAV_BUTTON_WIDTH));

    // Skip remaining space to align CLR with SET button above
    // SET button is at position: TIME_FIELD_WIDTH + GRID from the right
    // We've used: NAV_BUTTON_WIDTH + GRID + NAV_BUTTON_WIDTH from the left
    // Need to skip to get to SET position
    int usedWidth = NAV_BUTTON_WIDTH + GRID + NAV_BUTTON_WIDTH;
    int setButtonPosition = TIME_FIELD_WIDTH + GRID;
    navRight.removeFromLeft(setButtonPosition - usedWidth);
    m_trimOutClearButton->setBounds(navRight.removeFromLeft(BUTTON_WIDTH));
  }

  contentArea.removeFromTop(GRID); // Reduced: GRID * 2 -> GRID

  // === GAIN CONTROL (Feature 5) ===
  auto gainRow = contentArea.removeFromTop(GRID * 3);
  if (m_gainLabel && m_gainSlider && m_gainValueLabel) {
    // Label on the left
    m_gainLabel->setBounds(gainRow.removeFromLeft(GRID * 6));
    gainRow.removeFromLeft(GRID);

    // Value label on the right
    auto valueLabelArea = gainRow.removeFromRight(GRID * 10);
    m_gainValueLabel->setBounds(valueLabelArea);

    // Slider takes remaining space
    gainRow.removeFromRight(GRID);
    m_gainSlider->setBounds(gainRow);
  }

  contentArea.removeFromTop(GRID * 1.5); // Reduced: GRID * 2 -> GRID * 1.5

  // === FADE SECTION (Equal width columns with consistent spacing) ===
  auto fadeSection = contentArea.removeFromTop(GRID * 6);

  // Calculate equal column widths
  const int fadeColumnWidth = (fadeSection.getWidth() - GRID * 2) / 2; // 50% each with center gap
  const int FADE_TIME_WIDTH = GRID * 7;                                // Time dropdown
  const int FADE_CURVE_WIDTH = GRID * 12;                              // Curve dropdown

  // Labels row (Fade In left, Fade Out right)
  auto fadeLabelsRow = fadeSection.removeFromTop(GRID * 2);
  if (m_fadeInLabel) {
    auto fadeInLabelArea = fadeLabelsRow.removeFromLeft(fadeColumnWidth);
    m_fadeInLabel->setBounds(fadeInLabelArea);
  }
  fadeLabelsRow.removeFromLeft(GRID * 2); // Center gap
  if (m_fadeOutLabel) {
    m_fadeOutLabel->setBounds(fadeLabelsRow); // Takes remaining width
  }

  fadeSection.removeFromTop(GRID / 2);

  // Controls row (equal width columns)
  auto fadeControlsRow = fadeSection.removeFromTop(GRID * 3);

  // Fade IN controls (left column: time + curve)
  auto fadeInArea = fadeControlsRow.removeFromLeft(fadeColumnWidth);
  if (m_fadeInCombo && m_fadeInCurveCombo) {
    m_fadeInCombo->setBounds(fadeInArea.removeFromLeft(FADE_TIME_WIDTH));
    fadeInArea.removeFromLeft(GRID);
    m_fadeInCurveCombo->setBounds(fadeInArea); // Takes remaining space
  }

  fadeControlsRow.removeFromLeft(GRID * 2); // Center gap

  // Fade OUT controls (right column: time + curve, equal width to left)
  if (m_fadeOutCombo && m_fadeOutCurveCombo) {
    m_fadeOutCombo->setBounds(fadeControlsRow.removeFromLeft(FADE_TIME_WIDTH));
    fadeControlsRow.removeFromLeft(GRID);
    m_fadeOutCurveCombo->setBounds(fadeControlsRow); // Takes remaining space
  }

  contentArea.removeFromTop(GRID * 1.5); // Increased: GRID * 2 -> GRID * 1.5 for fade spacing

  // === COLOR + GROUP SECTION (flush on same row) ===
  const int SPACING = GRID;
  const int COLOR_LABEL_WIDTH = GRID * 5;
  const int COLOR_PICKER_WIDTH = GRID * 18; // Compact button width (expandable)
  const int GROUP_LABEL_WIDTH = GRID * 6;

  auto colorGroupRow = contentArea.removeFromTop(GRID * 3);

  // Color label + expandable color button
  if (m_colorLabel && m_colorSwatchPicker) {
    m_colorLabel->setBounds(colorGroupRow.removeFromLeft(COLOR_LABEL_WIDTH));
    colorGroupRow.removeFromLeft(SPACING / 2);
    m_colorSwatchPicker->setBounds(colorGroupRow.removeFromLeft(COLOR_PICKER_WIDTH));
    colorGroupRow.removeFromLeft(SPACING);
  }

  // Group label + dropdown (flush with color controls)
  if (m_groupLabel && m_groupComboBox) {
    m_groupLabel->setBounds(colorGroupRow.removeFromLeft(GROUP_LABEL_WIDTH));
    colorGroupRow.removeFromLeft(SPACING / 2);
    m_groupComboBox->setBounds(colorGroupRow); // Takes remaining width
  }

  contentArea.removeFromTop(GRID * 1.5); // Added bottom margin for color/group row

  // Dialog buttons at bottom (25% taller)
  auto buttonArea = contentArea.removeFromBottom(GRID * 5); // Increased: GRID * 4 -> GRID * 5
  m_cancelButton->setBounds(buttonArea.removeFromRight(GRID * 10));
  buttonArea.removeFromRight(GRID);
  m_okButton->setBounds(buttonArea.removeFromRight(GRID * 10));
}

//==============================================================================
bool ClipEditDialog::keyPressed(const juce::KeyPress& key) {
  // SPACE key: Toggle Play/Pause (Issue #4)
  if (key == juce::KeyPress::spaceKey) {
    if (m_previewPlayer) {
      if (m_previewPlayer->isPlaying()) {
        m_previewPlayer->stop();
        DBG("ClipEditDialog: SPACE - Stopped playback");
      } else {
        m_previewPlayer->play();
        DBG("ClipEditDialog: SPACE - Started playback");
      }
    }
    return true;
  }

  // TAB key: Custom focus order (Name → Trim IN → Trim OUT only)
  if (key == juce::KeyPress::tabKey) {
    // Determine current focus
    auto* focused = juce::Component::getCurrentlyFocusedComponent();

    if (focused == m_nameEditor.get()) {
      // Name → Trim IN
      if (m_trimInTimeEditor) {
        m_trimInTimeEditor->grabKeyboardFocus();
      }
    } else if (focused == m_trimInTimeEditor.get()) {
      // Trim IN → Trim OUT
      if (m_trimOutTimeEditor) {
        m_trimOutTimeEditor->grabKeyboardFocus();
      }
    } else if (focused == m_trimOutTimeEditor.get()) {
      // Trim OUT → Name (cycle back)
      if (m_nameEditor) {
        m_nameEditor->grabKeyboardFocus();
      }
    } else {
      // Nothing focused or other control focused → start at Name
      if (m_nameEditor) {
        m_nameEditor->grabKeyboardFocus();
      }
    }
    return true; // Consume Tab key
  }

  // ENTER key: Submit dialog (OK) - only if not in text editor
  if (key == juce::KeyPress::returnKey ||
      key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys(), 0)) {
    // Check if we're in a text editor (they handle Enter themselves)
    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (focused == m_nameEditor.get() || focused == m_trimInTimeEditor.get() ||
        focused == m_trimOutTimeEditor.get()) {
      // Let text editor handle Enter (will trigger OK via onReturnKey callback)
      return false;
    }

    // Otherwise, trigger OK button directly
    if (onOkClicked)
      onOkClicked(m_metadata);
    return true;
  }

  // ESC key: Cancel dialog
  if (key == juce::KeyPress::escapeKey) {
    if (onCancelClicked)
      onCancelClicked();
    return true;
  }

  // ? key (Shift+/): Toggle Loop (Issue #4)
  if (key == juce::KeyPress('?') ||
      (key == juce::KeyPress('/') && key.getModifiers().isShiftDown())) {
    if (m_loopButton) {
      bool newState = !m_loopButton->getToggleState();
      m_loopButton->setToggleState(newState, juce::sendNotification);
      m_metadata.loopEnabled = newState;
      if (m_previewPlayer) {
        m_previewPlayer->setLoopEnabled(newState);
      }
      DBG("ClipEditDialog: ? key - Loop " << (newState ? "enabled" : "disabled"));
    }
    return true;
  }

  // Keyboard shortcuts (Issue #7 + Issue #10)
  //
  // TRIM POINTS:
  //   I = Set IN point (at current transport position)
  //   O = Set OUT point (at current transport position)
  //   J = Jump transport (not yet implemented)
  //   [ = Nudge IN point left (-1 tick)
  //   ] = Nudge IN point right (+1 tick)
  //   Shift+[ = Nudge IN point left (-15 ticks)
  //   Shift+] = Nudge IN point right (+15 ticks)
  //   ; = Nudge OUT point left (-1 tick)
  //   ' = Nudge OUT point right (+1 tick)
  //   Shift+; = Nudge OUT point left (-15 ticks)
  //   Shift+' = Nudge OUT point right (+15 ticks)
  //
  // WAVEFORM ZOOM:
  //   Cmd/Ctrl + Plus = Zoom in (1x → 2x → 4x → 8x → 16x)
  //   Cmd/Ctrl + Minus = Zoom out (16x → 8x → 4x → 2x → 1x)
  //
  // FADE TIMES (Issue #10):
  //   Cmd/Ctrl+Shift+[1-9] = Set OUT fade time (0.1s-0.9s)
  //   Cmd/Ctrl+Shift+0 = Set OUT fade time (1.0s)
  //   Cmd/Ctrl+Opt+Shift+[1-9] = Set IN fade time (0.1s-0.9s)
  //   Cmd/Ctrl+Opt+Shift+0 = Set IN fade time (1.0s)

  int64_t tickInSamples = m_metadata.sampleRate / 75;
  int64_t oneSecondInSamples = m_metadata.sampleRate;

  // Zoom keyboard shortcuts: Cmd/Ctrl +/- (must check BEFORE +/- alone)
  if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) &&
      (key == juce::KeyPress('+') || key == juce::KeyPress('='))) {
    // Zoom In (Cmd/Ctrl +)
    int currentLevel = m_waveformDisplay->getZoomLevel();
    if (currentLevel < 4) { // Max zoom level is 4 (16x)
      int64_t playheadPos = m_previewPlayer ? m_previewPlayer->getCurrentPosition() : 0;
      float playheadNormalized = 0.5f;
      if (m_metadata.durationSamples > 0) {
        playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
      }
      m_waveformDisplay->setZoomLevel(currentLevel + 1, playheadNormalized);
      updateZoomLabel();
      DBG("ClipEditDialog: Cmd/Ctrl+Plus - Zoom in to level " << (currentLevel + 1));
    }
    return true;
  }

  if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) &&
      key == juce::KeyPress('-')) {
    // Zoom Out (Cmd/Ctrl -)
    int currentLevel = m_waveformDisplay->getZoomLevel();
    if (currentLevel > 0) {
      int64_t playheadPos = m_previewPlayer ? m_previewPlayer->getCurrentPosition() : 0;
      float playheadNormalized = 0.5f;
      if (m_metadata.durationSamples > 0) {
        playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
      }
      m_waveformDisplay->setZoomLevel(currentLevel - 1, playheadNormalized);
      updateZoomLabel();
      DBG("ClipEditDialog: Cmd/Ctrl+Minus - Zoom out to level " << (currentLevel - 1));
    }
    return true;
  }

  // I key: Set IN point to current preview playback position
  if (key == juce::KeyPress('i') || key == juce::KeyPress('I')) {
    if (m_previewPlayer) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();
      m_metadata.trimInSamples =
          std::clamp(currentPos, int64_t(0), m_metadata.trimOutSamples - tickInSamples);
      updateTrimInfoLabel();

      // CRITICAL: ONLY update waveform, which triggers onTrimPointsChanged callback
      // That callback handles preview player update and restart (prevents double restart)
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      DBG("ClipEditDialog: 'I' key - Set IN point to sample " << m_metadata.trimInSamples);
    }
    return true;
  }

  // O key: Set OUT point to current preview playback position
  if (key == juce::KeyPress('o') || key == juce::KeyPress('O')) {
    if (m_previewPlayer) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();
      m_metadata.trimOutSamples = std::clamp(currentPos, m_metadata.trimInSamples + tickInSamples,
                                             m_metadata.durationSamples);
      updateTrimInfoLabel();

      // CRITICAL: ONLY update waveform, which triggers onTrimPointsChanged callback
      // That callback handles preview player update (prevents double call)
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      DBG("ClipEditDialog: 'O' key - Set OUT point to sample " << m_metadata.trimOutSamples);
    }
    return true;
  }

  // J key: Jump transport (not yet implemented)
  if (key == juce::KeyPress('j') || key == juce::KeyPress('J')) {
    DBG("ClipEditDialog: 'J' key - Transport jump (not yet implemented)");
    return true;
  }

  // Issue #10: Fade time keyboard shortcuts (Edit Dialog overrides global tab shortcuts)
  // Cmd+Shift+[1-9,0] = Set OUT fade time (0.1s-0.9s, 1.0s)
  // Cmd+Opt+Shift+[1-9,0] = Set IN fade time (0.1s-0.9s, 1.0s)
  if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) &&
      key.getModifiers().isShiftDown()) {
    char keyChar = key.getTextCharacter();
    bool isAlt = key.getModifiers().isAltDown();
    double fadeTime = -1.0;
    int comboId = -1;

    // Map digit keys to fade times: 1=0.1s, 2=0.2s, ..., 9=0.9s, 0=1.0s
    if (keyChar >= '1' && keyChar <= '9') {
      fadeTime = (keyChar - '0') * 0.1; // 1→0.1s, 2→0.2s, ..., 9→0.9s
      comboId = (keyChar - '0') + 1;    // 1→ID 2, 2→ID 3, ..., 9→ID 10
    } else if (keyChar == '0') {
      fadeTime = 1.0; // 0→1.0s
      comboId = 11;   // ID 11 = 1.0s
    }

    if (fadeTime >= 0.0 && comboId > 0) {
      if (isAlt) {
        // Cmd+Opt+Shift+[1-9,0]: Set IN fade time
        m_metadata.fadeInSeconds = fadeTime;
        if (m_fadeInCombo) {
          m_fadeInCombo->setSelectedId(comboId, juce::dontSendNotification);
        }
        if (m_previewPlayer) {
          m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                    m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
        }
        DBG("ClipEditDialog: Cmd+Opt+Shift+" << keyChar << " - Set IN fade to " << fadeTime << "s");
      } else {
        // Cmd+Shift+[1-9,0]: Set OUT fade time
        m_metadata.fadeOutSeconds = fadeTime;
        if (m_fadeOutCombo) {
          m_fadeOutCombo->setSelectedId(comboId, juce::dontSendNotification);
        }
        if (m_previewPlayer) {
          m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                    m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
        }
        DBG("ClipEditDialog: Cmd+Shift+" << keyChar << " - Set OUT fade to " << fadeTime << "s");
      }
      return true; // Edit Dialog overrides global tab shortcuts
    }
  }

  // < key (comma): Nudge IN point left (BUG FIX 9) - with acceleration on hold
  // Shift modifier: 15-tick jump instead of 1-tick
  // Note: < is Shift+Comma on US keyboards
  if (key == juce::KeyPress(',') || key == juce::KeyPress('<')) {
    // Detect Shift: either explicit modifier OR the shifted key character '<'
    bool isShift = key.getModifiers().isShiftDown() || (key.getTextCharacter() == '<');

    // Perform the nudge action immediately on first key press
    // Lambda must check Shift state dynamically for timer repeats
    auto nudgeAction = [this, tickInSamples, isShift]() {
      // Recalculate jump amount based on current Shift state (for timer repeats)
      bool currentShift = juce::ModifierKeys::currentModifiers.isShiftDown();
      int64_t jumpAmount = currentShift ? (15 * tickInSamples) : tickInSamples;

      // Get current playhead position before changing IN point
      int64_t oldInPoint = m_metadata.trimInSamples;

      m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimInSamples - jumpAmount);
      if (m_metadata.trimInSamples >= m_metadata.trimOutSamples) {
        m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimOutSamples - tickInSamples);
      }
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
        // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
        restartPlayback();
        DBG("ClipEditDialog: , key - trim IN changed, restarted from IN");
      }
    };

    // Execute immediately
    nudgeAction();

    // Setup timer for repeats
    if (!m_nudgeInLeftTimer.isTimerRunning()) {
      m_nudgeInLeftTimer.onNudge = nudgeAction;
      m_nudgeInLeftTimer.startNudge(300);
    }

    DBG("ClipEditDialog: " << (isShift ? "Shift+<" : "<") << " - Nudged IN point left by "
                           << (isShift ? "15 ticks" : "1 tick") << " to sample "
                           << m_metadata.trimInSamples);
    return true;
  }

  // > key (period): Nudge IN point right (BUG FIX 9) - with acceleration on hold
  // Shift modifier: 15-tick jump instead of 1-tick
  // Note: > is Shift+Period on US keyboards
  if (key == juce::KeyPress('.') || key == juce::KeyPress('>')) {
    // Detect Shift: either explicit modifier OR the shifted key character '>'
    bool isShift = key.getModifiers().isShiftDown() || (key.getTextCharacter() == '>');

    // Perform the nudge action immediately on first key press
    // Lambda must check Shift state dynamically for timer repeats
    auto nudgeAction = [this, tickInSamples, isShift]() {
      // Recalculate jump amount based on current Shift state (for timer repeats)
      bool currentShift = juce::ModifierKeys::currentModifiers.isShiftDown();
      int64_t jumpAmount = currentShift ? (15 * tickInSamples) : tickInSamples;

      m_metadata.trimInSamples = std::min(m_metadata.trimOutSamples - tickInSamples,
                                          m_metadata.trimInSamples + jumpAmount);
      if (m_metadata.trimInSamples >= m_metadata.trimOutSamples) {
        m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimOutSamples - tickInSamples);
      }
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
        // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
        restartPlayback();
        DBG("ClipEditDialog: . key - trim IN changed, restarted from IN");
      }
    };

    // Execute immediately
    nudgeAction();

    // Setup timer for repeats
    if (!m_nudgeInRightTimer.isTimerRunning()) {
      m_nudgeInRightTimer.onNudge = nudgeAction;
      m_nudgeInRightTimer.startNudge(300);
    }

    DBG("ClipEditDialog: " << (isShift ? "Shift+>" : ">") << " - Nudged IN point right by "
                           << (isShift ? "15 ticks" : "1 tick") << " to sample "
                           << m_metadata.trimInSamples);
    return true;
  }

  // ; key: Nudge OUT point left (Issue #7) - with acceleration on hold
  // Shift modifier: 15-tick jump instead of 1-tick
  if (key == juce::KeyPress(';') || key == juce::KeyPress(':')) {
    // Detect Shift: either explicit modifier OR the shifted key character ':'
    bool isShift = key.getModifiers().isShiftDown() || (key.getTextCharacter() == ':');

    // Perform the nudge action immediately on first key press
    // Lambda must check Shift state dynamically for timer repeats
    auto nudgeAction = [this, tickInSamples, isShift]() {
      // Recalculate jump amount based on current Shift state (for timer repeats)
      bool currentShift = juce::ModifierKeys::currentModifiers.isShiftDown();
      int64_t jumpAmount = currentShift ? (15 * tickInSamples) : tickInSamples;

      m_metadata.trimOutSamples = std::max(m_metadata.trimInSamples + tickInSamples,
                                           m_metadata.trimOutSamples - jumpAmount);
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
        // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
        restartPlayback();
        DBG("ClipEditDialog: ; key - trim OUT changed, restarted from IN");
      }
    };

    // Execute immediately
    nudgeAction();

    // Setup timer for repeats
    if (!m_nudgeOutLeftTimer.isTimerRunning()) {
      m_nudgeOutLeftTimer.onNudge = nudgeAction;
      m_nudgeOutLeftTimer.startNudge(300);
    }

    DBG("ClipEditDialog: " << (isShift ? "Shift+;" : ";") << " - Nudged OUT point left by "
                           << (isShift ? "15 ticks" : "1 tick") << " to sample "
                           << m_metadata.trimOutSamples);
    return true;
  }

  // ' key: Nudge OUT point right (Issue #7) - with acceleration on hold
  // Shift modifier: 15-tick jump instead of 1-tick
  if (key == juce::KeyPress('\'') || key == juce::KeyPress('"')) {
    // Detect Shift: either explicit modifier OR the shifted key character '"'
    bool isShift = key.getModifiers().isShiftDown() || (key.getTextCharacter() == '"');

    // Perform the nudge action immediately on first key press
    // Lambda must check Shift state dynamically for timer repeats
    auto nudgeAction = [this, tickInSamples, isShift]() {
      // Recalculate jump amount based on current Shift state (for timer repeats)
      bool currentShift = juce::ModifierKeys::currentModifiers.isShiftDown();
      int64_t jumpAmount = currentShift ? (15 * tickInSamples) : tickInSamples;

      m_metadata.trimOutSamples =
          std::min(m_metadata.durationSamples, m_metadata.trimOutSamples + jumpAmount);
      if (m_metadata.trimOutSamples <= m_metadata.trimInSamples) {
        m_metadata.trimOutSamples =
            std::min(m_metadata.durationSamples, m_metadata.trimInSamples + tickInSamples);
      }
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
        // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
        restartPlayback();
        DBG("ClipEditDialog: ' key - trim OUT changed, restarted from IN");
      }
    };

    // Execute immediately
    nudgeAction();

    // Setup timer for repeats
    if (!m_nudgeOutRightTimer.isTimerRunning()) {
      m_nudgeOutRightTimer.onNudge = nudgeAction;
      m_nudgeOutRightTimer.startNudge(300);
    }

    DBG("ClipEditDialog: " << (isShift ? "Shift+'" : "'") << " - Nudged OUT point right by "
                           << (isShift ? "15 ticks" : "1 tick") << " to sample "
                           << m_metadata.trimOutSamples);
    return true;
  }

  return Component::keyPressed(key);
}

bool ClipEditDialog::keyStateChanged(bool isKeyDown) {
  // Stop acceleration timers when keys are released
  // BUG FIX 9: Updated for new trim keys (< > for IN, ; ' for OUT)
  if (!isKeyDown) {
    // Check which nudge keys are no longer held
    if (!juce::KeyPress::isKeyCurrentlyDown(',') && !juce::KeyPress::isKeyCurrentlyDown('<')) {
      m_nudgeInLeftTimer.stopNudge();
    }
    if (!juce::KeyPress::isKeyCurrentlyDown('.') && !juce::KeyPress::isKeyCurrentlyDown('>')) {
      m_nudgeInRightTimer.stopNudge();
    }
    if (!juce::KeyPress::isKeyCurrentlyDown(';') && !juce::KeyPress::isKeyCurrentlyDown(':')) {
      m_nudgeOutLeftTimer.stopNudge();
    }
    if (!juce::KeyPress::isKeyCurrentlyDown('\'') && !juce::KeyPress::isKeyCurrentlyDown('"')) {
      m_nudgeOutRightTimer.stopNudge();
    }
  }

  return Component::keyStateChanged(isKeyDown);
}
