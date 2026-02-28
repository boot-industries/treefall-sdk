/*
  ==============================================================================

    AboutDialog.cpp
    Created: 18 Jan 2026
    Author:  Orpheus Clip Composer

    OCC144: About Dialog implementation for macOS standard menu compliance

  ==============================================================================
*/

#include "AboutDialog.h"
#include "../BuildInfo.h"

//==============================================================================
AboutDialog::AboutDialog() {
  // Title
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("Orpheus Clip Composer", juce::dontSendNotification);
  m_titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
  m_titleLabel.setJustificationType(juce::Justification::centred);
  m_titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  // Version
  addAndMakeVisible(m_versionLabel);
  m_versionLabel.setText("Version 0.2.0-alpha", juce::dontSendNotification);
  m_versionLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
  m_versionLabel.setJustificationType(juce::Justification::centred);
  m_versionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccccc));

  // Build info
  addAndMakeVisible(m_buildLabel);
  juce::String buildInfo = "Build: ";
  buildInfo += APP_BUILD_DATE;
  buildInfo += " (";
  buildInfo += APP_COMMIT_HASH;
  buildInfo += ")";
  m_buildLabel.setText(buildInfo, juce::dontSendNotification);
  m_buildLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
  m_buildLabel.setJustificationType(juce::Justification::centred);
  m_buildLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));

  // Copyright
  addAndMakeVisible(m_copyrightLabel);
  m_copyrightLabel.setText("Copyright 2025-2026 Chris Lyons", juce::dontSendNotification);
  m_copyrightLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
  m_copyrightLabel.setJustificationType(juce::Justification::centred);
  m_copyrightLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));

  // Credits
  addAndMakeVisible(m_creditsLabel);
  juce::String credits;
  credits << "Professional soundboard for broadcast, theater, and live performance.\n\n";
  credits << "Built with JUCE Framework and Orpheus SDK.\n";
  credits << "Audio processing powered by libsndfile and SpeexDSP.\n";
  credits << "Visualization by shmui.";
  m_creditsLabel.setText(credits, juce::dontSendNotification);
  m_creditsLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
  m_creditsLabel.setJustificationType(juce::Justification::centred);
  m_creditsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff999999));

  // OK button
  addAndMakeVisible(m_okButton);
  m_okButton.setButtonText("OK");
  m_okButton.onClick = [this]() {
    if (onOkClicked)
      onOkClicked();
  };

  setSize(getPreferredWidth(), getPreferredHeight());
}

void AboutDialog::paint(juce::Graphics& g) {
  // Dark background with subtle gradient
  juce::ColourGradient gradient(juce::Colour(0xff2a2a2a), 0.0f, 0.0f, juce::Colour(0xff1e1e1e),
                                0.0f, static_cast<float>(getHeight()), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

  // Border
  g.setColour(juce::Colour(0xff444444));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);

  // App icon placeholder (cyan circle with O)
  auto iconBounds = juce::Rectangle<int>(0, 20, getWidth(), 60);
  g.setColour(juce::Colour(0xff00bcd4)); // Accent cyan
  g.fillEllipse(iconBounds.getCentreX() - 25.0f, iconBounds.getY() + 5.0f, 50.0f, 50.0f);

  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
  g.drawText("O", iconBounds.getCentreX() - 25, iconBounds.getY() + 5, 50, 50,
             juce::Justification::centred);
}

void AboutDialog::resized() {
  auto bounds = getLocalBounds().reduced(20);

  // Icon area (painted)
  bounds.removeFromTop(70);

  // Title
  m_titleLabel.setBounds(bounds.removeFromTop(35));
  bounds.removeFromTop(5);

  // Version
  m_versionLabel.setBounds(bounds.removeFromTop(20));
  bounds.removeFromTop(2);

  // Build info
  m_buildLabel.setBounds(bounds.removeFromTop(18));
  bounds.removeFromTop(10);

  // Copyright
  m_copyrightLabel.setBounds(bounds.removeFromTop(18));
  bounds.removeFromTop(15);

  // Credits
  m_creditsLabel.setBounds(bounds.removeFromTop(80));

  // OK button at bottom
  bounds = getLocalBounds().reduced(20);
  auto buttonArea = bounds.removeFromBottom(35);
  m_okButton.setBounds(buttonArea.withSizeKeepingCentre(100, 30));
}
