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

  // Set color combo box based on metadata color
  if (m_colorComboBox) {
    // Map color to combo box item (simplified for now)
    if (m_metadata.color == juce::Colour(0xffe74c3c))
      m_colorComboBox->setSelectedId(1); // Red
    else if (m_metadata.color == juce::Colour(0xfff39c12))
      m_colorComboBox->setSelectedId(2); // Orange
    else if (m_metadata.color == juce::Colour(0xfff1c40f))
      m_colorComboBox->setSelectedId(3); // Yellow
    else if (m_metadata.color == juce::Colour(0xff2ecc71))
      m_colorComboBox->setSelectedId(4); // Green
    else if (m_metadata.color == juce::Colour(0xff1abc9c))
      m_colorComboBox->setSelectedId(5); // Cyan
    else if (m_metadata.color == juce::Colour(0xff3498db))
      m_colorComboBox->setSelectedId(6); // Blue
    else if (m_metadata.color == juce::Colour(0xff9b59b6))
      m_colorComboBox->setSelectedId(7); // Purple
    else if (m_metadata.color == juce::Colour(0xffff69b4))
      m_colorComboBox->setSelectedId(8); // Pink
    else
      m_colorComboBox->setSelectedId(1); // Default to Red
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
    // Map fade time to combo ID
    if (m_metadata.fadeInSeconds <= 0.05)
      m_fadeInCombo->setSelectedId(1);
    else if (m_metadata.fadeInSeconds <= 0.15)
      m_fadeInCombo->setSelectedId(2);
    else if (m_metadata.fadeInSeconds <= 0.25)
      m_fadeInCombo->setSelectedId(3);
    else if (m_metadata.fadeInSeconds <= 0.4)
      m_fadeInCombo->setSelectedId(4);
    else if (m_metadata.fadeInSeconds <= 0.75)
      m_fadeInCombo->setSelectedId(5);
    else if (m_metadata.fadeInSeconds <= 1.25)
      m_fadeInCombo->setSelectedId(6);
    else if (m_metadata.fadeInSeconds <= 1.75)
      m_fadeInCombo->setSelectedId(7);
    else if (m_metadata.fadeInSeconds <= 2.5)
      m_fadeInCombo->setSelectedId(8);
    else
      m_fadeInCombo->setSelectedId(9);
  }
  if (m_fadeOutCombo) {
    // Map fade time to combo ID
    if (m_metadata.fadeOutSeconds <= 0.05)
      m_fadeOutCombo->setSelectedId(1);
    else if (m_metadata.fadeOutSeconds <= 0.15)
      m_fadeOutCombo->setSelectedId(2);
    else if (m_metadata.fadeOutSeconds <= 0.25)
      m_fadeOutCombo->setSelectedId(3);
    else if (m_metadata.fadeOutSeconds <= 0.4)
      m_fadeOutCombo->setSelectedId(4);
    else if (m_metadata.fadeOutSeconds <= 0.75)
      m_fadeOutCombo->setSelectedId(5);
    else if (m_metadata.fadeOutSeconds <= 1.25)
      m_fadeOutCombo->setSelectedId(6);
    else if (m_metadata.fadeOutSeconds <= 1.75)
      m_fadeOutCombo->setSelectedId(7);
    else if (m_metadata.fadeOutSeconds <= 2.5)
      m_fadeOutCombo->setSelectedId(8);
    else
      m_fadeOutCombo->setSelectedId(9);
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

//==============================================================================
void ClipEditDialog::buildPhase1UI() {
  // Clip Name
  m_nameLabel = std::make_unique<juce::Label>("nameLabel", "Clip Name:");
  m_nameLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_nameLabel.get());

  m_nameEditor = std::make_unique<juce::TextEditor>();
  m_nameEditor->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::plain));
  m_nameEditor->setJustification(juce::Justification::centredLeft); // Vertically center text
  m_nameEditor->onTextChange = [this]() { m_metadata.displayName = m_nameEditor->getText(); };
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

  m_colorComboBox = std::make_unique<juce::ComboBox>();
  m_colorComboBox->addItem("Red", 1);
  m_colorComboBox->addItem("Orange", 2);
  m_colorComboBox->addItem("Yellow", 3);
  m_colorComboBox->addItem("Green", 4);
  m_colorComboBox->addItem("Cyan", 5);
  m_colorComboBox->addItem("Blue", 6);
  m_colorComboBox->addItem("Purple", 7);
  m_colorComboBox->addItem("Pink", 8);
  m_colorComboBox->onChange = [this]() {
    int colorId = m_colorComboBox->getSelectedId();
    switch (colorId) {
    case 1:
      m_metadata.color = juce::Colour(0xffe74c3c);
      break; // Red
    case 2:
      m_metadata.color = juce::Colour(0xfff39c12);
      break; // Orange
    case 3:
      m_metadata.color = juce::Colour(0xfff1c40f);
      break; // Yellow
    case 4:
      m_metadata.color = juce::Colour(0xff2ecc71);
      break; // Green
    case 5:
      m_metadata.color = juce::Colour(0xff1abc9c);
      break; // Cyan
    case 6:
      m_metadata.color = juce::Colour(0xff3498db);
      break; // Blue
    case 7:
      m_metadata.color = juce::Colour(0xff9b59b6);
      break; // Purple
    case 8:
      m_metadata.color = juce::Colour(0xffff69b4);
      break; // Pink
    }
  };
  addAndMakeVisible(m_colorComboBox.get());

  // Clip Group
  m_groupLabel = std::make_unique<juce::Label>("groupLabel", "Clip Group:");
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
      m_previewPlayer->stop();
      // Jump to IN point (start of trimmed region)
      DBG("ClipEditDialog: Skip to start (IN point)");
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
      m_previewPlayer->stop();
      // Jump to OUT point (end of trimmed region)
      DBG("ClipEditDialog: Skip to end (OUT point)");
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

  m_transportPositionLabel = std::make_unique<juce::Label>("posLabel", "00:00:00");
  m_transportPositionLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::plain));
  m_transportPositionLabel->setJustificationType(juce::Justification::centred);
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
    // Left click: Set IN point (THREE-BUTTON MOUSE MODE only)
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

    DBG("ClipEditDialog: Set IN point to sample " << newInPoint);
  };

  m_waveformDisplay->onRightClick = [this](int64_t samples) {
    // Right click: Set OUT point (THREE-BUTTON MOUSE MODE only)
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

    DBG("ClipEditDialog: Set OUT point to sample " << newOutPoint);
  };

  m_waveformDisplay->onMiddleClick = [this](int64_t samples) {
    // Click-to-jog: Jump transport to clicked position
    if (m_previewPlayer) {
      m_previewPlayer->jumpTo(samples);
      DBG("ClipEditDialog: Click-to-jog - jumped to sample " << samples);
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

    // Validate: IN must be < OUT
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

    // CRITICAL: If playhead is now before new IN point, clamp it forward to IN
    if (m_previewPlayer && m_previewPlayer->isPlaying()) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();
      if (currentPos < m_metadata.trimInSamples) {
        m_previewPlayer->jumpTo(m_metadata.trimInSamples);
        DBG("ClipEditDialog: Playhead was before new IN point - clamped to "
            << m_metadata.trimInSamples);
      }
    }
  };
  addAndMakeVisible(m_trimInTimeEditor.get());

  // Nudge buttons (< and > for rapid audition)
  m_trimInDecButton = std::make_unique<juce::TextButton>("<");
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

  m_trimInIncButton = std::make_unique<juce::TextButton>(">");
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

    // Validate: OUT must be > IN
    if (newOutPoint <= m_metadata.trimInSamples) {
      newOutPoint = std::min(m_metadata.durationSamples,
                             m_metadata.trimInSamples + (m_metadata.sampleRate / 75));
    }

    m_metadata.trimOutSamples = newOutPoint;
    updateTrimInfoLabel();

    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }

    // Update preview player
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

      // CRITICAL: If playhead is now past new OUT point, clamp it back to OUT
      if (m_previewPlayer->isPlaying()) {
        int64_t currentPos = m_previewPlayer->getCurrentPosition();
        if (currentPos > m_metadata.trimOutSamples) {
          m_previewPlayer->jumpTo(m_metadata.trimOutSamples);
          DBG("ClipEditDialog: Playhead was past new OUT point - clamped to "
              << m_metadata.trimOutSamples);
        }
      }
    }
  };
  addAndMakeVisible(m_trimOutTimeEditor.get());

  // Nudge buttons
  m_trimOutDecButton = std::make_unique<juce::TextButton>("<");
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

      // CRITICAL: If playhead is now past new OUT point, clamp it back to OUT
      // This ensures edit law enforcement (playhead must be <= OUT) when trimming during playback
      if (m_previewPlayer->isPlaying()) {
        int64_t currentPos = m_previewPlayer->getCurrentPosition();
        if (currentPos > m_metadata.trimOutSamples) {
          m_previewPlayer->jumpTo(m_metadata.trimOutSamples);
          DBG("ClipEditDialog: Playhead was past new OUT point - clamped to "
              << m_metadata.trimOutSamples);
        }
      }
    }
  };
  addAndMakeVisible(m_trimOutDecButton.get());

  m_trimOutIncButton = std::make_unique<juce::TextButton>(">");
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
  m_trimInfoLabel->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_trimInfoLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(m_trimInfoLabel.get());

  // File Info Panel (SpotOn-style yellow background)
  m_fileInfoPanel = std::make_unique<juce::Label>("fileInfoPanel", "");
  m_fileInfoPanel->setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
  m_fileInfoPanel->setJustificationType(juce::Justification::centredLeft);
  m_fileInfoPanel->setColour(juce::Label::backgroundColourId, juce::Colour(0xfffff4cc)); // Yellow
  m_fileInfoPanel->setColour(juce::Label::textColourId, juce::Colours::black);
  addAndMakeVisible(m_fileInfoPanel.get());
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
  m_fadeInCombo->addItem("0.5 s", 5);
  m_fadeInCombo->addItem("1.0 s", 6);
  m_fadeInCombo->addItem("1.5 s", 7);
  m_fadeInCombo->addItem("2.0 s", 8);
  m_fadeInCombo->addItem("3.0 s", 9);
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
      m_metadata.fadeInSeconds = 0.5;
      break;
    case 6:
      m_metadata.fadeInSeconds = 1.0;
      break;
    case 7:
      m_metadata.fadeInSeconds = 1.5;
      break;
    case 8:
      m_metadata.fadeInSeconds = 2.0;
      break;
    case 9:
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
  m_fadeOutCombo->addItem("0.5 s", 5);
  m_fadeOutCombo->addItem("1.0 s", 6);
  m_fadeOutCombo->addItem("1.5 s", 7);
  m_fadeOutCombo->addItem("2.0 s", 8);
  m_fadeOutCombo->addItem("3.0 s", 9);
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
      m_metadata.fadeOutSeconds = 0.5;
      break;
    case 6:
      m_metadata.fadeOutSeconds = 1.0;
      break;
    case 7:
      m_metadata.fadeOutSeconds = 1.5;
      break;
    case 8:
      m_metadata.fadeOutSeconds = 2.0;
      break;
    case 9:
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
  g.drawText("Edit Clip", 20, 0, 400, 50, juce::Justification::centredLeft, false);
}

void ClipEditDialog::resized() {
  // SpotOn-inspired professional grid layout
  const int GRID = 10; // 10px grid unit
  auto bounds = getLocalBounds();

  // Title bar (50px)
  bounds.removeFromTop(50);

  // Content area with padding
  auto contentArea = bounds.reduced(GRID * 2);

  // File Info Panel at very top (yellow background, SpotOn-style)
  if (m_fileInfoPanel) {
    m_fileInfoPanel->setBounds(contentArea.removeFromTop(GRID * 3));
  }
  contentArea.removeFromTop(GRID); // Spacing

  // === WAVEFORM SECTION (Prominent, SpotOn-style) ===
  if (m_waveformDisplay) {
    m_waveformDisplay->setBounds(contentArea.removeFromTop(GRID * 15)); // Larger waveform
  }
  contentArea.removeFromTop(GRID / 2); // Small spacing before zoom buttons

  // === ZOOM CONTROLS (+/- buttons with level display, centered below waveform) ===
  if (m_zoomOutButton && m_zoomLabel && m_zoomInButton) {
    auto zoomRow = contentArea.removeFromTop(GRID * 3);
    auto zoomCenter = zoomRow.withSizeKeepingCentre(GRID * 12, GRID * 3);

    m_zoomOutButton->setBounds(zoomCenter.removeFromLeft(GRID * 3));
    zoomCenter.removeFromLeft(GRID / 2);
    m_zoomLabel->setBounds(zoomCenter.removeFromLeft(GRID * 5));
    zoomCenter.removeFromLeft(GRID / 2);
    m_zoomInButton->setBounds(zoomCenter.removeFromLeft(GRID * 3));
  }
  contentArea.removeFromTop(GRID);

  // === TRANSPORT BAR (Professional, SpotOn-inspired with icons) ===
  if (m_skipToStartButton && m_playButton && m_stopButton && m_skipToEndButton && m_loopButton &&
      m_transportPositionLabel) {
    auto transportRow = contentArea.removeFromTop(GRID * 6); // Increased height for label

    // Transport buttons (top row)
    auto buttonRow = transportRow.removeFromTop(GRID * 4);
    auto transportCenter = buttonRow.withSizeKeepingCentre(GRID * 35, GRID * 4);

    // Skip to Start button (◄◄)
    m_skipToStartButton->setBounds(transportCenter.removeFromLeft(GRID * 4));
    transportCenter.removeFromLeft(GRID);

    // Loop button
    m_loopButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Play button (►) - larger, prominent
    m_playButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Stop button (■)
    m_stopButton->setBounds(transportCenter.removeFromLeft(GRID * 6));
    transportCenter.removeFromLeft(GRID);

    // Skip to End button (►►)
    m_skipToEndButton->setBounds(transportCenter.removeFromLeft(GRID * 4));

    // Transport position label (centered below buttons)
    auto labelRow = transportRow.removeFromTop(GRID * 2);
    m_transportPositionLabel->setBounds(labelRow.withSizeKeepingCentre(GRID * 15, GRID * 2));
  }
  contentArea.removeFromTop(GRID);

  // === TRIM SECTION (Grid-based, SpotOn-style) ===
  auto trimSection = contentArea.removeFromTop(GRID * 12);

  // Trim IN (left column)
  auto trimInCol = trimSection.removeFromLeft(trimSection.getWidth() / 2 - GRID);
  if (m_trimInLabel && m_trimInTimeEditor && m_trimInDecButton && m_trimInIncButton &&
      m_trimInHoldButton && m_trimInClearButton) {
    m_trimInLabel->setBounds(trimInCol.removeFromTop(GRID * 2));
    trimInCol.removeFromTop(GRID / 2);

    auto trimInRow = trimInCol.removeFromTop(GRID * 3);
    m_trimInTimeEditor->setBounds(trimInRow.removeFromLeft(GRID * 10));
    trimInRow.removeFromLeft(GRID);
    m_trimInDecButton->setBounds(trimInRow.removeFromLeft(GRID * 3));
    m_trimInIncButton->setBounds(trimInRow.removeFromLeft(GRID * 3));

    trimInCol.removeFromTop(GRID / 2);
    auto trimInButtonRow = trimInCol.removeFromTop(GRID * 3);
    m_trimInHoldButton->setBounds(
        trimInButtonRow.removeFromLeft(GRID * 5)); // SET button first (visual priority)
    trimInButtonRow.removeFromLeft(GRID);
    m_trimInClearButton->setBounds(trimInButtonRow.removeFromLeft(GRID * 5));
  }

  trimSection.removeFromLeft(GRID * 2); // Column spacing

  // Trim OUT (right column)
  auto trimOutCol = trimSection;
  if (m_trimOutLabel && m_trimOutTimeEditor && m_trimOutDecButton && m_trimOutIncButton &&
      m_trimOutHoldButton && m_trimOutClearButton) {
    m_trimOutLabel->setBounds(trimOutCol.removeFromTop(GRID * 2));
    trimOutCol.removeFromTop(GRID / 2);

    auto trimOutRow = trimOutCol.removeFromTop(GRID * 3);
    m_trimOutTimeEditor->setBounds(trimOutRow.removeFromLeft(GRID * 10));
    trimOutRow.removeFromLeft(GRID);
    m_trimOutDecButton->setBounds(trimOutRow.removeFromLeft(GRID * 3));
    m_trimOutIncButton->setBounds(trimOutRow.removeFromLeft(GRID * 3));

    trimOutCol.removeFromTop(GRID / 2);
    auto trimOutButtonRow = trimOutCol.removeFromTop(GRID * 3);
    m_trimOutHoldButton->setBounds(
        trimOutButtonRow.removeFromLeft(GRID * 5)); // SET button first (visual priority)
    trimOutButtonRow.removeFromLeft(GRID);
    m_trimOutClearButton->setBounds(trimOutButtonRow.removeFromLeft(GRID * 5));
  }

  contentArea.removeFromTop(GRID);

  // Duration label (centered)
  if (m_trimInfoLabel) {
    m_trimInfoLabel->setBounds(contentArea.removeFromTop(GRID * 2));
  }
  contentArea.removeFromTop(GRID * 2);

  // === METADATA SECTION ===
  // Clip Name
  auto nameRow = contentArea.removeFromTop(GRID * 6);
  m_nameLabel->setBounds(nameRow.removeFromTop(GRID * 2));
  m_nameEditor->setBounds(nameRow.removeFromTop(GRID * 3));
  contentArea.removeFromTop(GRID);

  // Color and Group (inline)
  auto metadataRow = contentArea.removeFromTop(GRID * 3);
  m_colorLabel->setBounds(metadataRow.removeFromLeft(GRID * 6));
  m_colorComboBox->setBounds(metadataRow.removeFromLeft(GRID * 12));
  metadataRow.removeFromLeft(GRID * 2);
  m_groupLabel->setBounds(metadataRow.removeFromLeft(GRID * 9));
  m_groupComboBox->setBounds(metadataRow.removeFromLeft(GRID * 13));
  contentArea.removeFromTop(GRID * 2);

  // === FADE SECTION ===
  auto fadeSection = contentArea.removeFromTop(GRID * 8);

  // Fade IN (left column)
  auto fadeInCol = fadeSection.removeFromLeft(fadeSection.getWidth() / 2 - GRID);
  if (m_fadeInLabel && m_fadeInCombo && m_fadeInCurveCombo) {
    m_fadeInLabel->setBounds(fadeInCol.removeFromTop(GRID * 2));
    fadeInCol.removeFromTop(GRID / 2);
    auto fadeInRow = fadeInCol.removeFromTop(GRID * 3);
    m_fadeInCombo->setBounds(fadeInRow.removeFromLeft(GRID * 10));
    fadeInRow.removeFromLeft(GRID);
    m_fadeInCurveCombo->setBounds(fadeInRow);
  }

  fadeSection.removeFromLeft(GRID * 2);

  // Fade OUT (right column)
  auto fadeOutCol = fadeSection;
  if (m_fadeOutLabel && m_fadeOutCombo && m_fadeOutCurveCombo) {
    m_fadeOutLabel->setBounds(fadeOutCol.removeFromTop(GRID * 2));
    fadeOutCol.removeFromTop(GRID / 2);
    auto fadeOutRow = fadeOutCol.removeFromTop(GRID * 3);
    m_fadeOutCombo->setBounds(fadeOutRow.removeFromLeft(GRID * 10));
    fadeOutRow.removeFromLeft(GRID);
    m_fadeOutCurveCombo->setBounds(fadeOutRow);
  }

  // Dialog buttons at bottom
  auto buttonArea = contentArea.removeFromBottom(GRID * 4);
  m_cancelButton->setBounds(buttonArea.removeFromRight(GRID * 10));
  buttonArea.removeFromRight(GRID);
  m_okButton->setBounds(buttonArea.removeFromRight(GRID * 10));
}

//==============================================================================
bool ClipEditDialog::keyPressed(const juce::KeyPress& key) {
  // ENTER key: Submit dialog (OK)
  if (key == juce::KeyPress::returnKey ||
      key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys(), 0)) {
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

  // Keyboard shortcuts (SpotOn-inspired accessibility)
  //
  // TRIM POINTS:
  //   I = Set IN point (at current transport position)
  //   O = Set OUT point (at current transport position)
  //   J = Jump transport (not yet implemented)
  //   [ = Nudge IN point left (-1 tick)
  //   ] = Nudge IN point right (+1 tick)
  //   Shift+[ = Nudge OUT point left (-1 tick)
  //   Shift+] = Nudge OUT point right (+1 tick)
  //   Option+[ / Shift+Option+[ = Jump by 1 second (IN or OUT)
  //   Option+] / Shift+Option+] = Jump by 1 second (IN or OUT)
  //
  // WAVEFORM ZOOM:
  //   Cmd/Ctrl + Plus = Zoom in (1x → 2x → 4x → 8x)
  //   Cmd/Ctrl + Minus = Zoom out (8x → 4x → 2x → 1x)
  //
  // FADE TIMES:
  //   Cmd/Ctrl + 1-5 = Set OUT fade time (0.1s, 0.2s, 0.3s, 0.5s, 1.0s)
  //   Opt+Cmd/Ctrl + 1-5 = Set IN fade time (0.1s, 0.2s, 0.3s, 0.5s, 1.0s)

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

  // Number keys with Cmd/Ctrl: Set OUT fade time (0.1-1.0s)
  // Number keys with Opt+Cmd/Ctrl: Set IN fade time (0.1-1.0s)
  // Mapping: 1=0.1s, 2=0.2s, 3=0.3s, 4=0.5s, 5=1.0s
  if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) {
    char keyChar = key.getTextCharacter();
    bool isAlt = key.getModifiers().isAltDown();
    double fadeTime = -1.0;

    // Map number keys to fade times
    if (keyChar >= '1' && keyChar <= '5') {
      switch (keyChar) {
      case '1':
        fadeTime = 0.1;
        break;
      case '2':
        fadeTime = 0.2;
        break;
      case '3':
        fadeTime = 0.3;
        break;
      case '4':
        fadeTime = 0.5;
        break;
      case '5':
        fadeTime = 1.0;
        break;
      }

      if (fadeTime >= 0.0) {
        if (isAlt) {
          // Opt+Cmd/Ctrl+N: Set IN fade time
          m_metadata.fadeInSeconds = fadeTime;
          // Update UI combo box
          if (m_fadeInCombo) {
            if (fadeTime <= 0.05)
              m_fadeInCombo->setSelectedId(1, juce::dontSendNotification);
            else if (fadeTime <= 0.15)
              m_fadeInCombo->setSelectedId(2, juce::dontSendNotification);
            else if (fadeTime <= 0.25)
              m_fadeInCombo->setSelectedId(3, juce::dontSendNotification);
            else if (fadeTime <= 0.4)
              m_fadeInCombo->setSelectedId(4, juce::dontSendNotification);
            else if (fadeTime <= 0.75)
              m_fadeInCombo->setSelectedId(5, juce::dontSendNotification);
            else if (fadeTime <= 1.25)
              m_fadeInCombo->setSelectedId(6, juce::dontSendNotification);
          }
          // Update preview player
          if (m_previewPlayer) {
            m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                      m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
          }
          DBG("ClipEditDialog: Opt+Cmd/Ctrl+" << keyChar << " - Set IN fade to " << fadeTime
                                              << "s");
        } else {
          // Cmd/Ctrl+N: Set OUT fade time
          m_metadata.fadeOutSeconds = fadeTime;
          // Update UI combo box
          if (m_fadeOutCombo) {
            if (fadeTime <= 0.05)
              m_fadeOutCombo->setSelectedId(1, juce::dontSendNotification);
            else if (fadeTime <= 0.15)
              m_fadeOutCombo->setSelectedId(2, juce::dontSendNotification);
            else if (fadeTime <= 0.25)
              m_fadeOutCombo->setSelectedId(3, juce::dontSendNotification);
            else if (fadeTime <= 0.4)
              m_fadeOutCombo->setSelectedId(4, juce::dontSendNotification);
            else if (fadeTime <= 0.75)
              m_fadeOutCombo->setSelectedId(5, juce::dontSendNotification);
            else if (fadeTime <= 1.25)
              m_fadeOutCombo->setSelectedId(6, juce::dontSendNotification);
          }
          // Update preview player
          if (m_previewPlayer) {
            m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                      m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
          }
          DBG("ClipEditDialog: Cmd/Ctrl+" << keyChar << " - Set OUT fade to " << fadeTime << "s");
        }
        return true;
      }
    }
  }

  // [ key: Nudge IN or OUT point left
  // Option modifier: jump by 1 SECOND instead of 1 tick
  // Shift modifier: control OUT point instead of IN point
  if (key == juce::KeyPress('[') || key == juce::KeyPress('{')) {
    bool isShift = key.getModifiers().isShiftDown() || key == juce::KeyPress('{');
    bool isOption = key.getModifiers().isAltDown();
    int64_t jumpAmount = isOption ? oneSecondInSamples : tickInSamples;

    if (isShift) {
      // Control OUT point (Shift+[)
      m_metadata.trimOutSamples = std::max(m_metadata.trimInSamples + tickInSamples,
                                           m_metadata.trimOutSamples - jumpAmount);
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

        // CRITICAL: If playhead is now past new OUT point, clamp it back to OUT
        if (m_previewPlayer->isPlaying()) {
          int64_t currentPos = m_previewPlayer->getCurrentPosition();
          if (currentPos > m_metadata.trimOutSamples) {
            m_previewPlayer->jumpTo(m_metadata.trimOutSamples);
            DBG("ClipEditDialog: Playhead was past new OUT point - clamped to "
                << m_metadata.trimOutSamples);
          }
        }
      }
      DBG("ClipEditDialog: Shift+[ - Nudged OUT point left by "
          << (isOption ? "1 second" : "1 tick") << " to sample " << m_metadata.trimOutSamples);
    } else {
      // Control IN point ([)
      m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimInSamples - jumpAmount);
      // Enforce edit law: IN < OUT
      if (m_metadata.trimInSamples >= m_metadata.trimOutSamples) {
        m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimOutSamples - tickInSamples);
      }
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      // IN point changes restart playback from new IN
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

        // NOTE: Decrementing IN extends range to the left, playhead cannot be before new IN
        // No need to clamp
      }
      DBG("ClipEditDialog: [ - Nudged IN point left by "
          << (isOption ? "1 second" : "1 tick") << " to sample " << m_metadata.trimInSamples);
    }
    return true;
  }

  // ] key: Nudge IN or OUT point right
  // Option modifier: jump by 1 SECOND instead of 1 tick
  // Shift modifier: control OUT point instead of IN point
  if (key == juce::KeyPress(']') || key == juce::KeyPress('}')) {
    bool isShift = key.getModifiers().isShiftDown() || key == juce::KeyPress('}');
    bool isOption = key.getModifiers().isAltDown();
    int64_t jumpAmount = isOption ? oneSecondInSamples : tickInSamples;

    if (isShift) {
      // Control OUT point (Shift+])
      m_metadata.trimOutSamples =
          std::min(m_metadata.durationSamples, m_metadata.trimOutSamples + jumpAmount);
      // Enforce edit law: OUT > IN
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
      }
      DBG("ClipEditDialog: Shift+] - Nudged OUT point right by "
          << (isOption ? "1 second" : "1 tick") << " to sample " << m_metadata.trimOutSamples);
    } else {
      // Control IN point (])
      m_metadata.trimInSamples = std::min(m_metadata.trimOutSamples - tickInSamples,
                                          m_metadata.trimInSamples + jumpAmount);
      // Enforce edit law: IN < OUT
      if (m_metadata.trimInSamples >= m_metadata.trimOutSamples) {
        m_metadata.trimInSamples = std::max(int64_t(0), m_metadata.trimOutSamples - tickInSamples);
      }
      updateTrimInfoLabel();
      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }
      // IN point changes restart playback from new IN
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

        // CRITICAL: If playhead is now before new IN point, clamp it forward to IN
        if (m_previewPlayer->isPlaying()) {
          int64_t currentPos = m_previewPlayer->getCurrentPosition();
          if (currentPos < m_metadata.trimInSamples) {
            m_previewPlayer->jumpTo(m_metadata.trimInSamples);
            DBG("ClipEditDialog: Playhead was before new IN point - clamped to "
                << m_metadata.trimInSamples);
          }
        }
      }
      DBG("ClipEditDialog: ] - Nudged IN point right by "
          << (isOption ? "1 second" : "1 tick") << " to sample " << m_metadata.trimInSamples);
    }
    return true;
  }

  return Component::keyPressed(key);
}
