// SPDX-License-Identifier: MIT
#pragma once

#include "../Audio/AudioEngine.h"
#include <juce_gui_extra/juce_gui_extra.h>

/// Audio I/O Settings Dialog
/// Allows user to configure sample rate, buffer size, and audio device
class AudioSettingsDialog : public juce::Component {
public:
  AudioSettingsDialog(AudioEngine* engine);

  std::function<void()> onCloseClicked;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  void applySettings();
  void populateDeviceList();
  void populateSampleRates(const std::vector<uint32_t>& supportedRates = {},
                           uint32_t preferredRate = 48000);
  void populateBufferSizes(const std::vector<uint32_t>& supportedBufferSizes = {},
                           uint32_t preferredBufferSize = 512);
  void loadSavedSettings();
  void saveSettings(const std::string& deviceName, uint32_t sampleRate, uint32_t bufferSize);
  void syncSupportedOptionsForDevice();
  void refreshStatusLabels(const juce::String& transientStatus = {});
  static int idForValue(uint32_t value, const std::vector<uint32_t>& values);
  static uint32_t selectedValueFor(const juce::ComboBox& comboBox, uint32_t fallback);

  AudioEngine* m_audioEngine;

  juce::Label m_deviceLabel;
  juce::ComboBox m_deviceCombo;

  juce::Label m_sampleRateLabel;
  juce::ComboBox m_sampleRateCombo;

  juce::Label m_bufferSizeLabel;
  juce::ComboBox m_bufferSizeCombo;

  juce::TextButton m_applyButton;
  juce::TextButton m_closeButton;
  juce::Label m_statusLabel;
  juce::Label m_detailLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsDialog)
};
