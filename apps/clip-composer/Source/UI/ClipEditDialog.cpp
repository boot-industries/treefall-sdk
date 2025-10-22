// SPDX-License-Identifier: MIT

#include "ClipEditDialog.h"

//==============================================================================
ClipEditDialog::ClipEditDialog() {
  // Build Phase 1 UI (basic metadata)
  buildPhase1UI();

  // Build Phase 2 UI (In/Out points)
  buildPhase2UI();

  // Build Phase 3 UI (Fade times)
  buildPhase3UI();

  setSize(700, 800); // Expanded for all phases
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

  // Phase 2: Initialize trim sliders based on audio file duration
  if (m_trimInSlider && m_trimOutSlider) {
    double maxSamples = static_cast<double>(m_metadata.durationSamples);
    m_trimInSlider->setRange(0.0, maxSamples, 1.0);
    m_trimOutSlider->setRange(0.0, maxSamples, 1.0);

    m_trimInSlider->setValue(static_cast<double>(m_metadata.trimInSamples),
                             juce::dontSendNotification);
    m_trimOutSlider->setValue(
        m_metadata.trimOutSamples > 0 ? static_cast<double>(m_metadata.trimOutSamples) : maxSamples,
        juce::dontSendNotification);

    updateTrimInfoLabel();
  }

  // Load waveform display
  if (m_waveformDisplay && m_metadata.filePath.isNotEmpty()) {
    juce::File audioFile(m_metadata.filePath);
    if (audioFile.existsAsFile()) {
      m_waveformDisplay->setAudioFile(audioFile);
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples > 0
                                                                     ? m_metadata.trimOutSamples
                                                                     : m_metadata.durationSamples);
    }
  }

  // Phase 3: Initialize fade sliders
  if (m_fadeInSlider) {
    m_fadeInSlider->setValue(m_metadata.fadeInSeconds, juce::dontSendNotification);
  }
  if (m_fadeOutSlider) {
    m_fadeOutSlider->setValue(m_metadata.fadeOutSeconds, juce::dontSendNotification);
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

void ClipEditDialog::updateTrimInfoLabel() {
  if (!m_trimInfoLabel)
    return;

  int64_t trimmedSamples = m_metadata.trimOutSamples - m_metadata.trimInSamples;

  if (trimmedSamples < 0) {
    m_trimInfoLabel->setText("Invalid trim range", juce::dontSendNotification);
    return;
  }

  if (m_metadata.sampleRate > 0) {
    double durationSeconds = static_cast<double>(trimmedSamples) / m_metadata.sampleRate;
    int totalSeconds = static_cast<int>(durationSeconds);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    juce::String durationText =
        juce::String::formatted("Trimmed Duration: %d:%02d", minutes, seconds);
    m_trimInfoLabel->setText(durationText, juce::dontSendNotification);
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

  // Trim In Point
  m_trimInLabel = std::make_unique<juce::Label>("trimInLabel", "Trim In (samples):");
  m_trimInLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_trimInLabel.get());

  m_trimInSlider =
      std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
  m_trimInSlider->setRange(0.0, 1000000.0, 1.0); // Will be set dynamically based on file
  m_trimInSlider->onValueChange = [this]() {
    m_metadata.trimInSamples = static_cast<int64_t>(m_trimInSlider->getValue());
    updateTrimInfoLabel();
    // Update waveform display markers
    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }
  };
  addAndMakeVisible(m_trimInSlider.get());

  // Trim Out Point
  m_trimOutLabel = std::make_unique<juce::Label>("trimOutLabel", "Trim Out (samples):");
  m_trimOutLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_trimOutLabel.get());

  m_trimOutSlider =
      std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
  m_trimOutSlider->setRange(0.0, 1000000.0, 1.0);
  m_trimOutSlider->onValueChange = [this]() {
    m_metadata.trimOutSamples = static_cast<int64_t>(m_trimOutSlider->getValue());
    updateTrimInfoLabel();
    // Update waveform display markers
    if (m_waveformDisplay) {
      m_waveformDisplay->setTrimPoints(m_metadata.trimInSamples, m_metadata.trimOutSamples);
    }
  };
  addAndMakeVisible(m_trimOutSlider.get());

  // Trim Info Label (shows duration in seconds)
  m_trimInfoLabel = std::make_unique<juce::Label>("trimInfoLabel", "Duration: --:--");
  m_trimInfoLabel->setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
  m_trimInfoLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(m_trimInfoLabel.get());
}

void ClipEditDialog::buildPhase3UI() {
  // Fade In Section
  m_fadeInLabel = std::make_unique<juce::Label>("fadeInLabel", "Fade In:");
  m_fadeInLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_fadeInLabel.get());

  m_fadeInSlider =
      std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
  m_fadeInSlider->setRange(0.0, 3.0, 0.1); // 0.0s - 3.0s in 0.1s increments
  m_fadeInSlider->setTextValueSuffix(" s");
  m_fadeInSlider->onValueChange = [this]() {
    m_metadata.fadeInSeconds = m_fadeInSlider->getValue();
  };
  addAndMakeVisible(m_fadeInSlider.get());

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
  };
  addAndMakeVisible(m_fadeInCurveCombo.get());

  // Fade Out Section
  m_fadeOutLabel = std::make_unique<juce::Label>("fadeOutLabel", "Fade Out:");
  m_fadeOutLabel->setFont(juce::FontOptions("Inter", 14.0f, juce::Font::bold));
  addAndMakeVisible(m_fadeOutLabel.get());

  m_fadeOutSlider =
      std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
  m_fadeOutSlider->setRange(0.0, 3.0, 0.1); // 0.0s - 3.0s in 0.1s increments
  m_fadeOutSlider->setTextValueSuffix(" s");
  m_fadeOutSlider->onValueChange = [this]() {
    m_metadata.fadeOutSeconds = m_fadeOutSlider->getValue();
  };
  addAndMakeVisible(m_fadeOutSlider.get());

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
  };
  addAndMakeVisible(m_fadeOutCurveCombo.get());
}

//==============================================================================
void ClipEditDialog::paint(juce::Graphics& g) {
  // Dark background
  g.fillAll(juce::Colour(0xff1a1a1a));

  // Title bar
  g.setColour(juce::Colour(0xff252525));
  g.fillRect(0, 0, getWidth(), 50);

  // Title text
  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions("Inter", 20.0f, juce::Font::bold));
  g.drawText("Edit Clip", 20, 0, 400, 50, juce::Justification::centredLeft, false);
}

void ClipEditDialog::resized() {
  auto bounds = getLocalBounds();

  // Title bar (50px)
  bounds.removeFromTop(50);

  // Content area with padding
  auto contentArea = bounds.reduced(20);

  // === PHASE 1: Basic Metadata ===
  // Clip Name
  auto nameRow = contentArea.removeFromTop(60);
  m_nameLabel->setBounds(nameRow.removeFromTop(20));
  m_nameEditor->setBounds(nameRow.removeFromTop(30));
  contentArea.removeFromTop(10); // Spacing

  // File Path
  auto filePathRow = contentArea.removeFromTop(60);
  m_filePathLabel->setBounds(filePathRow.removeFromTop(20));
  m_filePathEditor->setBounds(filePathRow.removeFromTop(30));
  contentArea.removeFromTop(10); // Spacing

  // Color
  auto colorRow = contentArea.removeFromTop(50);
  m_colorLabel->setBounds(colorRow.removeFromLeft(150));
  m_colorComboBox->setBounds(colorRow.removeFromLeft(250));
  contentArea.removeFromTop(10); // Spacing

  // Clip Group
  auto groupRow = contentArea.removeFromTop(50);
  m_groupLabel->setBounds(groupRow.removeFromLeft(150));
  m_groupComboBox->setBounds(groupRow.removeFromLeft(250));
  contentArea.removeFromTop(20); // Extra spacing before Phase 2

  // === PHASE 2: In/Out Points ===
  // Waveform Display
  if (m_waveformDisplay) {
    auto waveformArea = contentArea.removeFromTop(100);
    m_waveformDisplay->setBounds(waveformArea);
  }
  contentArea.removeFromTop(10);

  // Trim In
  if (m_trimInLabel && m_trimInSlider) {
    auto trimInRow = contentArea.removeFromTop(50);
    m_trimInLabel->setBounds(trimInRow.removeFromTop(20));
    m_trimInSlider->setBounds(trimInRow.removeFromTop(25));
  }
  contentArea.removeFromTop(5);

  // Trim Out
  if (m_trimOutLabel && m_trimOutSlider) {
    auto trimOutRow = contentArea.removeFromTop(50);
    m_trimOutLabel->setBounds(trimOutRow.removeFromTop(20));
    m_trimOutSlider->setBounds(trimOutRow.removeFromTop(25));
  }
  contentArea.removeFromTop(5);

  // Trim Info Label
  if (m_trimInfoLabel) {
    auto trimInfoRow = contentArea.removeFromTop(25);
    m_trimInfoLabel->setBounds(trimInfoRow);
  }
  contentArea.removeFromTop(20); // Extra spacing before Phase 3

  // === PHASE 3: Fade Times ===
  // Fade In
  if (m_fadeInLabel && m_fadeInSlider && m_fadeInCurveCombo) {
    auto fadeInRow = contentArea.removeFromTop(60);
    m_fadeInLabel->setBounds(fadeInRow.removeFromTop(20));

    auto fadeInControlRow = fadeInRow.removeFromTop(30);
    m_fadeInSlider->setBounds(fadeInControlRow.removeFromLeft(fadeInControlRow.getWidth() - 160));
    fadeInControlRow.removeFromLeft(10); // Spacing
    m_fadeInCurveCombo->setBounds(fadeInControlRow);
  }
  contentArea.removeFromTop(10);

  // Fade Out
  if (m_fadeOutLabel && m_fadeOutSlider && m_fadeOutCurveCombo) {
    auto fadeOutRow = contentArea.removeFromTop(60);
    m_fadeOutLabel->setBounds(fadeOutRow.removeFromTop(20));

    auto fadeOutControlRow = fadeOutRow.removeFromTop(30);
    m_fadeOutSlider->setBounds(
        fadeOutControlRow.removeFromLeft(fadeOutControlRow.getWidth() - 160));
    fadeOutControlRow.removeFromLeft(10); // Spacing
    m_fadeOutCurveCombo->setBounds(fadeOutControlRow);
  }

  // Dialog buttons at bottom
  auto buttonArea = contentArea.removeFromBottom(40);
  m_cancelButton->setBounds(buttonArea.removeFromRight(100));
  buttonArea.removeFromRight(10); // Spacing
  m_okButton->setBounds(buttonArea.removeFromRight(100));
}
