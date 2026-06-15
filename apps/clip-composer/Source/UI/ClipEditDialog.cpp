// SPDX-License-Identifier: MIT

#include "ClipEditDialog.h"
#include "ClipEditDialogLayoutPolicy.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

using namespace OCC::Design;

namespace {
occ::ui::PreviewPlaybackUiSnapshot
getPreviewSnapshot(const std::unique_ptr<PreviewPlayer>& previewPlayer) {
  if (!previewPlayer)
    return {};

  return previewPlayer->getPlaybackSnapshot();
}
} // namespace

//==============================================================================
ClipEditDialog::ClipEditDialog(AudioEngine* audioEngine, int buttonIndex)
    : m_audioEngine(audioEngine), m_buttonIndex(buttonIndex) {
  // PreviewPlayer drives the edit dialog audition path and mirrors metadata changes back to the
  // clip.
  m_previewPlayer = std::make_unique<PreviewPlayer>(m_audioEngine, m_buttonIndex);

  // Keep the 75fps preview timer running while the dialog is open so this view
  // follows the shared transport state regardless of which controller changed it.
  m_previewPlayer->startPositionTimer();

  // Build Phase 1 UI (basic metadata)
  buildPhase1UI();

  // Build Phase 2 UI (In/Out points)
  buildPhase2UI();

  // Build Phase 3 UI (Fade times)
  buildPhase3UI();

  setSize(700, 880); // Expanded for all phases + larger 64px dials
}

ClipEditDialog::~ClipEditDialog() {
  // Clear callbacks before owned children are destroyed to avoid dangling UI calls.
  // The overview minimap intercepts mouse events; clearing its scrub callback before
  // teardown defends against a queued mouseDrag landing after destruction begins
  // (see 5bdbb86a for the original ASan symptom).
  if (m_waveformOverview)
    m_waveformOverview->onViewportScrubbed = nullptr;
  if (m_previewPlayer) {
    m_previewPlayer->onPositionChanged = nullptr;
    m_previewPlayer->onPlaybackStopped = nullptr;
    // The dialog is a view, so closing it must not stop the shared clip transport.
  }
}

//==============================================================================
void ClipEditDialog::setClipMetadata(const ClipMetadata& metadata) {
  m_metadata = metadata;

  // Update UI controls
  if (m_nameEditor)
    m_nameEditor->setText(m_metadata.displayName, false);

  if (m_filePathEditor)
    m_filePathEditor->setText(m_metadata.filePath, false);

  if (m_groupSelector)
    m_groupSelector->setSelectedGroup(m_metadata.clipGroup);

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

  // Set initial transport button colors
  updateTransportButtonColors();

  const auto previewSnapshot = getPreviewSnapshot(m_previewPlayer);
  const bool wasAlreadyPlaying = previewSnapshot.isPlaying;
  DBG("ClipEditDialog::setClipMetadata() - Clip was "
      << (wasAlreadyPlaying ? "ALREADY PLAYING" : "stopped") << " when dialog opened");

  // Load waveform display and preview player
  if (m_waveformDisplay && m_metadata.filePath.isNotEmpty()) {
    juce::File audioFile(m_metadata.filePath);
    if (audioFile.existsAsFile()) {
      m_waveformDisplay->setAudioFile(audioFile);
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples > 0
                                                                     ? m_metadata.trimOutSamples
                                                                     : m_metadata.durationSamples);

      // Also feed the full-file overview minimap.
      if (m_waveformOverview) {
        juce::AudioFormatManager fmtMgr;
        fmtMgr.registerBasicFormats();
        if (auto* reader = fmtMgr.createReaderFor(audioFile)) {
          m_waveformOverview->setAudioFile(reader);
          m_waveformOverview->setTrimSamples(
              m_metadata.trimInSamples, m_metadata.trimOutSamples > 0 ? m_metadata.trimOutSamples
                                                                      : m_metadata.durationSamples);

          // Wire up minimap scrubbing callback.
          // SafePointer guards against the queued-mouse-event-after-destruction
          // scenario that caused the ASan crash fixed in 5bdbb86a — if the dialog
          // tears down while a drag is mid-flight, the lambda no-ops cleanly.
          juce::Component::SafePointer<ClipEditDialog> safeSelf(this);
          m_waveformOverview->onViewportScrubbed = [safeSelf](int64_t startSample,
                                                              int64_t endSample) {
            auto* self = safeSelf.getComponent();
            if (self == nullptr || self->m_waveformDisplay == nullptr)
              return;
            const float duration = static_cast<float>(self->m_metadata.durationSamples);
            if (duration <= 0.0f)
              return;
            const float startNorm = static_cast<float>(startSample) / duration;
            const float endNorm = static_cast<float>(endSample) / duration;
            const float center = (startNorm + endNorm) * 0.5f;
            self->m_waveformDisplay->setZoomCenter(center);
            const float viewportWidthNorm = endNorm - startNorm;
            if (viewportWidthNorm > 0.0f) {
              const float targetZoom = 1.0f / viewportWidthNorm;
              int targetLevel = 0;
              if (targetZoom >= 8.0f)
                targetLevel = 4;
              else if (targetZoom >= 4.0f)
                targetLevel = 3;
              else if (targetZoom >= 2.0f)
                targetLevel = 2;
              else if (targetZoom >= 1.0f)
                targetLevel = 1;
              self->m_waveformDisplay->setZoomLevel(targetLevel, center);
            }
          };
          delete reader;
        }
      }

      // Configure preview player without breaking shared playback continuity.
      if (m_previewPlayer) {
        m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples > 0
                                                                     ? m_metadata.trimOutSamples
                                                                     : m_metadata.durationSamples);
        m_previewPlayer->setFades(m_metadata.fadeInSeconds, m_metadata.fadeOutSeconds,
                                  m_metadata.fadeInCurve, m_metadata.fadeOutCurve);
      }
    }
  }

  if (m_previewPlayer) {
    m_previewPlayer->setAuditionSource(m_metadata.filePath);
  }

  // Sync loop button to metadata
  if (m_loopButton) {
    m_loopButton->setToggleState(m_metadata.loopEnabled, juce::dontSendNotification);
  }

  // Sync preview player loop state
  if (m_previewPlayer) {
    m_previewPlayer->setLoopEnabled(m_metadata.loopEnabled);
  }

  if (wasAlreadyPlaying && m_previewPlayer) {
    m_previewPlayer->startPositionTimer();
    DBG("ClipEditDialog::setClipMetadata() - Started position timer (clip was already playing)");
  }

  // Sync Stop Others button to metadata
  if (m_stopOthersButton) {
    m_stopOthersButton->setToggleState(m_metadata.stopOthersEnabled, juce::dontSendNotification);
  }

  // Sync FLAGS chips (the user-facing controls in the primary mockup anatomy).
  if (m_loopChip)
    m_loopChip->setToggleState(m_metadata.loopEnabled, juce::dontSendNotification);
  if (m_fadeInChip)
    m_fadeInChip->setToggleState(m_metadata.fadeInSeconds > 0.0, juce::dontSendNotification);
  if (m_fadeOutChip)
    m_fadeOutChip->setToggleState(m_metadata.fadeOutSeconds > 0.0, juce::dontSendNotification);
  if (m_stopOthersChip)
    m_stopOthersChip->setToggleState(m_metadata.stopOthersEnabled, juce::dontSendNotification);

  // Phase 3: Initialize fade combos using lookup table
  if (m_fadeInCombo) {
    m_fadeInCombo->setSelectedId(mapFadeTimeToComboId(m_metadata.fadeInSeconds));
  }
  if (m_fadeOutCombo) {
    m_fadeOutCombo->setSelectedId(mapFadeTimeToComboId(m_metadata.fadeOutSeconds));
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
  const auto previewSnapshot = getPreviewSnapshot(m_previewPlayer);
  if (!previewSnapshot.isPlaying)
    return; // Only enforce during playback

  const int64_t currentPos = previewSnapshot.currentPositionSamples;

  if (currentPos >= m_metadata.trimOutSamples) {
    // Playhead is at or past new OUT point - jump to IN and restart
    m_previewPlayer->play(); // Restarts from IN point
    DBG("ClipEditDialog: OUT point edit law enforced - playhead was >= OUT ("
        << currentPos << " >= " << m_metadata.trimOutSamples << "), jumped to IN and restarted");
  }
}

void ClipEditDialog::restartPlayback() {
  // Restart playback from current IN point (used for edit law enforcement)
  if (getPreviewSnapshot(m_previewPlayer).isPlaying && m_previewPlayer) {
    m_previewPlayer->play(); // play() restarts from IN point
  }
}

//==============================================================================
// Fade time ↔ combo ID mapping lookup tables
// Combo IDs: 1=0.0s, 2=0.1s, ..., 11=1.0s, 12=1.2s, 13=1.6s, 14=2.0s, 15=2.4s, 16=3.0s

int ClipEditDialog::mapFadeTimeToComboId(double fadeSeconds) {
  // Thresholds: midpoint between consecutive values
  static constexpr double kThresholds[] = {0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75,
                                           0.85, 0.95, 1.1,  1.4,  1.8,  2.2,  2.7};

  for (int i = 0; i < 15; ++i) {
    if (fadeSeconds <= kThresholds[i])
      return i + 1; // Combo IDs are 1-indexed
  }
  return 16; // 3.0s or greater
}

double ClipEditDialog::mapComboIdToFadeTime(int comboId) {
  // Direct mapping from combo ID to fade time in seconds
  static constexpr double kFadeTimes[] = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7,
                                          0.8, 0.9, 1.0, 1.2, 1.6, 2.0, 2.4, 3.0};

  if (comboId < 1 || comboId > 16)
    return 0.0;
  return kFadeTimes[comboId - 1]; // Convert 1-indexed to 0-indexed
}

//==============================================================================
void ClipEditDialog::updateAdvancedDisclosureIcon() {
  if (!m_advancedDisclosureButton)
    return;

  juce::Path chevronPath;
  if (m_advancedExpanded) {
    // Up chevron (expanded): M7 14l5 -5 5 5
    chevronPath.startNewSubPath(0.29f, 0.58f);
    chevronPath.lineTo(0.5f, 0.21f);
    chevronPath.lineTo(0.71f, 0.58f);
  } else {
    // Down chevron (collapsed): M7 10l5 5 5 -5
    chevronPath.startNewSubPath(0.29f, 0.42f);
    chevronPath.lineTo(0.5f, 0.79f);
    chevronPath.lineTo(0.71f, 0.42f);
  }

  auto chevronIcon = std::make_unique<juce::DrawablePath>();
  chevronIcon->setPath(chevronPath);
  chevronIcon->setStrokeFill(juce::Colour(OCC::Design::kTextSecondary));
  chevronIcon->setStrokeThickness(0.083f);
  chevronIcon->setFill(juce::Colours::transparentBlack);
  m_advancedDisclosureButton->setImages(chevronIcon.get());
  chevronIcon.release();
  m_advancedDisclosureButton->repaint();
}

//==============================================================================
void ClipEditDialog::buildPhase1UI() {
  // Clip Name
  m_nameLabel = std::make_unique<juce::Label>("nameLabel", "Clip Name:");
  m_nameLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_nameLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_nameLabel.get());

  m_nameEditor = std::make_unique<juce::TextEditor>();
  m_nameEditor->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::plain));
  m_nameEditor->setJustification(juce::Justification::centredLeft); // Vertically center text
  m_nameEditor->setMultiLine(false);                                // Single line only
  m_nameEditor->setScrollBarThickness(0);                           // No scrollbar
  m_nameEditor->setScrollToShowCursor(true);                        // Scroll to keep cursor visible
  m_nameEditor->onTextChange = [this]() {
    m_metadata.displayName = m_nameEditor->getText();
    // Title bar paints from m_metadata.displayName — repaint so the operator
    // sees the title track their typing.
    repaint();
  };
  m_nameEditor->setReturnKeyStartsNewLine(false); // Enter should NOT insert newline
  m_nameEditor->onReturnKey = [this]() {
    // Enter key = Confirm value and blur field (remove focus)
    // This allows user to confirm text without closing dialog
    m_nameEditor->giveAwayKeyboardFocus();
    DBG("ClipEditDialog: Name editor confirmed, focus cleared");
  };
  addAndMakeVisible(m_nameEditor.get());

  // File Path (read-only) - Note: These are NOT added as visible since file info
  // is shown in m_fileInfoPanel. Keeping for potential future use.
  m_filePathLabel = std::make_unique<juce::Label>("filePathLabel", "File Path:");
  m_filePathLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  // NOT visible - file info shown in m_fileInfoPanel

  m_filePathEditor = std::make_unique<juce::TextEditor>();
  m_filePathEditor->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
  m_filePathEditor->setReadOnly(true);
  m_filePathEditor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(kBgInset));
  // NOT visible - file info shown in m_fileInfoPanel

  // Color
  m_colorLabel = std::make_unique<juce::Label>("colorLabel", "Color:");
  m_colorLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_colorLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
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
  m_groupLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_groupLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_groupLabel.get());

  m_groupSelector = std::make_unique<GroupSelector>();
  m_groupSelector->setSelectedGroup(m_metadata.clipGroup);
  m_groupSelector->onGroupChanged = [this](int group) { m_metadata.clipGroup = group; };
  addAndMakeVisible(m_groupSelector.get());

  // Dialog buttons
  // OK uses the Console primary (Neve blue gradient via LookAndFeel button paint).
  m_okButton = std::make_unique<juce::TextButton>("OK");
  m_okButton->setColour(juce::TextButton::buttonColourId, juce::Colour(kNeveBlue));
  m_okButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(kNeveBlue).brighter(0.2f));
  m_okButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
  m_okButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  m_okButton->onClick = [this]() {
    if (onOkClicked)
      onOkClicked(m_metadata);
  };
  addAndMakeVisible(m_okButton.get());

  // Cancel uses the Console matte grey (default variant).
  m_cancelButton = std::make_unique<juce::TextButton>("Cancel");
  m_cancelButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff383d40));
  m_cancelButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff45494c));
  m_cancelButton->onClick = [this]() {
    if (onCancelClicked)
      onCancelClicked();
  };
  addAndMakeVisible(m_cancelButton.get());

  // FLAGS chips — design-kit chip-style toggles, real focusable components.
  // Each chip syncs its state to the metadata field and (for Loop / Stop Others)
  // bridges to the legacy juce::Button so existing keyboard shortcuts /
  // callbacks still fire.
  m_loopChip = std::make_unique<ConsoleChipButton>("loopChip", "Loop");
  m_loopChip->setToggleState(m_metadata.loopEnabled, juce::dontSendNotification);
  m_loopChip->onClick = [this]() {
    const bool enabled = m_loopChip->getToggleState();
    m_metadata.loopEnabled = enabled;
    if (m_loopButton && m_loopButton->getToggleState() != enabled)
      m_loopButton->setToggleState(enabled, juce::sendNotification);
  };
  addAndMakeVisible(m_loopChip.get());

  m_fadeInChip = std::make_unique<ConsoleChipButton>("fadeInChip", "Fade In");
  m_fadeInChip->setToggleState(m_metadata.fadeInSeconds > 0.0, juce::dontSendNotification);
  m_fadeInChip->onClick = [this]() {
    // Chip enables a default 0.5 s fade when toggled on, clears it when off.
    if (m_fadeInChip->getToggleState()) {
      if (m_metadata.fadeInSeconds <= 0.0)
        m_metadata.fadeInSeconds = 0.5;
    } else {
      m_metadata.fadeInSeconds = 0.0;
    }
    if (m_fadeInCombo)
      m_fadeInCombo->setSelectedId(mapFadeTimeToComboId(m_metadata.fadeInSeconds),
                                   juce::sendNotification);
  };
  addAndMakeVisible(m_fadeInChip.get());

  m_fadeOutChip = std::make_unique<ConsoleChipButton>("fadeOutChip", "Fade Out");
  m_fadeOutChip->setToggleState(m_metadata.fadeOutSeconds > 0.0, juce::dontSendNotification);
  m_fadeOutChip->onClick = [this]() {
    if (m_fadeOutChip->getToggleState()) {
      if (m_metadata.fadeOutSeconds <= 0.0)
        m_metadata.fadeOutSeconds = 0.5;
    } else {
      m_metadata.fadeOutSeconds = 0.0;
    }
    if (m_fadeOutCombo)
      m_fadeOutCombo->setSelectedId(mapFadeTimeToComboId(m_metadata.fadeOutSeconds),
                                    juce::sendNotification);
  };
  addAndMakeVisible(m_fadeOutChip.get());

  m_stopOthersChip = std::make_unique<ConsoleChipButton>("stopOthersChip", "Stop Others");
  m_stopOthersChip->setToggleState(m_metadata.stopOthersEnabled, juce::dontSendNotification);
  m_stopOthersChip->onClick = [this]() {
    const bool enabled = m_stopOthersChip->getToggleState();
    m_metadata.stopOthersEnabled = enabled;
    if (m_stopOthersButton && m_stopOthersButton->getToggleState() != enabled)
      m_stopOthersButton->setToggleState(enabled, juce::sendNotification);
  };
  addAndMakeVisible(m_stopOthersChip.get());

  // Action triad (AUDITION / REPLACE FILE / CLEAR) — design-kit spec
  // These are ConsoleActionButton components with Ghost variant (no fill, text only)
  // to match the mockup's unobtrusive action row.
  m_auditionActionButton =
      std::make_unique<ConsoleActionButton>("auditionAction", ConsoleActionButton::Variant::Ghost);
  m_auditionActionButton->setLabel("AUDITION");
  m_auditionActionButton->onClick = [this]() {
    if (onAuditionClicked)
      onAuditionClicked();
    // Also trigger playback via preview player for immediate audition
    if (m_previewPlayer)
      m_previewPlayer->play();
  };
  addAndMakeVisible(m_auditionActionButton.get());

  m_replaceFileActionButton = std::make_unique<ConsoleActionButton>(
      "replaceFileAction", ConsoleActionButton::Variant::Ghost);
  m_replaceFileActionButton->setLabel("REPLACE FILE");
  m_replaceFileActionButton->onClick = [this]() {
    if (onReplaceFileClicked)
      onReplaceFileClicked();
  };
  addAndMakeVisible(m_replaceFileActionButton.get());

  m_clearActionButton =
      std::make_unique<ConsoleActionButton>("clearAction", ConsoleActionButton::Variant::Danger);
  m_clearActionButton->setLabel("CLEAR");
  m_clearActionButton->onClick = [this]() {
    if (onClearClicked)
      onClearClicked();
  };
  addAndMakeVisible(m_clearActionButton.get());

  // Advanced section disclosure button (chevron up/down next to ADVANCED eyebrow)
  m_advancedDisclosureButton = std::make_unique<juce::DrawableButton>(
      "advancedDisclosure", juce::DrawableButton::ImageFitted);
  {
    juce::Path chevronPath;
    // Up chevron (expanded state): M7 14l5 -5 5 5
    chevronPath.startNewSubPath(0.29f, 0.58f);
    chevronPath.lineTo(0.5f, 0.21f);
    chevronPath.lineTo(0.71f, 0.58f);
    auto chevronIcon = std::make_unique<juce::DrawablePath>();
    chevronIcon->setPath(chevronPath);
    chevronIcon->setStrokeFill(juce::Colour(OCC::Design::kTextSecondary));
    chevronIcon->setStrokeThickness(0.083f); // 2px in normalized coords
    chevronIcon->setFill(juce::Colours::transparentBlack);
    m_advancedDisclosureButton->setImages(chevronIcon.get());
    chevronIcon.release();
  }
  m_advancedDisclosureButton->setColour(juce::DrawableButton::backgroundColourId,
                                        juce::Colours::transparentBlack);
  m_advancedDisclosureButton->setColour(juce::DrawableButton::backgroundOnColourId,
                                        juce::Colour(OCC::Design::kBgComponent));
  m_advancedDisclosureButton->onClick = [this]() {
    m_advancedExpanded = !m_advancedExpanded;
    updateAdvancedDisclosureIcon();
    resized(); // Re-layout to show/hide advanced section
  };
  addAndMakeVisible(m_advancedDisclosureButton.get());
}

void ClipEditDialog::buildPhase2UI() {
  // Waveform Overview (full-file minimap, design-kit "overview minimap" pattern)
  m_waveformOverview = std::make_unique<WaveformOverview>();
  addAndMakeVisible(m_waveformOverview.get());

  // Waveform Display (zoomed main view)
  m_waveformDisplay = std::make_unique<WaveformDisplay>();
  addAndMakeVisible(m_waveformDisplay.get());

  // Preview Transport Controls (Professional icon-based buttons inspired by SpotOn and Merging
  // Ovation)

  // Skip to Start button - Tabler 'player-skip-back' icon
  // SVG: <path d="M20 5v14l-12 -7z" /> <path d="M4 5l0 14" />
  m_skipToStartButton =
      std::make_unique<juce::DrawableButton>("SkipToStart", juce::DrawableButton::ImageFitted);
  {
    juce::Path skipToStartPath;

    // Convert from Tabler's 24×24 viewBox to normalized 0-1 coordinates
    auto norm = [](float val) { return val / 24.0f; };

    // Triangle: M20 5 v14 l-12 -7 z
    skipToStartPath.startNewSubPath(norm(20), norm(5));
    skipToStartPath.lineTo(norm(20), norm(19)); // v14 (vertical 14 down)
    skipToStartPath.lineTo(norm(8), norm(12));  // l-12 -7 (relative: -12 horizontal, -7 vertical)
    skipToStartPath.closeSubPath();

    // Vertical bar: M4 5 l0 14
    skipToStartPath.startNewSubPath(norm(4), norm(5));
    skipToStartPath.lineTo(norm(4), norm(19)); // l0 14 (vertical line down 14)

    // Scale down by 33% (to 67% size) and center within button
    float scale = 0.67f;
    float offset = (1.0f - scale) / 2.0f; // Center offset
    skipToStartPath.applyTransform(juce::AffineTransform::scale(scale).translated(offset, offset));

    auto skipToStartIcon = std::make_unique<juce::DrawablePath>();
    skipToStartIcon->setPath(skipToStartPath);
    skipToStartIcon->setStrokeFill(juce::Colours::white);
    skipToStartIcon->setStrokeThickness(norm(2.0f)); // stroke-width="2" from SVG
    skipToStartIcon->setFill(juce::Colours::transparentBlack);
    m_skipToStartButton->setImages(skipToStartIcon.get());
    skipToStartIcon.release(); // DrawableButton takes ownership
  }
  m_skipToStartButton->setColour(juce::DrawableButton::backgroundColourId,
                                 juce::Colour(kBgComponent));
  m_skipToStartButton->setColour(juce::DrawableButton::backgroundOnColourId,
                                 juce::Colour(kBgComponent).brighter(0.15f));
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

  // Play button - Tabler 'player-play' icon
  // SVG: <path d="M7 4v16l13 -8z" />
  m_playButton = std::make_unique<juce::DrawableButton>("Play", juce::DrawableButton::ImageFitted);
  {
    juce::Path playPath;

    // Convert from Tabler's 24×24 viewBox to normalized 0-1 coordinates
    auto norm = [](float val) { return val / 24.0f; };

    // Triangle: M7 4 v16 l13 -8 z
    playPath.startNewSubPath(norm(7), norm(4));
    playPath.lineTo(norm(7), norm(20));  // v16 (vertical 16 down)
    playPath.lineTo(norm(20), norm(12)); // l13 -8 (relative: +13 horizontal, -8 vertical)
    playPath.closeSubPath();

    // Scale down by 33% (to 67% size) and center within button
    float scale = 0.67f;
    float offset = (1.0f - scale) / 2.0f; // Center offset
    playPath.applyTransform(juce::AffineTransform::scale(scale).translated(offset, offset));

    auto playIcon = std::make_unique<juce::DrawablePath>();
    playIcon->setPath(playPath);
    playIcon->setStrokeFill(juce::Colours::white);
    playIcon->setStrokeThickness(norm(2.0f)); // stroke-width="2" from SVG
    playIcon->setFill(juce::Colours::transparentBlack);
    m_playButton->setImages(playIcon.get());
    playIcon.release(); // DrawableButton takes ownership
  }
  m_playButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(kAccentGreen));
  m_playButton->setColour(juce::DrawableButton::backgroundOnColourId,
                          juce::Colour(kAccentGreen).brighter(0.15f));
  m_playButton->onClick = [this]() {
    if (m_previewPlayer) {
      // Play button ALWAYS restarts from IN point (that's its purpose)
      m_previewPlayer->play();

      // CRITICAL: Immediately update waveform playhead to IN point (don't wait for 75fps timer)
      // This ensures visual feedback is instant when PLAY is pressed
      if (m_waveformDisplay) {
        m_waveformDisplay->setPlayheadPosition(m_metadata.trimInSamples);
      }

      // CRITICAL: Force 75fps timer to start for playhead updates
      // AudioEngine::isClipPlaying() stub returns false, causing timer to stop immediately
      // In edit mode, we ALWAYS want visual feedback regardless of audio engine state
      m_previewPlayer->startPositionTimer();

      // Update button colors to show active state
      updateTransportButtonColors();

      DBG("ClipEditDialog: Play button - restarted from IN point, forced timer start");
    }
  };
  addAndMakeVisible(m_playButton.get());

  // Stop button - Tabler 'square' icon (scaled down 20% to match other icons)
  // SVG: <path d="M4.8 4.8m0 1.6a1.6 1.6 0 0 1 1.6 -1.6h11.2a1.6 1.6 0 0 1 1.6 1.6v11.2a1.6 1.6 0 0
  // 1 -1.6 1.6h-11.2a1.6 1.6 0 0 1 -1.6 -1.6z" />
  m_stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ImageFitted);
  {
    juce::Path stopPath;

    // Convert from Tabler's 24×24 viewBox to normalized 0-1 coordinates
    auto norm = [](float val) { return val / 24.0f; };

    // Rounded square path (already scaled down 20% in SVG)
    // Starting at (4.8, 4.8), size 14.4×14.4, corner radius 1.6
    float x = norm(4.8f), y = norm(4.8f);
    float width = norm(14.4f), height = norm(14.4f);
    float radius = norm(1.6f);

    stopPath.addRoundedRectangle(x, y, width, height, radius);

    // Scale down by 33% (to 67% size) and center within button
    float scale = 0.67f;
    float offset = (1.0f - scale) / 2.0f; // Center offset
    stopPath.applyTransform(juce::AffineTransform::scale(scale).translated(offset, offset));

    auto stopIcon = std::make_unique<juce::DrawablePath>();
    stopIcon->setPath(stopPath);
    stopIcon->setStrokeFill(juce::Colours::white);
    stopIcon->setStrokeThickness(norm(2.0f)); // stroke-width="2" from SVG
    stopIcon->setFill(juce::Colours::transparentBlack);
    m_stopButton->setImages(stopIcon.get());
    stopIcon.release(); // DrawableButton takes ownership
  }
  m_stopButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(kMeterRed));
  m_stopButton->setColour(juce::DrawableButton::backgroundOnColourId,
                          juce::Colour(kMeterRed).brighter(0.15f));
  m_stopButton->onClick = [this]() {
    if (!m_previewPlayer)
      return;

    // Simple logic: If already stopped, reset playhead to IN. Otherwise, stop.
    if (!m_previewPlayer->isPlaying()) {
      // Already stopped - reset playhead to IN point (remains stopped)
      int64_t trimIn = m_metadata.trimInSamples;

      // Update waveform display only (don't start playback)
      if (m_waveformDisplay) {
        m_waveformDisplay->setPlayheadPosition(trimIn);
      }

      DBG("ClipEditDialog: STOP while stopped - reset playhead to IN (" << trimIn << " samples)");
    } else {
      // Playing - stop playback
      m_previewPlayer->stop();
      DBG("ClipEditDialog: Preview playback stopped");
    }

    // Update button colors immediately (instant visual feedback)
    updateTransportButtonColors();
  };
  addAndMakeVisible(m_stopButton.get());

  // Skip to End button - Tabler 'player-skip-forward' icon
  // SVG: <path d="M4 5v14l12 -7z" /> <path d="M20 5l0 14" />
  m_skipToEndButton =
      std::make_unique<juce::DrawableButton>("SkipToEnd", juce::DrawableButton::ImageFitted);
  {
    juce::Path skipToEndPath;

    // Convert from Tabler's 24×24 viewBox to normalized 0-1 coordinates
    auto norm = [](float val) { return val / 24.0f; };

    // Triangle: M4 5 v14 l12 -7 z
    skipToEndPath.startNewSubPath(norm(4), norm(5));
    skipToEndPath.lineTo(norm(4), norm(19));  // v14 (vertical 14 down)
    skipToEndPath.lineTo(norm(16), norm(12)); // l12 -7 (relative: +12 horizontal, -7 vertical)
    skipToEndPath.closeSubPath();

    // Vertical bar: M20 5 l0 14
    skipToEndPath.startNewSubPath(norm(20), norm(5));
    skipToEndPath.lineTo(norm(20), norm(19)); // l0 14 (vertical line down 14)

    // Scale down by 33% (to 67% size) and center within button
    float scale = 0.67f;
    float offset = (1.0f - scale) / 2.0f; // Center offset
    skipToEndPath.applyTransform(juce::AffineTransform::scale(scale).translated(offset, offset));

    auto skipToEndIcon = std::make_unique<juce::DrawablePath>();
    skipToEndIcon->setPath(skipToEndPath);
    skipToEndIcon->setStrokeFill(juce::Colours::white);
    skipToEndIcon->setStrokeThickness(norm(2.0f)); // stroke-width="2" from SVG
    skipToEndIcon->setFill(juce::Colours::transparentBlack);
    m_skipToEndButton->setImages(skipToEndIcon.get());
    skipToEndIcon.release(); // DrawableButton takes ownership
  }
  m_skipToEndButton->setColour(juce::DrawableButton::backgroundColourId,
                               juce::Colour(kBgComponent));
  m_skipToEndButton->setColour(juce::DrawableButton::backgroundOnColourId,
                               juce::Colour(kBgComponent).brighter(0.15f));
  m_skipToEndButton->onClick = [this]() {
    if (m_previewPlayer) {
      // Issue #9: Jump to 2 seconds before OUT point (keep play/pause state)
      bool wasPlaying = m_previewPlayer->isPlaying();

      // Calculate target: 2 seconds before OUT, or IN if clip < 2s
      int64_t twoSecondsInSamples = 2 * m_metadata.sampleRate;
      int64_t targetPosition =
          std::max(m_metadata.trimInSamples, m_metadata.trimOutSamples - twoSecondsInSamples);

      m_previewPlayer->jumpTo(targetPosition);

      // Set audition region for visual feedback (yellow highlight from playhead to OUT)
      if (m_waveformDisplay) {
        m_waveformDisplay->setAuditionRegion(targetPosition, m_metadata.trimOutSamples);
      }

      // If was playing, resume playing; if paused, stay paused
      if (wasPlaying && !m_previewPlayer->isPlaying()) {
        m_previewPlayer->play();
      }

      DBG("ClipEditDialog: Skip to end (2s before OUT) - " << (wasPlaying ? "resumed" : "paused"));
    }
  };
  addAndMakeVisible(m_skipToEndButton.get());

  // Loop button - Tabler 'repeat' icon
  // SVG: <path d="M4 12v-3a3 3 0 0 1 3 -3h13m-3 -3l3 3l-3 3" />
  //      <path d="M20 12v3a3 3 0 0 1 -3 3h-13m3 3l-3 -3l3 -3" />
  m_loopButton = std::make_unique<juce::DrawableButton>("Loop", juce::DrawableButton::ImageFitted);
  {
    juce::Path loopPath;

    // Convert from Tabler's 24×24 viewBox to normalized 0-1 coordinates
    auto norm = [](float val) { return val / 24.0f; };

    // Top path: M4 12 v-3 a3 3 0 0 1 3 -3 h13
    loopPath.startNewSubPath(norm(4), norm(12));
    loopPath.lineTo(norm(4), norm(9)); // v-3 (vertical -3 up)

    // Arc: a3 3 0 0 1 3 -3 (elliptical arc, radius 3x3, end point +3 horizontal, -3 vertical)
    loopPath.quadraticTo(norm(4), norm(6), norm(7),
                         norm(6)); // Approximate arc with quadratic bezier

    loopPath.lineTo(norm(20), norm(6)); // h13 (horizontal 13 right)

    // Top arrow: m-3 -3 l3 3 l-3 3
    loopPath.startNewSubPath(norm(17), norm(3));
    loopPath.lineTo(norm(20), norm(6));
    loopPath.lineTo(norm(17), norm(9));

    // Bottom path: M20 12 v3 a3 3 0 0 1 -3 3 h-13
    loopPath.startNewSubPath(norm(20), norm(12));
    loopPath.lineTo(norm(20), norm(15)); // v3 (vertical 3 down)

    // Arc: a3 3 0 0 1 -3 3 (elliptical arc, radius 3x3, end point -3 horizontal, +3 vertical)
    loopPath.quadraticTo(norm(20), norm(18), norm(17),
                         norm(18)); // Approximate arc with quadratic bezier

    loopPath.lineTo(norm(4), norm(18)); // h-13 (horizontal -13 left)

    // Bottom arrow: m3 3 l-3 -3 l3 -3
    loopPath.startNewSubPath(norm(7), norm(21));
    loopPath.lineTo(norm(4), norm(18));
    loopPath.lineTo(norm(7), norm(15));

    // Scale down by 33% (to 67% size) and center within button
    float scale = 0.67f;
    float offset = (1.0f - scale) / 2.0f; // Center offset
    loopPath.applyTransform(juce::AffineTransform::scale(scale).translated(offset, offset));

    auto loopIcon = std::make_unique<juce::DrawablePath>();
    loopIcon->setPath(loopPath);
    loopIcon->setStrokeFill(juce::Colours::white);
    loopIcon->setStrokeThickness(norm(2.0f)); // stroke-width="2" from SVG
    loopIcon->setFill(juce::Colours::transparentBlack);

    m_loopButton->setImages(loopIcon.get());
    loopIcon.release(); // DrawableButton takes ownership
  }
  m_loopButton->setClickingTogglesState(true); // Enable toggle mode
  m_loopButton->setColour(juce::DrawableButton::backgroundColourId, juce::Colour(kBgComponent));
  m_loopButton->setColour(juce::DrawableButton::backgroundOnColourId,
                          juce::Colour(kNeveBlue)); // Neve blue when active (loop enabled)
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
  m_transportPositionLabel->setFont(juce::FontOptions(
      "HK Grotesk", kFont3XL, juce::Font::bold)); // 24pt - Transport Time hierarchy
  m_transportPositionLabel->setJustificationType(juce::Justification::centred);
  m_transportPositionLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_transportPositionLabel.get());

  // Wire up preview player callbacks
  if (m_previewPlayer) {
    m_previewPlayer->onPositionChanged = [this](int64_t samplePosition) {
      // Update position label
      if (m_transportPositionLabel) {
        juce::String timeString = samplesToTimeString(samplePosition, m_metadata.sampleRate);
        m_transportPositionLabel->setText(timeString, juce::dontSendNotification);
      }

      // Update transport button colors based on playback state (75fps atomic sync)
      updateTransportButtonColors();

      // Update waveform playhead
      if (m_waveformDisplay) {
        m_waveformDisplay->setPlayheadPosition(samplePosition);

        // Clear audition highlight when:
        // 1. Playhead reaches OUT (end of audition)
        // 2. Playhead jumps backward (loop restart detected)
        bool reachedOut = (samplePosition >= m_metadata.trimOutSamples);
        bool loopJump =
            (samplePosition < m_previousPlayheadPosition - 1000); // Backward jump > 1000 samples

        if (reachedOut || loopJump) {
          m_waveformDisplay->clearAuditionRegion();
        }

        m_previousPlayheadPosition = samplePosition;
      }
    };

    m_previewPlayer->onPlaybackStopped = [this]() {
      // Clear audition region when playback stops
      if (m_waveformDisplay) {
        m_waveformDisplay->clearAuditionRegion();
      }

      // Update transport button colors when playback stops
      updateTransportButtonColors();

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

    m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

    // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      restartPlayback();
    }

    DBG("ClipEditDialog: Cmd+Click - Set IN point to sample " << newInPoint
                                                              << ", restarted from IN");
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

    m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);

    // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      restartPlayback();
    }

    DBG("ClipEditDialog: Cmd+Shift+Click - Set OUT point to sample " << newOutPoint
                                                                     << ", restarted from IN");
  };

  m_waveformDisplay->onMiddleClick = [this](int64_t samples) {
    // Click-to-jog: Jump transport to clicked position
    // CRITICAL: Clamp to [IN, OUT] bounds to prevent playhead from escaping trim range
    // FEATURE: jumpTo() automatically starts playback if clip is stopped (seamless UX)
    if (m_previewPlayer) {
      int64_t clampedSamples =
          std::clamp(samples, m_metadata.trimInSamples, m_metadata.trimOutSamples);

      // jumpTo() handles both cases:
      // - If playing: Seeks to position (gap-free)
      // - If stopped: Seeks to position AND starts playback
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

    // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
    if (m_previewPlayer) {
      m_previewPlayer->setTrimPoints(inSamples, outSamples);
      restartPlayback();
      DBG("ClipEditDialog: Handle drag - trim points changed to ["
          << inSamples << ", " << outSamples << "], restarted from IN");
    }
  };

  // Zoom controls: +/- buttons (4 levels: 1x, 2x, 4x, 8x)
  // Zoom always centers on playhead position for intuitive navigation
  m_zoomOutButton = std::make_unique<juce::TextButton>("-");
  m_zoomOutButton->setColour(juce::TextButton::buttonColourId, juce::Colour(kBgComponent));
  m_zoomOutButton->setColour(juce::TextButton::buttonOnColourId,
                             juce::Colour(kBgComponent).brighter(0.15f));
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
  m_zoomLabel->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
  m_zoomLabel->setJustificationType(juce::Justification::centred);
  m_zoomLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_zoomLabel.get());

  m_zoomInButton = std::make_unique<juce::TextButton>("+");
  m_zoomInButton->setColour(juce::TextButton::buttonColourId, juce::Colour(kBgComponent));
  m_zoomInButton->setColour(juce::TextButton::buttonOnColourId,
                            juce::Colour(kBgComponent).brighter(0.15f));
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
  m_trimInLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_trimInLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_trimInLabel.get());

  // Time editor (MM:SS:FF - SpotOn format, 75fps)
  m_trimInTimeEditor = std::make_unique<juce::TextEditor>();
  m_trimInTimeEditor->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
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

    // Clear focus after confirming value
    m_trimInTimeEditor->giveAwayKeyboardFocus();
    DBG("ClipEditDialog: Trim IN time editor confirmed, focus cleared");
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
  m_trimInHoldButton->setColour(juce::TextButton::buttonColourId, juce::Colour(kNeveBlue));
  m_trimInHoldButton->setColour(juce::TextButton::buttonOnColourId,
                                juce::Colour(kNeveBlue).brighter(0.15f));
  m_trimInHoldButton->onClick = [this]() {
    if (m_previewPlayer) {
      int64_t currentPos = m_previewPlayer->getCurrentPosition();

      // Enforce edit law: IN must be < OUT
      if (currentPos >= m_metadata.trimOutSamples) {
        currentPos = std::max(int64_t(0), m_metadata.trimOutSamples - (m_metadata.sampleRate / 75));
      }

      m_metadata.trimInSamples = currentPos;
      updateTrimInfoLabel();

      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      restartPlayback();

      DBG("ClipEditDialog: SET - Set IN point to current position " << currentPos
                                                                    << ", restarted from IN");
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
  m_trimOutLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_trimOutLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_trimOutLabel.get());

  // Time editor (MM:SS:FF - SpotOn format, 75fps)
  m_trimOutTimeEditor = std::make_unique<juce::TextEditor>();
  m_trimOutTimeEditor->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
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

    // Clear focus after confirming value
    m_trimOutTimeEditor->giveAwayKeyboardFocus();
    DBG("ClipEditDialog: Trim OUT time editor confirmed, focus cleared");
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
  m_trimOutHoldButton->setColour(juce::TextButton::buttonColourId, juce::Colour(kNeveBlue));
  m_trimOutHoldButton->setColour(juce::TextButton::buttonOnColourId,
                                 juce::Colour(kNeveBlue).brighter(0.15f));
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
  m_trimInfoLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_trimInfoLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  m_trimInfoLabel->setColour(juce::Label::backgroundColourId, juce::Colour(kBgInset));
  m_trimInfoLabel->setColour(juce::Label::outlineColourId, juce::Colour(kBorderDefault));
  m_trimInfoLabel->setJustificationType(juce::Justification::centred);
  addAndMakeVisible(m_trimInfoLabel.get());

  // File Info Panel (Neo-vintage console surface, subtle not jarring)
  m_fileInfoPanel = std::make_unique<juce::Label>("fileInfoPanel", "");
  m_fileInfoPanel->setFont(juce::FontOptions("HK Grotesk", kFontXS, juce::Font::plain));
  m_fileInfoPanel->setJustificationType(juce::Justification::centredLeft);
  m_fileInfoPanel->setColour(juce::Label::backgroundColourId, juce::Colour(kBgSurface));
  m_fileInfoPanel->setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));
  m_fileInfoPanel->setColour(juce::Label::outlineColourId, juce::Colour(kBorderDefault));
  addAndMakeVisible(m_fileInfoPanel.get());

  // Gain Control (Feature 5: -30dB to +10dB, persisted through the clip metadata path)
  m_gainLabel = std::make_unique<juce::Label>("gainLabel", "Gain:");
  m_gainLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_gainLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  addAndMakeVisible(m_gainLabel.get());

  // Create rotary dial instead of horizontal slider
  m_gainSlider =
      std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
  m_gainSlider->setRange(-30.0, 10.0, 0.1); // -30dB to +10dB in 0.1dB steps (finer control)
  m_gainSlider->setValue(0.0);              // Default 0dB
  m_gainSlider->setDoubleClickReturnValue(true, 0.0); // Double-click resets to 0dB
  m_gainSlider->onValueChange = [this]() {
    double gain = m_gainSlider->getValue();
    m_gainValueLabel->setText(juce::String(gain, 1) + " dB", juce::dontSendNotification);
    m_metadata.gainDb = gain;
  };
  addAndMakeVisible(m_gainSlider.get());

  // Text input field for gain (below the dial)
  m_gainValueLabel = std::make_unique<juce::Label>("gainValueLabel", "0.0 dB");
  m_gainValueLabel->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
  m_gainValueLabel->setJustificationType(juce::Justification::centred);
  m_gainValueLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  m_gainValueLabel->setEditable(true);
  m_gainValueLabel->onTextChange = [this]() {
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

  // Deferred pitch placeholder: visible for layout continuity, but disabled until
  // AudioEngine exposes real pitch-shift support.
  m_placeholderLabel = std::make_unique<juce::Label>("placeholderLabel", "Pitch (deferred):");
  m_placeholderLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_placeholderLabel->setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));
  addAndMakeVisible(m_placeholderLabel.get());

  m_placeholderDial =
      std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
  m_placeholderDial->setRange(-12.0, 12.0, 0.1);
  m_placeholderDial->setValue(0.0);
  m_placeholderDial->setDoubleClickReturnValue(true, 0.0);
  m_placeholderDial->setEnabled(false);
  m_placeholderDial->setTooltip(
      "Pitch shifting is deferred until AudioEngine pitch support lands.");
  addAndMakeVisible(m_placeholderDial.get());

  m_placeholderValueLabel = std::make_unique<juce::Label>("placeholderValueLabel", "Deferred");
  m_placeholderValueLabel->setFont(juce::FontOptions("HK Grotesk", kFontSM, juce::Font::plain));
  m_placeholderValueLabel->setJustificationType(juce::Justification::centred);
  m_placeholderValueLabel->setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));
  m_placeholderValueLabel->setEditable(false);
  m_placeholderValueLabel->setTooltip(
      "Pitch shifting is deferred until AudioEngine pitch support lands.");
  addAndMakeVisible(m_placeholderValueLabel.get());
}

void ClipEditDialog::buildPhase3UI() {
  // Fade In Section
  m_fadeInLabel = std::make_unique<juce::Label>("fadeInLabel", "Fade In:");
  m_fadeInLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_fadeInLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
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
    m_metadata.fadeInSeconds = mapComboIdToFadeTime(m_fadeInCombo->getSelectedId());
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
  m_fadeOutLabel->setFont(juce::FontOptions("HK Grotesk", kFontMD, juce::Font::bold));
  m_fadeOutLabel->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
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
    m_metadata.fadeOutSeconds = mapComboIdToFadeTime(m_fadeOutCombo->getSelectedId());
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
  // Console chassis background.
  g.fillAll(juce::Colour(kBgPrimary));

  // Outer border with Neve accent.
  auto bounds = getLocalBounds().toFloat();
  g.setColour(juce::Colour(kNeveBlue).withAlpha(0.6f));
  g.drawRoundedRectangle(bounds.reduced(1.0f), kRadiusLG, kBorderMedium);

  // Inner shadow / depth.
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.drawRoundedRectangle(bounds.reduced(3.0f), kRadiusMD, kBorderThin);

  // Title bar — mockup spec: eyebrow ("EDIT CLIP") + bold title + clip index (mono, muted).
  auto titleBar = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), 50.0f);
  g.setColour(juce::Colour(kBgSecondary));
  g.fillRect(titleBar);
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawHorizontalLine(50, 0.0f, static_cast<float>(getWidth()));

  // Eyebrow.
  g.setColour(juce::Colour(kTextSecondary));
  g.setFont(OCC::Console::monoFont(10.0f, juce::Font::plain));
  g.drawText("EDIT CLIP", 20, 6, 200, 14, juce::Justification::centredLeft, false);

  // Bold title.
  g.setColour(juce::Colour(kTextPrimary));
  g.setFont(OCC::Console::consoleFont(20.0f, juce::Font::bold));
  g.drawText(m_metadata.displayName.isNotEmpty() ? m_metadata.displayName
                                                 : juce::String("New Clip"),
             20, 20, getWidth() - 160, 26, juce::Justification::centredLeft, false);

  // Clip index (mono, right-aligned in title bar).
  if (m_buttonIndex >= 0) {
    g.setColour(juce::Colour(kTextSecondary));
    g.setFont(OCC::Console::monoFont(11.0f, juce::Font::plain));
    g.drawText("#" + juce::String(m_buttonIndex + 1).paddedLeft('0', 3), getWidth() - 110, 18, 90,
               18, juce::Justification::centredRight, false);
  }

  // ----- SECTION EYEBROWS -----
  // Mirror the same vertical accounting used by resized() so the painted
  // eyebrow text lands exactly above each control band.
  const int GRID = 10;
  constexpr int kEyebrowHeight = 14;
  constexpr int kEyebrowGap = 4;
  constexpr int kSectionGap = GRID * 2;

  int y = 50 + GRID * 2; // outer bounds.removeFromTop(50) + content padding (GRID * 2)

  auto drawEyebrow = [&](const char* text) {
    g.setColour(juce::Colour(kTextSecondary));
    g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
    g.drawText(text, GRID * 2, y, getWidth() - GRID * 4, kEyebrowHeight,
               juce::Justification::centredLeft, false);
    y += kEyebrowHeight + kEyebrowGap;
  };

  // WAVEFORM block — eyebrow over the minimap + zoomed view + toolbar trio.
  drawEyebrow("WAVEFORM");
  y += 24; // overview minimap height
  y += GRID / 2;
  y += GRID * 11; // zoomed waveform height
  y += GRID / 2;
  y += GRID * 4; // transport toolbar height
  y += kSectionGap;

  // NAME
  drawEyebrow("NAME");
  y += GRID * 3; // m_nameEditor height
  y += kSectionGap;

  // GROUP + COLOUR (two-up eyebrows side by side)
  {
    g.setColour(juce::Colour(kTextSecondary));
    g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
    const int halfWidth = (getWidth() - GRID * 4 - kSectionGap) / 2;
    g.drawText("GROUP", GRID * 2, y, halfWidth, kEyebrowHeight, juce::Justification::centredLeft,
               false);
    g.drawText("COLOUR", GRID * 2 + halfWidth + kSectionGap, y, halfWidth, kEyebrowHeight,
               juce::Justification::centredLeft, false);
    y += kEyebrowHeight + kEyebrowGap;
  }
  y += GRID * 3;
  y += kSectionGap;

  // TRIM / FADE — per-cell eyebrows over the 2x2 grid.
  // Top row: TRIM IN / TRIM OUT above the time editors.
  {
    g.setColour(juce::Colour(kTextSecondary));
    g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
    const int halfWidth = (getWidth() - GRID * 4 - kSectionGap) / 2;
    g.drawText("TRIM IN", GRID * 2, y, halfWidth, kEyebrowHeight, juce::Justification::centredLeft,
               false);
    g.drawText("TRIM OUT", GRID * 2 + halfWidth + kSectionGap, y, halfWidth, kEyebrowHeight,
               juce::Justification::centredLeft, false);
    y += kEyebrowHeight + kEyebrowGap;
  }
  y += GRID * 3; // top row height
  y += GRID;     // row gap
  // Bottom row: FADE IN / FADE OUT above the fade combos.
  {
    g.setColour(juce::Colour(kTextSecondary));
    g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
    const int halfWidth = (getWidth() - GRID * 4 - kSectionGap) / 2;
    g.drawText("FADE IN", GRID * 2, y, halfWidth, kEyebrowHeight, juce::Justification::centredLeft,
               false);
    g.drawText("FADE OUT", GRID * 2 + halfWidth + kSectionGap, y, halfWidth, kEyebrowHeight,
               juce::Justification::centredLeft, false);
    y += kEyebrowHeight + kEyebrowGap;
  }
  y += GRID * 3; // bottom row height
  y += kSectionGap;

  // FLAGS
  drawEyebrow("FLAGS");
  y += GRID * 3;
  y += kSectionGap;

  // ADVANCED
  drawEyebrow("ADVANCED");
  y += occ::clip_edit::advancedSectionContentHeight(m_advancedExpanded, GRID);
  y += kSectionGap;

  // ACTIONS (for the action triad at bottom)
  drawEyebrow("ACTIONS");
  (void)y;
}

void ClipEditDialog::resized() {
  // OCC149 phase 5b.6 — mockup-anatomy layout.
  //
  // Operator narrative: a sound designer / producer trimming and tagging a
  // clip. The dialog is read top-to-bottom in mockup order:
  //   1. Title bar (eyebrow + name + clip #)
  //   2. Waveform with scrubbing controls
  //   3. NAME field
  //   4. GROUP (A/B/C/D routing channel) + COLOUR (visual identifier swatch)
  //   5. 2x2 trim/fade grid — Trim In · Trim Out / Fade In · Fade Out
  //   6. FLAGS — Loop · Fade In · Fade Out · Stop Others (existing toggles,
  //      restyled inline)
  //   7. ADVANCED — gain dial, deferred pitch dial, transport scrub, SET/CLR/
  //      nudge buttons (the legacy controls that don't belong in the primary
  //      anatomy but the operator still needs).
  //   8. Action row — OK (commit) and Cancel.
  //
  // The mockup's AUDITION/REPLACE/CLEAR action triad now maps to concrete
  // operator workflows:
  //   - AUDITION overlaps with the transport play button (same behaviour).
  //   - REPLACE FILE delegates to MainComponent's file chooser + reload path.
  //   - CLEAR is wired to MainComponent::onClearClicked, which mirrors the
  //     right-click "Remove Clip?" flow through UndoManager so undo history
  //     stays coherent across entry points.
  //
  // Eyebrow labels are painted in paint(); the legacy juce::Label "Clip
  // Name:" / "Trim In:" etc. are hidden so they don't double up.

  const int GRID = 10; // 10px grid unit (legacy convention)
  auto bounds = getLocalBounds();

  // Hide the legacy text labels — eyebrows are painted into paint() instead.
  for (auto* label :
       {m_nameLabel.get(), m_filePathLabel.get(), m_colorLabel.get(), m_groupLabel.get(),
        m_trimInLabel.get(), m_trimOutLabel.get(), m_gainLabel.get(), m_placeholderLabel.get(),
        m_fadeInLabel.get(), m_fadeOutLabel.get()}) {
    if (label)
      label->setVisible(false);
  }

  // Title bar (50px) — paint() handles eyebrow + name + clip #.
  bounds.removeFromTop(50);

  // Content area with padding.
  auto contentArea = bounds.reduced(GRID * 2);

  constexpr int kEyebrowHeight = 14;     // Painted text band
  constexpr int kEyebrowGap = 4;         // Gap below eyebrow before its control
  constexpr int kSectionGap = GRID * 2;  // Vertical breathing room between sections
  constexpr int kFieldHeight = GRID * 3; // Inset field / button row height
  (void)kFieldHeight;                    // Used implicitly via consistent removeFromTop arguments

  // ----- WAVEFORM BLOCK -----
  // Three-tier design-kit pattern:
  //   * 24 px overview minimap (full-file scrub)
  //   * 110 px zoomed waveform (main editing surface, with amplitude scale)
  //   * 36 px transport toolbar (skip / play / stop / skip + zoom + position)
  // Transport sits immediately under the waveform per operator narrative:
  // the sound designer scrubs the zoomed view and reaches straight down for
  // play / zoom / skip. They never have to hunt across the dialog.
  contentArea.removeFromTop(kEyebrowHeight); // eyebrow band (painted)
  contentArea.removeFromTop(kEyebrowGap);
  if (m_waveformOverview)
    m_waveformOverview->setBounds(contentArea.removeFromTop(24));
  contentArea.removeFromTop(GRID / 2);
  if (m_waveformDisplay)
    m_waveformDisplay->setBounds(contentArea.removeFromTop(GRID * 11));
  contentArea.removeFromTop(GRID / 2);
  // Transport toolbar — restored to prominence directly beneath the waveform.
  {
    auto toolbar = contentArea.removeFromTop(GRID * 4);
    // Three zones: left transport cluster, centre time readout, right zoom cluster.
    constexpr int kIconBtn = 30;
    constexpr int kGapInner = 4;
    auto leftZone = toolbar.removeFromLeft(kIconBtn * 5 + kGapInner * 4 + GRID);
    auto rightZone = toolbar.removeFromRight(GRID * 18);
    auto centreZone = toolbar; // remaining middle

    // Left cluster: ⏮ ▶ ⏹ ⏭ ↻ (skip start, play, stop, skip end, loop)
    auto leftRow = leftZone;
    if (m_skipToStartButton)
      m_skipToStartButton->setBounds(leftRow.removeFromLeft(kIconBtn));
    leftRow.removeFromLeft(kGapInner);
    if (m_playButton)
      m_playButton->setBounds(leftRow.removeFromLeft(kIconBtn));
    leftRow.removeFromLeft(kGapInner);
    if (m_stopButton)
      m_stopButton->setBounds(leftRow.removeFromLeft(kIconBtn));
    leftRow.removeFromLeft(kGapInner);
    if (m_skipToEndButton)
      m_skipToEndButton->setBounds(leftRow.removeFromLeft(kIconBtn));

    // Centre: position readout label (e.g., "00:00:00.59 / 00:00:30.00").
    if (m_transportPositionLabel)
      m_transportPositionLabel->setBounds(centreZone.withSizeKeepingCentre(GRID * 18, GRID * 3));

    // Right cluster: zoom controls.
    auto rightRow = rightZone;
    if (m_zoomOutButton)
      m_zoomOutButton->setBounds(rightRow.removeFromLeft(GRID * 3));
    rightRow.removeFromLeft(kGapInner);
    if (m_zoomLabel)
      m_zoomLabel->setBounds(rightRow.removeFromLeft(GRID * 5));
    rightRow.removeFromLeft(kGapInner);
    if (m_zoomInButton)
      m_zoomInButton->setBounds(rightRow.removeFromLeft(GRID * 3));
  }
  contentArea.removeFromTop(kSectionGap);

  // ----- NAME -----
  contentArea.removeFromTop(kEyebrowHeight);
  contentArea.removeFromTop(kEyebrowGap);
  if (m_nameEditor) {
    m_nameEditor->setBounds(contentArea.removeFromTop(GRID * 3));
  }
  contentArea.removeFromTop(kSectionGap);

  // ----- GROUP + COLOUR (two-up row) -----
  // Mockup uses two separate eyebrows on the same row; both controls hang
  // below their respective eyebrows. Group on the left, Colour on the right.
  contentArea.removeFromTop(kEyebrowHeight);
  contentArea.removeFromTop(kEyebrowGap);
  {
    auto row = contentArea.removeFromTop(GRID * 3);
    const int halfWidth = (row.getWidth() - kSectionGap) / 2;
    auto groupArea = row.removeFromLeft(halfWidth);
    row.removeFromLeft(kSectionGap);
    auto colourArea = row;
    if (m_groupSelector)
      m_groupSelector->setBounds(groupArea);
    if (m_colorSwatchPicker)
      m_colorSwatchPicker->setBounds(colourArea);
  }
  contentArea.removeFromTop(kSectionGap);

  // ----- 2x2 TRIM / FADE GRID -----
  // Per-cell eyebrows (TRIM IN / TRIM OUT / FADE IN / FADE OUT) painted in
  // paint(); resized() walks the same arithmetic to reserve their bands.
  //   eyebrow band → TRIM IN / TRIM OUT row
  //   row gap
  //   eyebrow band → FADE IN / FADE OUT row
  contentArea.removeFromTop(kEyebrowHeight);
  contentArea.removeFromTop(kEyebrowGap);
  {
    constexpr int kCellHeight = GRID * 3;
    const int halfWidth = (contentArea.getWidth() - kSectionGap) / 2;

    auto topRow = contentArea.removeFromTop(kCellHeight);
    if (m_trimInTimeEditor)
      m_trimInTimeEditor->setBounds(topRow.removeFromLeft(halfWidth));
    topRow.removeFromLeft(kSectionGap);
    if (m_trimOutTimeEditor)
      m_trimOutTimeEditor->setBounds(topRow);

    contentArea.removeFromTop(GRID); // row gap

    // Reserve eyebrow band for FADE IN / FADE OUT before laying out the combos.
    contentArea.removeFromTop(kEyebrowHeight);
    contentArea.removeFromTop(kEyebrowGap);

    auto bottomRow = contentArea.removeFromTop(kCellHeight);
    if (m_fadeInCombo)
      m_fadeInCombo->setBounds(bottomRow.removeFromLeft(halfWidth));
    bottomRow.removeFromLeft(kSectionGap);
    if (m_fadeOutCombo)
      m_fadeOutCombo->setBounds(bottomRow);
  }
  contentArea.removeFromTop(kSectionGap);

  // ----- FLAGS -----
  // Four design-kit ConsoleChipButton instances (real focusable components):
  // Loop · Fade In · Fade Out · Stop Others. Amber-tinted when set, inset
  // when not. The legacy m_loopButton / m_stopOthersButton are hidden but
  // still wired so existing keyboard shortcuts continue to work.
  contentArea.removeFromTop(kEyebrowHeight);
  contentArea.removeFromTop(kEyebrowGap);
  {
    auto row = contentArea.removeFromTop(GRID * 3);
    const int chipGap = GRID;
    const int chipWidth = (row.getWidth() - chipGap * 3) / 4;
    if (m_loopChip) {
      m_loopChip->setBounds(row.removeFromLeft(chipWidth));
      row.removeFromLeft(chipGap);
    }
    if (m_fadeInChip) {
      m_fadeInChip->setBounds(row.removeFromLeft(chipWidth));
      row.removeFromLeft(chipGap);
    }
    if (m_fadeOutChip) {
      m_fadeOutChip->setBounds(row.removeFromLeft(chipWidth));
      row.removeFromLeft(chipGap);
    }
    if (m_stopOthersChip) {
      m_stopOthersChip->setBounds(row);
    }
  }
  // Hide the legacy toggle controls — they remain as model bridges only.
  if (m_loopButton)
    m_loopButton->setVisible(false);
  if (m_stopOthersButton)
    m_stopOthersButton->setVisible(false);
  contentArea.removeFromTop(kSectionGap);

  auto setAdvancedVisible = [this](bool shouldBeVisible) {
    for (auto* component : {static_cast<juce::Component*>(m_gainSlider.get()),
                            static_cast<juce::Component*>(m_gainValueLabel.get()),
                            static_cast<juce::Component*>(m_placeholderDial.get()),
                            static_cast<juce::Component*>(m_placeholderValueLabel.get()),
                            static_cast<juce::Component*>(m_trimInfoLabel.get()),
                            static_cast<juce::Component*>(m_trimInHoldButton.get()),
                            static_cast<juce::Component*>(m_trimInDecButton.get()),
                            static_cast<juce::Component*>(m_trimInIncButton.get()),
                            static_cast<juce::Component*>(m_trimInClearButton.get()),
                            static_cast<juce::Component*>(m_trimOutHoldButton.get()),
                            static_cast<juce::Component*>(m_trimOutDecButton.get()),
                            static_cast<juce::Component*>(m_trimOutIncButton.get()),
                            static_cast<juce::Component*>(m_trimOutClearButton.get()),
                            static_cast<juce::Component*>(m_fadeInCurveCombo.get()),
                            static_cast<juce::Component*>(m_fadeOutCurveCombo.get())}) {
      if (component)
        component->setVisible(shouldBeVisible);
    }
  };

  // ----- ADVANCED -----
  // Transport buttons and zoom now live in the waveform toolbar above. The
  // Advanced section only carries the truly secondary controls: gain dial,
  // deferred pitch, SET/CLR/nudge for sample-accurate trim, fade-curve combos,
  // and the trim duration readout. Collapsing hides those real components (not
  // just their layout space) so keyboard focus cannot land on parked controls.
  contentArea.removeFromTop(kEyebrowHeight);
  contentArea.removeFromTop(kEyebrowGap);
  setAdvancedVisible(m_advancedExpanded);

  if (m_advancedExpanded) {
    // Sub-row: Gain · Pitch dials on the left, trim duration readout on the right.
    const int dialSize = occ::clip_edit::kAdvancedDialSizePx;
    {
      auto advRow = contentArea.removeFromTop(dialSize + GRID * 2);
      const int knobBlockW = dialSize + GRID * 2;

      if (m_gainSlider && m_gainValueLabel) {
        auto block = advRow.removeFromLeft(knobBlockW);
        auto dial = block.removeFromTop(dialSize).withSizeKeepingCentre(dialSize, dialSize);
        m_gainSlider->setBounds(dial);
        m_gainValueLabel->setBounds(block.withSizeKeepingCentre(GRID * 6, GRID * 2));
      }
      advRow.removeFromLeft(GRID);

      if (m_placeholderDial && m_placeholderValueLabel) {
        auto block = advRow.removeFromLeft(knobBlockW);
        auto dial = block.removeFromTop(dialSize).withSizeKeepingCentre(dialSize, dialSize);
        m_placeholderDial->setBounds(dial);
        m_placeholderValueLabel->setBounds(block.withSizeKeepingCentre(GRID * 6, GRID * 2));
      }

      // Trim duration readout — pinned to the right side of this row.
      if (m_trimInfoLabel) {
        auto readout = advRow.removeFromRight(GRID * 24).withSizeKeepingCentre(GRID * 22, GRID * 3);
        m_trimInfoLabel->setBounds(readout);
      }
    }
    contentArea.removeFromTop(GRID);

    // Sub-row: SET · < · > · CLR for trim-in (left) and trim-out (right).
    if (m_trimInHoldButton && m_trimInDecButton && m_trimInIncButton && m_trimInClearButton &&
        m_trimOutHoldButton && m_trimOutDecButton && m_trimOutIncButton && m_trimOutClearButton) {
      auto row = contentArea.removeFromTop(GRID * 3);
      const int halfWidth = (row.getWidth() - kSectionGap) / 2;
      auto leftHalf = row.removeFromLeft(halfWidth);
      row.removeFromLeft(kSectionGap);
      auto rightHalf = row;

      const int btnW = (leftHalf.getWidth() - GRID * 3) / 4;
      m_trimInHoldButton->setBounds(leftHalf.removeFromLeft(btnW));
      leftHalf.removeFromLeft(GRID);
      m_trimInDecButton->setBounds(leftHalf.removeFromLeft(btnW));
      leftHalf.removeFromLeft(GRID);
      m_trimInIncButton->setBounds(leftHalf.removeFromLeft(btnW));
      leftHalf.removeFromLeft(GRID);
      m_trimInClearButton->setBounds(leftHalf.removeFromLeft(btnW));

      const int btnW2 = (rightHalf.getWidth() - GRID * 3) / 4;
      m_trimOutHoldButton->setBounds(rightHalf.removeFromLeft(btnW2));
      rightHalf.removeFromLeft(GRID);
      m_trimOutDecButton->setBounds(rightHalf.removeFromLeft(btnW2));
      rightHalf.removeFromLeft(GRID);
      m_trimOutIncButton->setBounds(rightHalf.removeFromLeft(btnW2));
      rightHalf.removeFromLeft(GRID);
      m_trimOutClearButton->setBounds(rightHalf.removeFromLeft(btnW2));
    }

    // Sub-row: fade curve combos for FADE IN / FADE OUT.
    if (m_fadeInCurveCombo && m_fadeOutCurveCombo) {
      contentArea.removeFromTop(GRID);
      auto curveRow = contentArea.removeFromTop(GRID * 3);
      const int halfWidth = (curveRow.getWidth() - kSectionGap) / 2;
      m_fadeInCurveCombo->setBounds(curveRow.removeFromLeft(halfWidth));
      curveRow.removeFromLeft(kSectionGap);
      m_fadeOutCurveCombo->setBounds(curveRow);
    }
  }

  // Hide the FileInfoPanel / FilePathEditor for the v0.2.3 anatomy — the
  // mockup's source-file card is a separate row we'll add in a future pass.
  if (m_fileInfoPanel)
    m_fileInfoPanel->setVisible(false);
  if (m_filePathEditor)
    m_filePathEditor->setVisible(false);

  // ----- ACTION ROW: Action triad (left) + OK / Cancel (right) pinned to dialog bottom -----
  auto footer = bounds.removeFromBottom(GRID * 5); // pulled from outer bounds, not contentArea
  footer.reduce(GRID * 2, GRID);

  // Left side: Action triad (AUDITION / REPLACE FILE / CLEAR)
  if (m_auditionActionButton && m_replaceFileActionButton && m_clearActionButton) {
    const int actionBtnW = GRID * 9;
    const int actionGap = GRID;
    m_auditionActionButton->setBounds(footer.removeFromLeft(actionBtnW));
    footer.removeFromLeft(actionGap);
    m_replaceFileActionButton->setBounds(footer.removeFromLeft(actionBtnW));
    footer.removeFromLeft(actionGap);
    m_clearActionButton->setBounds(footer.removeFromLeft(actionBtnW));
  }

  // Right side: OK / Cancel
  if (m_okButton && m_cancelButton) {
    m_cancelButton->setBounds(footer.removeFromRight(GRID * 10));
    footer.removeFromRight(GRID);
    m_okButton->setBounds(footer.removeFromRight(GRID * 10));
  }
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

  // TAB key: Always cycles through three text fields (Name → IN → OUT → Name)
  // Works regardless of what has focus currently
  if (key == juce::KeyPress::tabKey) {
    auto* focused = juce::Component::getCurrentlyFocusedComponent();

    if (focused == m_nameEditor.get()) {
      // Name → Trim IN
      m_trimInTimeEditor->grabKeyboardFocus();
      DBG("ClipEditDialog: TAB - Name → Trim IN");
    } else if (focused == m_trimInTimeEditor.get()) {
      // Trim IN → Trim OUT
      m_trimOutTimeEditor->grabKeyboardFocus();
      DBG("ClipEditDialog: TAB - Trim IN → Trim OUT");
    } else if (focused == m_trimOutTimeEditor.get()) {
      // Trim OUT → Name (cycle back)
      m_nameEditor->grabKeyboardFocus();
      DBG("ClipEditDialog: TAB - Trim OUT → Name");
    } else {
      // Nothing focused or other control → start at Name
      // This allows TAB to work from anywhere in dialog
      m_nameEditor->grabKeyboardFocus();
      DBG("ClipEditDialog: TAB - Other → Name (cycle start)");
    }
    return true; // Consume Tab key
  }

  // ENTER key: Two-stage behavior
  // Stage 1: In text field → blur field (remove focus)
  // Stage 2: No text field focused → trigger OK button
  // CRITICAL SAFETY: Prevent accidental triggering of show-critical controls (CLEAR, etc.)
  if (key == juce::KeyPress::returnKey ||
      key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys(), 0)) {
    auto* focused = juce::Component::getCurrentlyFocusedComponent();

    // Case 1: Text editor has focus - let it handle Enter (confirms text, blurs field)
    if (focused == m_nameEditor.get() || focused == m_trimInTimeEditor.get() ||
        focused == m_trimOutTimeEditor.get()) {
      // Let text editor handle Enter (will blur via onReturnKey callback)
      return false;
    }

    // Case 2: Dangerous component has focus - block Enter completely
    // CRITICAL: Prevent accidental CLEAR button triggers
    if (focused == m_trimInClearButton.get() || focused == m_trimOutClearButton.get()) {
      DBG("ClipEditDialog: ENTER blocked - dangerous component has focus");
      return true; // Consume Enter key without action
    }

    // Case 3: Safe to trigger OK (either nothing focused or safe component focused)
    if (onOkClicked) {
      onOkClicked(m_metadata);
      DBG("ClipEditDialog: ENTER - triggered OK button");
    }
    return true;
  }

  // ESC key: Cancel dialog
  if (key == juce::KeyPress::escapeKey) {
    if (onCancelClicked)
      onCancelClicked();
    return true;
  }

  // ? key (Shift+/): Toggle Loop (Issue #4)
  if (key.getTextCharacter() == '?') {
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

  // R key: Zoom Out
  if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R') {
    if (m_waveformDisplay) {
      int currentLevel = m_waveformDisplay->getZoomLevel();
      if (currentLevel > 0) {
        // Calculate playhead position for zoom center
        float playheadNormalized = 0.5f; // Default to center
        if (m_previewPlayer && m_metadata.durationSamples > 0) {
          int64_t playheadPos = m_previewPlayer->getCurrentPosition();
          playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
        }
        m_waveformDisplay->setZoomLevel(currentLevel - 1, playheadNormalized);
        updateZoomLabel();
        DBG("ClipEditDialog: R key - Zoom out to level " << (currentLevel - 1));
      }
    }
    return true;
  }

  // T key: Zoom In
  if (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T') {
    if (m_waveformDisplay) {
      int currentLevel = m_waveformDisplay->getZoomLevel();
      if (currentLevel < 4) { // Max zoom level is 4 (16x)
        // Calculate playhead position for zoom center
        float playheadNormalized = 0.5f; // Default to center
        if (m_previewPlayer && m_metadata.durationSamples > 0) {
          int64_t playheadPos = m_previewPlayer->getCurrentPosition();
          playheadNormalized = static_cast<float>(playheadPos) / m_metadata.durationSamples;
        }
        m_waveformDisplay->setZoomLevel(currentLevel + 1, playheadNormalized);
        updateZoomLabel();
        DBG("ClipEditDialog: T key - Zoom in to level " << (currentLevel + 1));
      }
    }
    return true;
  }

  // LEFT ARROW key: Skip to Start (|<<)
  if (key == juce::KeyPress::leftKey) {
    if (m_skipToStartButton) {
      m_skipToStartButton->triggerClick();
      DBG("ClipEditDialog: Left Arrow - Skip to start");
    }
    return true;
  }

  // RIGHT ARROW key: Skip to End (>>|)
  if (key == juce::KeyPress::rightKey) {
    if (m_skipToEndButton) {
      m_skipToEndButton->triggerClick();
      DBG("ClipEditDialog: Right Arrow - Skip to end");
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

      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      restartPlayback();

      DBG("ClipEditDialog: 'I' key - Set IN point to sample " << m_metadata.trimInSamples
                                                              << ", restarted from IN");
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

      if (m_waveformDisplay) {
        m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      }

      // EDIT LAW: ANY trim command restarts playback from IN (unconditional)
      m_previewPlayer->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
      restartPlayback();

      DBG("ClipEditDialog: 'O' key - Set OUT point to sample " << m_metadata.trimOutSamples
                                                               << ", restarted from IN");
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

        // 2s end audition (like >>|): Jump to OUT - 2s and start playing
        bool wasPlaying = m_previewPlayer->isPlaying();

        // Calculate target: 2 seconds before OUT, or IN if clip < 2s
        int64_t twoSecondsInSamples = 2 * m_metadata.sampleRate;
        int64_t targetPosition =
            std::max(m_metadata.trimInSamples, m_metadata.trimOutSamples - twoSecondsInSamples);

        m_previewPlayer->jumpTo(targetPosition);

        // Set audition region for visual feedback (yellow highlight from playhead to OUT)
        if (m_waveformDisplay) {
          m_waveformDisplay->setAuditionRegion(targetPosition, m_metadata.trimOutSamples);
        }

        // If was playing, resume playing; if paused, stay paused
        if (wasPlaying && !m_previewPlayer->isPlaying()) {
          m_previewPlayer->play();
        }

        DBG("ClipEditDialog: ; key - trim OUT changed, 2s end audition");
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

        // 2s end audition (like >>|): Jump to OUT - 2s and start playing
        bool wasPlaying = m_previewPlayer->isPlaying();

        // Calculate target: 2 seconds before OUT, or IN if clip < 2s
        int64_t twoSecondsInSamples = 2 * m_metadata.sampleRate;
        int64_t targetPosition =
            std::max(m_metadata.trimInSamples, m_metadata.trimOutSamples - twoSecondsInSamples);

        m_previewPlayer->jumpTo(targetPosition);

        // Set audition region for visual feedback (yellow highlight from playhead to OUT)
        if (m_waveformDisplay) {
          m_waveformDisplay->setAuditionRegion(targetPosition, m_metadata.trimOutSamples);
        }

        // If was playing, resume playing; if paused, stay paused
        if (wasPlaying && !m_previewPlayer->isPlaying()) {
          m_previewPlayer->play();
        }

        DBG("ClipEditDialog: ' key - trim OUT changed, 2s end audition");
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

//==============================================================================
void ClipEditDialog::updateTransportButtonColors() {
  // Color scheme: Dark (component bg) = inactive, Bright (colored) = active
  // Using DesignTokens for consistent Neve-inspired console aesthetic
  if (!m_previewPlayer)
    return;

  const bool isPlaying = getPreviewSnapshot(m_previewPlayer).isPlaying;
  bool isLoopEnabled = m_metadata.loopEnabled;

  const juce::Colour DARK_INACTIVE = juce::Colour(kBgComponent);
  const juce::Colour PLAY_ACTIVE = juce::Colour(kAccentGreen);
  const juce::Colour STOP_ACTIVE = juce::Colour(kMeterRed);
  const juce::Colour LOOP_ACTIVE = juce::Colour(kNeveBlue);

  // Play button: Green when playing, component bg when stopped
  if (m_playButton) {
    m_playButton->setColour(juce::DrawableButton::backgroundColourId,
                            isPlaying ? PLAY_ACTIVE : DARK_INACTIVE);
  }

  // Stop button: Red when stopped, component bg when playing
  if (m_stopButton) {
    m_stopButton->setColour(juce::DrawableButton::backgroundColourId,
                            !isPlaying ? STOP_ACTIVE : DARK_INACTIVE);
  }

  // Loop button: Neve blue when loop enabled, component bg when disabled
  if (m_loopButton) {
    m_loopButton->setColour(juce::DrawableButton::backgroundColourId,
                            isLoopEnabled ? LOOP_ACTIVE : DARK_INACTIVE);
  }

  // Skip buttons: Always component bg (navigation only, not stateful)
  if (m_skipToStartButton) {
    m_skipToStartButton->setColour(juce::DrawableButton::backgroundColourId, DARK_INACTIVE);
  }
  if (m_skipToEndButton) {
    m_skipToEndButton->setColour(juce::DrawableButton::backgroundColourId, DARK_INACTIVE);
  }
}
