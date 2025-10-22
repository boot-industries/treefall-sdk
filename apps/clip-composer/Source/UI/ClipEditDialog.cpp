// SPDX-License-Identifier: MIT

#include "ClipEditDialog.h"

//==============================================================================
ClipEditDialog::ClipEditDialog() {
  // Build Phase 1 UI (basic metadata)
  buildPhase1UI();

  setSize(600, 500);
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
  // TODO: Add In/Out point controls with waveform display
}

void ClipEditDialog::buildPhase3UI() {
  // TODO: Add Fade In/Out time sliders
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

  // Dialog buttons at bottom
  auto buttonArea = contentArea.removeFromBottom(40);
  m_cancelButton->setBounds(buttonArea.removeFromRight(100));
  buttonArea.removeFromRight(10); // Spacing
  m_okButton->setBounds(buttonArea.removeFromRight(100));
}
