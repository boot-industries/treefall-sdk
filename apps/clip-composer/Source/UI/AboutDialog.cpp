/*
  ==============================================================================

    AboutDialog.cpp
    Created: 18 Jan 2026
    Author:  Orpheus Clip Composer

    OCC144: About Dialog implementation for macOS standard menu compliance
    OCC149: Updated with Console design language (Design-system alignment)

  ==============================================================================
*/

#include "AboutDialog.h"
#include "BuildInfo.h"
#include "DesignTokens.h"
#include "ConsoleTheme.h"

using namespace OCC::Design;

//==============================================================================
AboutDialog::AboutDialog() {
  // Title
  addAndMakeVisible(m_titleLabel);
  m_titleLabel.setText("Orpheus Clip Composer", juce::dontSendNotification);
  m_titleLabel.setFont(OCC::Console::consoleFont(24.0f, juce::Font::bold));
  m_titleLabel.setJustificationType(juce::Justification::centred);
  m_titleLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  // Version
  addAndMakeVisible(m_versionLabel);
  m_versionLabel.setText("Version " + juce::String(occ::BuildInfo::version),
                         juce::dontSendNotification);
  m_versionLabel.setFont(OCC::Console::consoleFont(14.0f));
  m_versionLabel.setJustificationType(juce::Justification::centred);
  m_versionLabel.setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));

  // Build info
  addAndMakeVisible(m_buildLabel);
  juce::String buildInfo = "Build: ";
  buildInfo += occ::BuildInfo::buildDate;
  buildInfo += "\n";
  buildInfo += occ::BuildInfo::gitDescribe;
  buildInfo += " (";
  buildInfo += occ::BuildInfo::gitHash;
  buildInfo += ")";
  m_buildLabel.setText(buildInfo, juce::dontSendNotification);
  m_buildLabel.setFont(OCC::Console::monoFont(11.0f));
  m_buildLabel.setJustificationType(juce::Justification::centred);
  m_buildLabel.setColour(juce::Label::textColourId, juce::Colour(kTextMuted));

  // Copyright
  addAndMakeVisible(m_copyrightLabel);
  m_copyrightLabel.setText("Copyright 2025-2026 Chris Lyons", juce::dontSendNotification);
  m_copyrightLabel.setFont(OCC::Console::consoleFont(12.0f));
  m_copyrightLabel.setJustificationType(juce::Justification::centred);
  m_copyrightLabel.setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));

  // Credits
  addAndMakeVisible(m_creditsLabel);
  juce::String credits;
  credits << "Professional soundboard for broadcast, theater, and live performance.\n\n";
  credits << "Built with JUCE Framework and Orpheus SDK.\n";
  credits << "Audio processing powered by libsndfile and SpeexDSP.\n";
  credits << "Visualization by shmui.";
  m_creditsLabel.setText(credits, juce::dontSendNotification);
  m_creditsLabel.setFont(OCC::Console::monoFont(11.0f));
  m_creditsLabel.setJustificationType(juce::Justification::centred);
  m_creditsLabel.setColour(juce::Label::textColourId, juce::Colour(kTextMuted));

  // OK button - Console primary action
  m_okButton = std::make_unique<ConsoleActionButton>("about-ok", ConsoleActionButton::Variant::Primary);
  m_okButton->setLabel("OK");
  m_okButton->onClick = [this]() {
    if (onOkClicked)
      onOkClicked();
  };
  addAndMakeVisible(m_okButton.get());

  setSize(getPreferredWidth(), getPreferredHeight());
}

void AboutDialog::paint(juce::Graphics& g) {
  // Console chassis background
  g.fillAll(juce::Colour(kBgSurface));

  // Subtle gradient overlay
  juce::ColourGradient gradient(juce::Colour(kBgSurface).withAlpha(0.9f), 0.0f, 0.0f,
                                juce::Colour(kBgPrimary).withAlpha(0.9f), 0.0f,
                                static_cast<float>(getHeight()), false);
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);

  // Border - Neve accent
  g.setColour(juce::Colour(kNeveBlue).withAlpha(0.6f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 8.0f, kBorderMedium);

  // Inner shadow / depth
  g.setColour(juce::Colours::black.withAlpha(0.3f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f), 5.0f, kBorderThin);

  // App icon placeholder (cyan circle with O)
  auto iconBounds = juce::Rectangle<int>(0, 20, getWidth(), 60);
  g.setColour(juce::Colour(kAccentCyan));
  g.fillEllipse(iconBounds.getCentreX() - 25.0f, iconBounds.getY() + 5.0f, 50.0f, 50.0f);

  g.setColour(juce::Colours::white);
  g.setFont(OCC::Console::consoleFont(28.0f, juce::Font::bold));
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
  m_buildLabel.setBounds(bounds.removeFromTop(32));
  bounds.removeFromTop(10);

  // Copyright
  m_copyrightLabel.setBounds(bounds.removeFromTop(18));
  bounds.removeFromTop(15);

  // Credits
  m_creditsLabel.setBounds(bounds.removeFromTop(80));

  // OK button at bottom
  bounds = getLocalBounds().reduced(20);
  auto buttonArea = bounds.removeFromBottom(35);
  m_okButton->setBounds(buttonArea.withSizeKeepingCentre(100, 30));
}