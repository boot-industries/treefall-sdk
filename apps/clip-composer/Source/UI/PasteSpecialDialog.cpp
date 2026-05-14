/*
  ==============================================================================

    PasteSpecialDialog.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 17: Paste Special Dialog (OCC117)

  ==============================================================================
*/

#include "PasteSpecialDialog.h"
#include "../Core/GridConstants.h"
#include "DesignTokens.h"

PasteSpecialDialog::PasteSpecialDialog(SessionManager* sessionManager,
                                       const SessionManager::ClipData& sourceClip, int currentTab)
    : m_sessionManager(sessionManager), m_sourceClip(sourceClip), m_currentTab(currentTab) {

  // Title
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("Paste Special", juce::dontSendNotification);
  m_titleLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
  m_titleLabel.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(m_sourceLabel);
  m_sourceLabel.setText("Source: " + juce::String(sourceClip.displayName),
                        juce::dontSendNotification);
  m_sourceLabel.setJustificationType(juce::Justification::centred);
  m_sourceLabel.setColour(juce::Label::textColourId, juce::Colour(OCC::Design::kTextMuted));

  // Levels section
  addAndMakeVisible(m_levelsLabel);
  m_levelsLabel.setText("Levels", juce::dontSendNotification);
  m_levelsLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

  addAndMakeVisible(m_gainAbsoluteCheckbox);
  m_gainAbsoluteCheckbox.setButtonText("Gain (Absolute)");
  m_gainAbsoluteCheckbox.onClick = [this]() {
    if (m_gainAbsoluteCheckbox.getToggleState()) {
      m_gainRelativeCheckbox.setToggleState(false, juce::dontSendNotification);
    }
    updateControlStates();
  };

  addAndMakeVisible(m_gainRelativeCheckbox);
  m_gainRelativeCheckbox.setButtonText("Gain (Relative)");
  m_gainRelativeCheckbox.onClick = [this]() {
    if (m_gainRelativeCheckbox.getToggleState()) {
      m_gainAbsoluteCheckbox.setToggleState(false, juce::dontSendNotification);
    }
    updateControlStates();
  };

  addAndMakeVisible(m_gainRelativeSlider);
  m_gainRelativeSlider.setRange(-30.0, 10.0, 0.1);
  m_gainRelativeSlider.setValue(0.0);
  m_gainRelativeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  m_gainRelativeSlider.onValueChange = [this]() {
    m_gainRelativeValueLabel.setText(juce::String(m_gainRelativeSlider.getValue(), 1) + " dB",
                                     juce::dontSendNotification);
  };

  addAndMakeVisible(m_gainRelativeValueLabel);
  m_gainRelativeValueLabel.setText("0.0 dB", juce::dontSendNotification);

  // Fades section
  addAndMakeVisible(m_fadesLabel);
  m_fadesLabel.setText("Fades", juce::dontSendNotification);
  m_fadesLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

  addAndMakeVisible(m_fadeInCheckbox);
  m_fadeInCheckbox.setButtonText("Fade In Time");
  m_fadeInCheckbox.onClick = [this]() {
    if (m_fadeInCheckbox.getToggleState()) {
      m_fadeInCurveCheckbox.setToggleState(true, juce::dontSendNotification);
    }
  };

  addAndMakeVisible(m_fadeInCurveCheckbox);
  m_fadeInCurveCheckbox.setButtonText("Fade In Curve");

  addAndMakeVisible(m_fadeOutCheckbox);
  m_fadeOutCheckbox.setButtonText("Fade Out Time");
  m_fadeOutCheckbox.onClick = [this]() {
    if (m_fadeOutCheckbox.getToggleState()) {
      m_fadeOutCurveCheckbox.setToggleState(true, juce::dontSendNotification);
    }
  };

  addAndMakeVisible(m_fadeOutCurveCheckbox);
  m_fadeOutCurveCheckbox.setButtonText("Fade Out Curve");

  // Misc section
  addAndMakeVisible(m_miscLabel);
  m_miscLabel.setText("Misc", juce::dontSendNotification);
  m_miscLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

  addAndMakeVisible(m_colorCheckbox);
  m_colorCheckbox.setButtonText("Color");

  addAndMakeVisible(m_clipGroupCheckbox);
  m_clipGroupCheckbox.setButtonText("Clip Group");

  addAndMakeVisible(m_loopCheckbox);
  m_loopCheckbox.setButtonText("Loop");

  addAndMakeVisible(m_stopOthersCheckbox);
  m_stopOthersCheckbox.setButtonText("Stop Others");

  // Scope section
  addAndMakeVisible(m_scopeLabel);
  m_scopeLabel.setText("Paste To:", juce::dontSendNotification);
  m_scopeLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

  addAndMakeVisible(m_scopeCurrentPageRadio);
  m_scopeCurrentPageRadio.setButtonText("Current Page");
  m_scopeCurrentPageRadio.setRadioGroupId(1);
  m_scopeCurrentPageRadio.setToggleState(true, juce::dontSendNotification);
  m_scopeCurrentPageRadio.onClick = [this]() { updateControlStates(); };

  addAndMakeVisible(m_scopeAllPagesRadio);
  m_scopeAllPagesRadio.setButtonText("All Pages");
  m_scopeAllPagesRadio.setRadioGroupId(1);
  m_scopeAllPagesRadio.onClick = [this]() { updateControlStates(); };

  addAndMakeVisible(m_scopeRangeRadio);
  m_scopeRangeRadio.setButtonText("Range:");
  m_scopeRangeRadio.setRadioGroupId(1);
  m_scopeRangeRadio.onClick = [this]() { updateControlStates(); };

  addAndMakeVisible(m_rangeStartEditor);
  m_rangeStartEditor.setText("1");
  m_rangeStartEditor.setInputRestrictions(3, "0123456789");

  addAndMakeVisible(m_rangeDashLabel);
  m_rangeDashLabel.setText("-", juce::dontSendNotification);
  m_rangeDashLabel.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(m_rangeEndEditor);
  m_rangeEndEditor.setText(juce::String(occ::TOTAL_BUTTONS));
  m_rangeEndEditor.setInputRestrictions(3, "0123456789");

  // Buttons
  addAndMakeVisible(m_clearAllButton);
  m_clearAllButton.setButtonText("Clear All");
  m_clearAllButton.onClick = [this]() { clearAllOptions(); };

  addAndMakeVisible(m_okButton);
  m_okButton.setButtonText("OK");
  m_okButton.onClick = [this]() {
    if (onOkClicked)
      onOkClicked();
  };

  addAndMakeVisible(m_cancelButton);
  m_cancelButton.setButtonText("Cancel");
  m_cancelButton.onClick = [this]() {
    if (onCancelClicked)
      onCancelClicked();
  };

  updateControlStates();
  setSize(400, 520);
}

void PasteSpecialDialog::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(OCC::Design::kBgSurface));

  // Draw border
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawRect(getLocalBounds(), 1);

  // Draw section separators
  g.setColour(juce::Colour(OCC::Design::kBorderDefault).withAlpha(0.65f));
  g.drawHorizontalLine(85, 10, getWidth() - 10);
  g.drawHorizontalLine(175, 10, getWidth() - 10);
  g.drawHorizontalLine(285, 10, getWidth() - 10);
  g.drawHorizontalLine(385, 10, getWidth() - 10);
}

void PasteSpecialDialog::resized() {
  auto area = getLocalBounds().reduced(15);

  // Title
  m_titleLabel.setBounds(area.removeFromTop(30));
  m_sourceLabel.setBounds(area.removeFromTop(20));
  area.removeFromTop(15);

  // Levels section
  m_levelsLabel.setBounds(area.removeFromTop(20));
  auto levelsRow1 = area.removeFromTop(25);
  m_gainAbsoluteCheckbox.setBounds(levelsRow1.removeFromLeft(150));
  m_gainRelativeCheckbox.setBounds(levelsRow1.removeFromLeft(150));

  auto levelsRow2 = area.removeFromTop(25);
  levelsRow2.removeFromLeft(20); // Indent
  m_gainRelativeSlider.setBounds(levelsRow2.removeFromLeft(200));
  m_gainRelativeValueLabel.setBounds(levelsRow2.removeFromLeft(60));
  area.removeFromTop(10);

  // Fades section
  m_fadesLabel.setBounds(area.removeFromTop(20));
  auto fadesRow1 = area.removeFromTop(25);
  m_fadeInCheckbox.setBounds(fadesRow1.removeFromLeft(150));
  m_fadeInCurveCheckbox.setBounds(fadesRow1.removeFromLeft(150));
  auto fadesRow2 = area.removeFromTop(25);
  m_fadeOutCheckbox.setBounds(fadesRow2.removeFromLeft(150));
  m_fadeOutCurveCheckbox.setBounds(fadesRow2.removeFromLeft(150));
  area.removeFromTop(10);

  // Misc section
  m_miscLabel.setBounds(area.removeFromTop(20));
  auto miscRow1 = area.removeFromTop(25);
  m_colorCheckbox.setBounds(miscRow1.removeFromLeft(100));
  m_clipGroupCheckbox.setBounds(miscRow1.removeFromLeft(100));
  auto miscRow2 = area.removeFromTop(25);
  m_loopCheckbox.setBounds(miscRow2.removeFromLeft(100));
  m_stopOthersCheckbox.setBounds(miscRow2.removeFromLeft(100));
  area.removeFromTop(10);

  // Scope section
  m_scopeLabel.setBounds(area.removeFromTop(20));
  auto scopeRow1 = area.removeFromTop(25);
  m_scopeCurrentPageRadio.setBounds(scopeRow1.removeFromLeft(130));
  m_scopeAllPagesRadio.setBounds(scopeRow1.removeFromLeft(130));

  auto scopeRow2 = area.removeFromTop(25);
  m_scopeRangeRadio.setBounds(scopeRow2.removeFromLeft(80));
  m_rangeStartEditor.setBounds(scopeRow2.removeFromLeft(50));
  m_rangeDashLabel.setBounds(scopeRow2.removeFromLeft(20));
  m_rangeEndEditor.setBounds(scopeRow2.removeFromLeft(50));

  // Buttons at bottom
  auto buttonArea = getLocalBounds().reduced(15).removeFromBottom(35);
  m_cancelButton.setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(10);
  m_okButton.setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(20);
  m_clearAllButton.setBounds(buttonArea.removeFromLeft(80));
}

orpheus::PasteSpecialOptions PasteSpecialDialog::getOptions() const {
  orpheus::PasteSpecialOptions options;

  // Gain
  options.gainAbsolute = m_gainAbsoluteCheckbox.getToggleState();
  options.gainRelative = m_gainRelativeCheckbox.getToggleState();
  options.gainRelativeDb = static_cast<float>(m_gainRelativeSlider.getValue());

  // Fades
  options.fadeIn = m_fadeInCheckbox.getToggleState();
  options.fadeInCurve = m_fadeInCurveCheckbox.getToggleState();
  options.fadeOut = m_fadeOutCheckbox.getToggleState();
  options.fadeOutCurve = m_fadeOutCurveCheckbox.getToggleState();

  // Misc
  options.color = m_colorCheckbox.getToggleState();
  options.clipGroup = m_clipGroupCheckbox.getToggleState();
  options.loop = m_loopCheckbox.getToggleState();
  options.stopOthers = m_stopOthersCheckbox.getToggleState();

  return options;
}

std::vector<int> PasteSpecialDialog::getTargetIndices() const {
  std::vector<int> indices;

  if (m_scopeCurrentPageRadio.getToggleState()) {
    // Current page only
    int startIndex = m_currentTab * occ::BUTTONS_PER_TAB;
    for (int i = 0; i < occ::BUTTONS_PER_TAB; ++i) {
      indices.push_back(startIndex + i);
    }
  } else if (m_scopeAllPagesRadio.getToggleState()) {
    // All pages
    for (int i = 0; i < occ::TOTAL_BUTTONS; ++i) {
      indices.push_back(i);
    }
  } else if (m_scopeRangeRadio.getToggleState()) {
    // Range
    int start = m_rangeStartEditor.getText().getIntValue() - 1; // Convert to 0-based
    int end = m_rangeEndEditor.getText().getIntValue() - 1;

    start = juce::jlimit(0, occ::TOTAL_BUTTONS - 1, start);
    end = juce::jlimit(0, occ::TOTAL_BUTTONS - 1, end);

    if (start > end)
      std::swap(start, end);

    for (int i = start; i <= end; ++i) {
      indices.push_back(i);
    }
  }

  return indices;
}

void PasteSpecialDialog::updateControlStates() {
  // Enable/disable gain relative slider
  m_gainRelativeSlider.setEnabled(m_gainRelativeCheckbox.getToggleState());
  m_gainRelativeValueLabel.setEnabled(m_gainRelativeCheckbox.getToggleState());

  // Enable/disable range editors
  bool rangeEnabled = m_scopeRangeRadio.getToggleState();
  m_rangeStartEditor.setEnabled(rangeEnabled);
  m_rangeEndEditor.setEnabled(rangeEnabled);
}

void PasteSpecialDialog::clearAllOptions() {
  m_gainAbsoluteCheckbox.setToggleState(false, juce::dontSendNotification);
  m_gainRelativeCheckbox.setToggleState(false, juce::dontSendNotification);
  m_fadeInCheckbox.setToggleState(false, juce::dontSendNotification);
  m_fadeInCurveCheckbox.setToggleState(false, juce::dontSendNotification);
  m_fadeOutCheckbox.setToggleState(false, juce::dontSendNotification);
  m_fadeOutCurveCheckbox.setToggleState(false, juce::dontSendNotification);
  m_colorCheckbox.setToggleState(false, juce::dontSendNotification);
  m_clipGroupCheckbox.setToggleState(false, juce::dontSendNotification);
  m_loopCheckbox.setToggleState(false, juce::dontSendNotification);
  m_stopOthersCheckbox.setToggleState(false, juce::dontSendNotification);

  updateControlStates();
}
