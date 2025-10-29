// SPDX-License-Identifier: MIT

#include "AudioSettingsDialog.h"

//==============================================================================
AudioSettingsDialog::AudioSettingsDialog(AudioEngine* engine) : m_audioEngine(engine) {
  // Device dropdown
  addAndMakeVisible(m_deviceLabel);
  m_deviceLabel.setText("Audio Device:", juce::dontSendNotification);
  m_deviceLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_deviceCombo);
  populateDeviceList();

  // Sample Rate dropdown
  addAndMakeVisible(m_sampleRateLabel);
  m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
  m_sampleRateLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_sampleRateCombo);
  populateSampleRates();

  // Buffer Size dropdown
  addAndMakeVisible(m_bufferSizeLabel);
  m_bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
  m_bufferSizeLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_bufferSizeCombo);
  populateBufferSizes();

  // Apply button
  addAndMakeVisible(m_applyButton);
  m_applyButton.setButtonText("Apply Settings");
  m_applyButton.onClick = [this] { applySettings(); };

  // Close button
  addAndMakeVisible(m_closeButton);
  m_closeButton.setButtonText("Close");
  m_closeButton.onClick = [this] {
    if (onCloseClicked) {
      onCloseClicked();
    }
  };

  // Status label
  addAndMakeVisible(m_statusLabel);
  m_statusLabel.setJustificationType(juce::Justification::centred);

  // Load current settings from engine
  if (m_audioEngine) {
    uint32_t currentSampleRate = m_audioEngine->getSampleRate();
    uint32_t currentBufferSize = m_audioEngine->getBufferSize();
    std::string currentDevice = m_audioEngine->getCurrentDeviceName();

    // Set device combo
    for (int i = 1; i <= m_deviceCombo.getNumItems(); ++i) {
      if (m_deviceCombo.getItemText(i - 1).toStdString() == currentDevice) {
        m_deviceCombo.setSelectedId(i, juce::dontSendNotification);
        break;
      }
    }

    // Set sample rate combo
    if (currentSampleRate == 44100)
      m_sampleRateCombo.setSelectedId(1, juce::dontSendNotification);
    else if (currentSampleRate == 48000)
      m_sampleRateCombo.setSelectedId(2, juce::dontSendNotification);
    else if (currentSampleRate == 96000)
      m_sampleRateCombo.setSelectedId(3, juce::dontSendNotification);

    // Set buffer size combo
    if (currentBufferSize == 64)
      m_bufferSizeCombo.setSelectedId(1, juce::dontSendNotification);
    else if (currentBufferSize == 128)
      m_bufferSizeCombo.setSelectedId(2, juce::dontSendNotification);
    else if (currentBufferSize == 256)
      m_bufferSizeCombo.setSelectedId(3, juce::dontSendNotification);
    else if (currentBufferSize == 512)
      m_bufferSizeCombo.setSelectedId(4, juce::dontSendNotification);
    else if (currentBufferSize == 1024)
      m_bufferSizeCombo.setSelectedId(5, juce::dontSendNotification);
    else if (currentBufferSize == 2048)
      m_bufferSizeCombo.setSelectedId(6, juce::dontSendNotification);

    // Update status label
    m_statusLabel.setText("Current: " + juce::String(currentSampleRate) + " Hz, " +
                              juce::String(currentBufferSize) + " samples",
                          juce::dontSendNotification);
  }

  setSize(500, 300); // Increased height to prevent button clipping
}

void AudioSettingsDialog::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff252525));

  g.setColour(juce::Colours::white);
  g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
  g.drawText("Audio I/O Settings", 0, 10, getWidth(), 30, juce::Justification::centred);
}

void AudioSettingsDialog::resized() {
  auto bounds = getLocalBounds().reduced(20);
  bounds.removeFromTop(40); // Skip title area

  auto labelWidth = 120;
  auto rowHeight = 35; // Increased from 30 to give more vertical space
  auto spacing = 10;

  // Device row
  auto deviceRow = bounds.removeFromTop(rowHeight);
  m_deviceLabel.setBounds(deviceRow.removeFromLeft(labelWidth));
  deviceRow.removeFromLeft(spacing);
  m_deviceCombo.setBounds(deviceRow);

  bounds.removeFromTop(spacing);

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

  // Apply and Close buttons side-by-side with proper height, centered horizontally
  auto buttonRow = bounds.removeFromTop(rowHeight);
  int totalButtonWidth = 150 + 10 + 100; // Apply width + spacing + Close width
  int leftMargin = (buttonRow.getWidth() - totalButtonWidth) / 2;
  buttonRow.removeFromLeft(leftMargin);                                 // Center the button group
  m_applyButton.setBounds(buttonRow.removeFromLeft(150).reduced(0, 2)); // Slight vertical padding
  buttonRow.removeFromLeft(10);                                         // Spacing
  m_closeButton.setBounds(buttonRow.removeFromLeft(100).reduced(0, 2)); // Slight vertical padding

  bounds.removeFromTop(spacing);

  // Status label
  m_statusLabel.setBounds(bounds.removeFromTop(rowHeight));
}

//==============================================================================
void AudioSettingsDialog::applySettings() {
  if (!m_audioEngine) {
    m_statusLabel.setText("Error: Audio engine not available", juce::dontSendNotification);
    return;
  }

  // Get selected device
  std::string deviceName = m_deviceCombo.getText().toStdString();

  // Get selected sample rate
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

  // Get selected buffer size
  uint32_t bufferSize = 512;
  switch (m_bufferSizeCombo.getSelectedId()) {
  case 1:
    bufferSize = 64;
    break;
  case 2:
    bufferSize = 128;
    break;
  case 3:
    bufferSize = 256;
    break;
  case 4:
    bufferSize = 512;
    break;
  case 5:
    bufferSize = 1024;
    break;
  case 6:
    bufferSize = 2048;
    break;
  }

  // Update status
  m_statusLabel.setText("Applying: " + juce::String(sampleRate) + " Hz, " +
                            juce::String(bufferSize) + " samples...",
                        juce::dontSendNotification);

  // Apply settings to audio engine
  bool success = m_audioEngine->setAudioDevice(deviceName, sampleRate, bufferSize);

  if (success) {
    // Save settings to preferences
    saveSettings(deviceName, sampleRate, bufferSize);

    m_statusLabel.setText("Settings applied successfully!", juce::dontSendNotification);

    // Show success message
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "Audio Settings Applied",
        "New audio settings:\n\n"
        "Device: " +
            juce::String(deviceName) +
            "\n"
            "Sample Rate: " +
            juce::String(sampleRate) +
            " Hz\n"
            "Buffer Size: " +
            juce::String(bufferSize) +
            " samples\n\n"
            "Latency: " +
            juce::String((bufferSize / (double)sampleRate) * 1000.0, 2) + " ms",
        "OK");
  } else {
    m_statusLabel.setText("Failed to apply settings", juce::dontSendNotification);

    // Show error message
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon, "Settings Failed",
        "Could not apply audio settings.\n\n"
        "The audio engine failed to reinitialize with the requested configuration.\n\n"
        "Please check the console for error details.",
        "OK");
  }
}

void AudioSettingsDialog::populateDeviceList() {
  m_deviceCombo.clear();

  if (m_audioEngine) {
    auto devices = m_audioEngine->getAvailableDevices();
    int id = 1;
    for (const auto& device : devices) {
      m_deviceCombo.addItem(juce::String(device), id++);
    }
  }

  // Default to first device if nothing selected
  if (m_deviceCombo.getSelectedId() == 0 && m_deviceCombo.getNumItems() > 0) {
    m_deviceCombo.setSelectedId(1, juce::dontSendNotification);
  }
}

void AudioSettingsDialog::populateSampleRates() {
  m_sampleRateCombo.clear();
  m_sampleRateCombo.addItem("44100 Hz", 1);
  m_sampleRateCombo.addItem("48000 Hz", 2);
  m_sampleRateCombo.addItem("96000 Hz", 3);
  m_sampleRateCombo.setSelectedId(2, juce::dontSendNotification); // Default to 48kHz
}

void AudioSettingsDialog::populateBufferSizes() {
  m_bufferSizeCombo.clear();
  m_bufferSizeCombo.addItem("64 samples", 1);
  m_bufferSizeCombo.addItem("128 samples", 2);
  m_bufferSizeCombo.addItem("256 samples", 3);
  m_bufferSizeCombo.addItem("512 samples", 4);
  m_bufferSizeCombo.addItem("1024 samples", 5);
  m_bufferSizeCombo.addItem("2048 samples", 6);
  m_bufferSizeCombo.setSelectedId(4, juce::dontSendNotification); // Default to 512
}

void AudioSettingsDialog::saveSettings(const std::string& deviceName, uint32_t sampleRate,
                                       uint32_t bufferSize) {
  // Use JUCE PropertiesFile for persistent storage
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";

  juce::PropertiesFile settings(options);

  settings.setValue("audioDevice", juce::String(deviceName));
  settings.setValue("sampleRate", static_cast<int>(sampleRate));
  settings.setValue("bufferSize", static_cast<int>(bufferSize));

  settings.saveIfNeeded();

  DBG("AudioSettingsDialog: Saved settings - Device: "
      << deviceName << ", SR: " << static_cast<int>(sampleRate)
      << " Hz, Buffer: " << static_cast<int>(bufferSize));
}

void AudioSettingsDialog::loadSavedSettings() {
  // Use JUCE PropertiesFile to load saved settings
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";

  juce::PropertiesFile settings(options);

  // Note: This is called from constructor, but we already load current settings
  // from AudioEngine in the constructor. This method is here for future use
  // when we want to restore settings on app launch (before AudioEngine is created).
}
