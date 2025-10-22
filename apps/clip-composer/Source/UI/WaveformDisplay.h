// SPDX-License-Identifier: MIT

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

//==============================================================================
/**
 * WaveformDisplay - Component for rendering audio waveforms
 *
 * Features:
 * - Efficient downsampled waveform rendering
 * - Visual trim point markers
 * - Interactive scrubbing (future)
 * - Support for stereo/mono files
 *
 * Threading:
 * - Waveform data generation happens on background thread
 * - Rendering happens on message thread (paint())
 * - Thread-safe via atomic flag and mutex
 */
class WaveformDisplay : public juce::Component {
public:
  //==============================================================================
  WaveformDisplay();
  ~WaveformDisplay() override = default;

  //==============================================================================
  // Load audio file and generate waveform data
  void setAudioFile(const juce::File& audioFile);

  // Set trim points (in samples) - updates visual markers
  void setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples);

  // Clear waveform data
  void clear();

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  struct WaveformData {
    std::vector<float> minValues; // Min sample value per pixel column
    std::vector<float> maxValues; // Max sample value per pixel column
    int sampleRate = 48000;
    int numChannels = 2;
    int64_t totalSamples = 0;
    bool isValid = false;
  };

  void generateWaveformData(const juce::File& audioFile);
  void drawWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds);
  void drawTrimMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds);

  //==============================================================================
  WaveformData m_waveformData;
  int64_t m_trimInSamples = 0;
  int64_t m_trimOutSamples = 0;
  std::atomic<bool> m_isLoading{false};

  juce::CriticalSection m_dataLock; // Protects waveform data during background generation

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
