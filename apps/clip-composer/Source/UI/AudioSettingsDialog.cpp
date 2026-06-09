// SPDX-License-Identifier: MIT

#include "AudioSettingsDialog.h"
#include "ConsoleTheme.h"
#include "DesignTokens.h"

#include <algorithm>

using namespace OCC::Design;

namespace {

const std::vector<uint32_t>& defaultSampleRates() {
  static const std::vector<uint32_t> rates = {44100, 48000, 96000};
  return rates;
}

const std::vector<uint32_t>& defaultBufferSizes() {
  static const std::vector<uint32_t> sizes = {64, 128, 256, 512, 1024, 2048};
  return sizes;
}

} // namespace

//==============================================================================
AudioSettingsDialog::AudioSettingsDialog(AudioEngine* engine) : m_audioEngine(engine) {
  setSize(560, 380);

  // Device row
  addAndMakeVisible(m_deviceLabel);
  m_deviceLabel.setText("Audio Device:", juce::dontSendNotification);
  m_deviceLabel.setFont(OCC::Console::consoleFont(12.0f, juce::Font::bold));
  m_deviceLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  m_deviceLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_deviceCombo);
  m_deviceCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBgInset));
  m_deviceCombo.setColour(juce::ComboBox::textColourId, juce::Colour(kTextPrimary));
  m_deviceCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(kBorderDefault));
  populateDeviceList();
  m_deviceCombo.onChange = [this] { syncSupportedOptionsForDevice(); };

  // Sample Rate row
  addAndMakeVisible(m_sampleRateLabel);
  m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
  m_sampleRateLabel.setFont(OCC::Console::consoleFont(12.0f, juce::Font::bold));
  m_sampleRateLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  m_sampleRateLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_sampleRateCombo);
  m_sampleRateCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBgInset));
  m_sampleRateCombo.setColour(juce::ComboBox::textColourId, juce::Colour(kTextPrimary));
  m_sampleRateCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(kBorderDefault));
  populateSampleRates();
  m_sampleRateCombo.onChange = [this] { refreshStatusLabels(); };

  // Buffer Size row
  addAndMakeVisible(m_bufferSizeLabel);
  m_bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
  m_bufferSizeLabel.setFont(OCC::Console::consoleFont(12.0f, juce::Font::bold));
  m_bufferSizeLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
  m_bufferSizeLabel.setJustificationType(juce::Justification::centredRight);

  addAndMakeVisible(m_bufferSizeCombo);
  m_bufferSizeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kBgInset));
  m_bufferSizeCombo.setColour(juce::ComboBox::textColourId, juce::Colour(kTextPrimary));
  m_bufferSizeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(kBorderDefault));
  populateBufferSizes();
  m_bufferSizeCombo.onChange = [this] { refreshStatusLabels(); };

  // Apply button - Primary
  m_applyButton = std::make_unique<ConsoleActionButton>("apply-settings", ConsoleActionButton::Variant::Primary);
  m_applyButton->setLabel("APPLY SETTINGS");
  m_applyButton->onClick = [this] { applySettings(); };
  addAndMakeVisible(m_applyButton.get());

  // Close button - Default
  m_closeButton = std::make_unique<ConsoleActionButton>("close-settings", ConsoleActionButton::Variant::Default);
  m_closeButton->setLabel("CLOSE");
  m_closeButton->onClick = [this] {
    if (onCloseClicked) {
      onCloseClicked();
    }
  };
  addAndMakeVisible(m_closeButton.get());

  // Status label
  addAndMakeVisible(m_statusLabel);
  m_statusLabel.setFont(OCC::Console::monoFont(11.0f));
  m_statusLabel.setJustificationType(juce::Justification::centredLeft);
  m_statusLabel.setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));

  // Detail label
  addAndMakeVisible(m_detailLabel);
  m_detailLabel.setFont(OCC::Console::monoFont(10.0f));
  m_detailLabel.setJustificationType(juce::Justification::topLeft);
  m_detailLabel.setColour(juce::Label::textColourId, juce::Colour(kTextSecondary));

  // Initialize with current settings
  if (m_audioEngine) {
    const auto currentSampleRate = m_audioEngine->getSampleRate();
    const auto currentBufferSize = m_audioEngine->getBufferSize();
    const auto currentDevice = m_audioEngine->getCurrentDeviceName();

    for (int i = 1; i <= m_deviceCombo.getNumItems(); ++i) {
      if (m_deviceCombo.getItemText(i - 1).toStdString() == currentDevice) {
        m_deviceCombo.setSelectedId(i, juce::dontSendNotification);
        break;
      }
    }

    syncSupportedOptionsForDevice();
    juce::ignoreUnused(currentSampleRate, currentBufferSize);
  }

  refreshStatusLabels();
}

void AudioSettingsDialog::paint(juce::Graphics& g) {
  // Console chassis background
  g.fillAll(juce::Colour(kBgSurface));

  // Title bar - eyebrow + title
  auto titleBar = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), 44.0f);
  g.setColour(juce::Colour(kBgComponent));
  g.fillRect(titleBar);
  g.setColour(juce::Colour(kBorderDefault));
  g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));

  // Eyebrow
  g.setColour(juce::Colour(kTextSecondary));
  g.setFont(OCC::Console::monoFont(10.0f, juce::Font::bold));
  g.drawText("AUDIO I/O SETTINGS", 20, 6, 200, 14, juce::Justification::centredLeft, false);

  // Bold title
  g.setColour(juce::Colour(kTextPrimary));
  g.setFont(OCC::Console::consoleFont(18.0f, juce::Font::bold));
  g.drawText("Audio I/O Settings", 20, 20, getWidth() - 40, 22, juce::Justification::centredLeft, false);
}

void AudioSettingsDialog::resized() {
  auto bounds = getLocalBounds().reduced(20);
  bounds.removeFromTop(50); // Title bar area

  const int labelWidth = 120;
  const int rowHeight = 36;
  const int spacing = 10;

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

  // Buttons row
  auto buttonRow = bounds.removeFromTop(rowHeight);
  const int totalButtonWidth = 150 + 10 + 100;
  const int availableWidth = buttonRow.getWidth();
  const int leftMargin = availableWidth > totalButtonWidth
      ? (availableWidth - totalButtonWidth) / 2
      : 4; // Minimum margin if space is tight
  if (leftMargin > 0)
    buttonRow.removeFromLeft(leftMargin);
  if (m_applyButton)
    m_applyButton->setBounds(buttonRow.removeFromLeft(juce::jmin(150, buttonRow.getWidth())).reduced(0, 2));
  buttonRow.removeFromLeft(10);
  if (m_closeButton)
    m_closeButton->setBounds(buttonRow.removeFromLeft(juce::jmin(100, buttonRow.getWidth())).reduced(0, 2));

  bounds.removeFromTop(spacing);
  m_statusLabel.setBounds(bounds.removeFromTop(rowHeight));
  bounds.removeFromTop(spacing / 2);
  m_detailLabel.setBounds(bounds);
}

//==============================================================================
void AudioSettingsDialog::applySettings() {
  if (!m_audioEngine) {
    m_statusLabel.setText("Error: Audio engine not available", juce::dontSendNotification);
    return;
  }

  const auto deviceName = m_deviceCombo.getText().toStdString();
  const auto sampleRate = selectedValueFor(m_sampleRateCombo, 48000);
  const auto bufferSize = selectedValueFor(m_bufferSizeCombo, 512);

  refreshStatusLabels("Applying " + juce::String(sampleRate) + " Hz / " + juce::String(bufferSize) +
                      " samples...");

  const bool success = m_audioEngine->setAudioDevice(deviceName, sampleRate, bufferSize);
  if (success) {
    saveSettings(deviceName, sampleRate, bufferSize);
    refreshStatusLabels("Audio settings applied");

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
            juce::String((bufferSize / static_cast<double>(sampleRate)) * 1000.0, 2) + " ms",
        "OK");
    return;
  }

  refreshStatusLabels("Failed to apply settings");
  const auto status = m_audioEngine->getAudioDeviceStatus();
  juce::String failureDetail = juce::String(status.lastError);
  if (failureDetail.isEmpty()) {
    failureDetail = "The audio engine rejected the requested configuration.";
  }

  juce::AlertWindow::showMessageBoxAsync(
      juce::AlertWindow::WarningIcon, "Settings Failed",
      "Could not apply audio settings.\n\n" + failureDetail +
          "\n\nPlease choose a supported device, sample rate, and buffer size.",
      "OK");
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

  if (m_deviceCombo.getSelectedId() == 0 && m_deviceCombo.getNumItems() > 0) {
    m_deviceCombo.setSelectedId(1, juce::dontSendNotification);
  }
}

void AudioSettingsDialog::populateSampleRates(const std::vector<uint32_t>& supportedRates,
                                              uint32_t preferredRate) {
  const auto& rates = supportedRates.empty() ? defaultSampleRates() : supportedRates;
  m_sampleRateCombo.clear();
  for (size_t i = 0; i < rates.size(); ++i) {
    m_sampleRateCombo.addItem(juce::String(rates[i]) + " Hz", static_cast<int>(i + 1));
  }
  m_sampleRateCombo.setSelectedId(idForValue(preferredRate, rates), juce::dontSendNotification);
}

void AudioSettingsDialog::populateBufferSizes(const std::vector<uint32_t>& supportedBufferSizes,
                                              uint32_t preferredBufferSize) {
  const auto& sizes = supportedBufferSizes.empty() ? defaultBufferSizes() : supportedBufferSizes;
  m_bufferSizeCombo.clear();
  for (size_t i = 0; i < sizes.size(); ++i) {
    m_bufferSizeCombo.addItem(juce::String(sizes[i]) + " samples", static_cast<int>(i + 1));
  }
  m_bufferSizeCombo.setSelectedId(idForValue(preferredBufferSize, sizes),
                                  juce::dontSendNotification);
}

void AudioSettingsDialog::saveSettings(const std::string& deviceName, uint32_t sampleRate,
                                       uint32_t bufferSize) {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";

  juce::PropertiesFile settings(options);

  settings.setValue("audioDevice", juce::String(deviceName));
  settings.setValue("sampleRate", static_cast<int>(sampleRate));
  settings.setValue("bufferSize", static_cast<int>(bufferSize));
  settings.saveIfNeeded();

  DBG("AudioSettingsDialog: Saved settings - Device: " << deviceName
                         << ", SR: " << static_cast<int>(sampleRate)
                         << " Hz, Buffer: " << static_cast<int>(bufferSize));
}

void AudioSettingsDialog::loadSavedSettings() {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";

  juce::PropertiesFile settings(options);
  juce::ignoreUnused(settings);
}

void AudioSettingsDialog::syncSupportedOptionsForDevice() {
  if (!m_audioEngine) {
    return;
  }

  const auto selectedDevice = m_deviceCombo.getText().toStdString();
  const auto preferredRate = selectedValueFor(m_sampleRateCombo, m_audioEngine->getSampleRate());
  const auto preferredBuffer = selectedValueFor(m_bufferSizeCombo, m_audioEngine->getBufferSize());

  if (const auto details = m_audioEngine->getDeviceDetails(selectedDevice)) {
    populateSampleRates(details->supportedSampleRates, preferredRate);
    populateBufferSizes(details->supportedBufferSizes, preferredBuffer);
  } else {
    populateSampleRates({}, preferredRate);
    populateBufferSizes({}, preferredBuffer);
  }

  refreshStatusLabels();
}

void AudioSettingsDialog::refreshStatusLabels(const juce::String& transientStatus) {
  if (!m_audioEngine) {
    m_statusLabel.setText("Audio engine unavailable", juce::dontSendNotification);
    m_detailLabel.setText({}, juce::dontSendNotification);
    return;
  }

  const auto status = m_audioEngine->getAudioDeviceStatus();
  const auto selectedRate = selectedValueFor(m_sampleRateCombo, status.sampleRate);
  const auto selectedBuffer = selectedValueFor(m_bufferSizeCombo, status.bufferSize);
  const auto selectedDevice = m_deviceCombo.getText().isEmpty()
                                  ? juce::String(status.requestedDeviceName)
                                  : m_deviceCombo.getText();

  juce::String statusText = transientStatus;
  if (statusText.isEmpty()) {
    statusText = status.lastError.empty() ? juce::String(status.summary)
                                          : "Current issue: " + juce::String(status.lastError);
  }
  m_statusLabel.setText(statusText, juce::dontSendNotification);

  const double latencyMs =
      (static_cast<double>(status.latencySamples) / std::max(1u, status.sampleRate)) * 1000.0;

  juce::String details;
  details << "Selected: " << selectedDevice << " | " << juce::String(static_cast<int>(selectedRate))
          << " Hz | " << juce::String(static_cast<int>(selectedBuffer)) << " samples\n";
  details << "Current: " << juce::String(status.activeDeviceName) << " via "
          << juce::String(status.driverName) << " | ";

  if (status.initialized) {
    details << juce::String(status.sampleRate) << " Hz | " << juce::String(status.bufferSize)
            << " samples | " << juce::String(latencyMs, 2) << " ms latency | "
            << (status.running ? "Running" : "Stopped");
  } else {
    details << "Not initialized";
  }

  if (status.usingFallbackDriver) {
    details << "\nFallback driver active";
  }

  details << "\nPlayout: Groups 1-4 via " << juce::String(status.activeDeviceName);
  details << "\nAudition: Dedicated cue buss via " << juce::String(status.activeDeviceName);
  if (!status.lastError.empty()) {
    details << "\nValidation: " << juce::String(status.lastError);
  } else if (status.initialized) {
    details << "\nValidation: Device and routing available";
  }

  m_detailLabel.setText(details, juce::dontSendNotification);
}

int AudioSettingsDialog::idForValue(uint32_t value, const std::vector<uint32_t>& values) {
  const auto it = std::find(values.begin(), values.end(), value);
  if (it == values.end()) {
    return values.empty() ? 0 : 1;
  }

  return static_cast<int>(std::distance(values.begin(), it) + 1);
}

uint32_t AudioSettingsDialog::selectedValueFor(const juce::ComboBox& comboBox, uint32_t fallback) {
  const auto text = comboBox.getText().upToFirstOccurrenceOf(" ", false, false);
  if (text.isNotEmpty()) {
    return static_cast<uint32_t>(text.getIntValue());
  }

  return fallback;
}