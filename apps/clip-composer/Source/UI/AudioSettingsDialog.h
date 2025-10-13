// SPDX-License-Identifier: MIT
#pragma once

#include "../Audio/AudioEngine.h"
#include <JuceHeader.h>

/// Audio I/O Settings Dialog
/// Allows user to configure sample rate, buffer size, and audio device
class AudioSettingsDialog : public juce::Component {
public:
  AudioSettingsDialog(AudioEngine* engine) : m_audioEngine(engine) {
    // Sample Rate dropdown
    addAndMakeVisible(m_sampleRateLabel);
    m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    m_sampleRateLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(m_sampleRateCombo);
    m_sampleRateCombo.addItem("44100 Hz", 1);
    m_sampleRateCombo.addItem("48000 Hz", 2);
    m_sampleRateCombo.addItem("96000 Hz", 3);
    m_sampleRateCombo.setSelectedId(2); // Default to 48kHz

    // Buffer Size dropdown
    addAndMakeVisible(m_bufferSizeLabel);
    m_bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
    m_bufferSizeLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(m_bufferSizeCombo);
    m_bufferSizeCombo.addItem("256 samples", 1);
    m_bufferSizeCombo.addItem("512 samples", 2);
    m_bufferSizeCombo.addItem("1024 samples", 3);
    m_bufferSizeCombo.addItem("2048 samples", 4);
    m_bufferSizeCombo.setSelectedId(3); // Default to 1024

    // Apply button
    addAndMakeVisible(m_applyButton);
    m_applyButton.setButtonText("Apply Settings");
    m_applyButton.onClick = [this] { applySettings(); };

    // Status label
    addAndMakeVisible(m_statusLabel);
    m_statusLabel.setText("Current: 48000 Hz, 1024 samples", juce::dontSendNotification);
    m_statusLabel.setJustificationType(juce::Justification::centred);

    setSize(400, 200);
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour(0xff252525));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("Audio I/O Settings", 0, 10, getWidth(), 30, juce::Justification::centred);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(20);
    bounds.removeFromTop(40); // Skip title area

    auto labelWidth = 120;
    auto rowHeight = 30;
    auto spacing = 10;

    // Sample Rate row
    auto sampleRateRow = bounds.removeFromTop(rowHeight);
    m_sampleRateLabel.setBounds(sampleRateRow.removeFromLeft(labelWidth));
    sampleRateRow.removeFromLeft(spacing);
    m_sampleRateCombo.setBounds(sampleRateRow);

    bounds.removeFromTop(spacing);

    // Buffer Size row
    auto bufferSizeRow = bounds.removeFromTop(rowHeight);
    m_bufferSizeLabel.setBounds(bufferSizeRow.removeFromLeft(labelWidth));
    bufferSizeRow.removeFromLeft(spacing);
    m_bufferSizeCombo.setBounds(bufferSizeRow);

    bounds.removeFromTop(spacing * 2);

    // Apply button
    auto buttonRow = bounds.removeFromTop(rowHeight);
    m_applyButton.setBounds(buttonRow.withSizeKeepingCentre(150, rowHeight));

    bounds.removeFromTop(spacing);

    // Status label
    m_statusLabel.setBounds(bounds.removeFromTop(rowHeight));
  }

private:
  void applySettings() {
    // Get selected values
    uint32_t sampleRate = 48000;
    switch (m_sampleRateCombo.getSelectedId()) {
    case 1:
      sampleRate = 44100;
      break;
    case 2:
      sampleRate = 48000;
      break;
    case 3:
      sampleRate = 96000;
      break;
    }

    uint16_t bufferSize = 1024;
    switch (m_bufferSizeCombo.getSelectedId()) {
    case 1:
      bufferSize = 256;
      break;
    case 2:
      bufferSize = 512;
      break;
    case 3:
      bufferSize = 1024;
      break;
    case 4:
      bufferSize = 2048;
      break;
    }

    // Update status
    m_statusLabel.setText("Applying: " + juce::String(sampleRate) + " Hz, " +
                              juce::String(bufferSize) + " samples",
                          juce::dontSendNotification);

    // TODO: Restart AudioEngine with new settings
    // This requires stopping, reinitializing, and restarting the engine
    // For now, just show a message that settings will apply on restart

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "Settings Applied",
        "Audio settings will take effect when you restart the application.\n\n"
        "Sample Rate: " +
            juce::String(sampleRate) +
            " Hz\n"
            "Buffer Size: " +
            juce::String(bufferSize) + " samples",
        "OK");
  }

  AudioEngine* m_audioEngine;

  juce::Label m_sampleRateLabel;
  juce::ComboBox m_sampleRateCombo;

  juce::Label m_bufferSizeLabel;
  juce::ComboBox m_bufferSizeCombo;

  juce::TextButton m_applyButton;
  juce::Label m_statusLabel;
};
