// SPDX-License-Identifier: MIT

#include "WaveformDisplay.h"

//==============================================================================
WaveformDisplay::WaveformDisplay() {
  setOpaque(false);
}

//==============================================================================
void WaveformDisplay::setAudioFile(const juce::File& audioFile) {
  if (!audioFile.existsAsFile()) {
    DBG("WaveformDisplay: Audio file does not exist: " << audioFile.getFullPathName());
    return;
  }

  // Generate waveform data on background thread
  m_isLoading.store(true);

  juce::Thread::launch([this, audioFile]() {
    generateWaveformData(audioFile);

    m_isLoading.store(false);

    // Trigger repaint on message thread
    juce::MessageManager::callAsync([this]() { repaint(); });
  });
}

void WaveformDisplay::setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples) {
  m_trimInSamples = trimInSamples;
  m_trimOutSamples = trimOutSamples;
  repaint();
}

void WaveformDisplay::clear() {
  juce::ScopedLock lock(m_dataLock);
  m_waveformData = WaveformData();
  m_trimInSamples = 0;
  m_trimOutSamples = 0;
  repaint();
}

//==============================================================================
void WaveformDisplay::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRect(bounds);

  // Loading state
  if (m_isLoading.load()) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
    g.drawText("Loading waveform...", bounds, juce::Justification::centred);
    return;
  }

  // Draw waveform if data is valid
  {
    juce::ScopedLock lock(m_dataLock);
    if (m_waveformData.isValid) {
      drawWaveform(g, bounds);
      drawTrimMarkers(g, bounds);
    } else {
      g.setColour(juce::Colours::white.withAlpha(0.3f));
      g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
      g.drawText("No waveform data", bounds, juce::Justification::centred);
    }
  }
}

void WaveformDisplay::resized() {
  // Waveform data is resolution-dependent, regenerate if needed
  // For now, just repaint
  repaint();
}

//==============================================================================
void WaveformDisplay::generateWaveformData(const juce::File& audioFile) {
  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats(); // WAV, AIFF

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));

  if (!reader) {
    DBG("WaveformDisplay: Failed to create reader for: " << audioFile.getFullPathName());
    return;
  }

  WaveformData newData;
  newData.sampleRate = static_cast<int>(reader->sampleRate);
  newData.numChannels = static_cast<int>(reader->numChannels);
  newData.totalSamples = reader->lengthInSamples;

  // Target width (pixels) - use current component width or default to 800
  int targetWidth = getWidth();
  if (targetWidth <= 0)
    targetWidth = 800;

  // Downsample: samples per pixel column
  int64_t samplesPerPixel = std::max<int64_t>(1, newData.totalSamples / targetWidth);

  newData.minValues.resize(targetWidth, 0.0f);
  newData.maxValues.resize(targetWidth, 0.0f);

  // Read audio in chunks
  const int bufferSize = 8192;
  juce::AudioBuffer<float> buffer(newData.numChannels, bufferSize);

  int64_t samplesRead = 0;
  int pixelIndex = 0;

  while (samplesRead < newData.totalSamples && pixelIndex < targetWidth) {
    int64_t samplesToRead = std::min<int64_t>(bufferSize, newData.totalSamples - samplesRead);

    reader->read(&buffer, 0, static_cast<int>(samplesToRead), samplesRead, true, true);

    // Process buffer to find min/max for current pixel columns
    for (int i = 0; i < samplesToRead; ++i) {
      int64_t globalSampleIndex = samplesRead + i;
      int currentPixel = static_cast<int>(globalSampleIndex / samplesPerPixel);

      if (currentPixel >= targetWidth)
        break;

      // Mix all channels to mono for waveform display
      float sampleValue = 0.0f;
      for (int ch = 0; ch < newData.numChannels; ++ch) {
        sampleValue += buffer.getSample(ch, i);
      }
      sampleValue /= static_cast<float>(newData.numChannels);

      // Track min/max for this pixel column
      if (globalSampleIndex % samplesPerPixel == 0) {
        // Start new pixel column
        newData.minValues[currentPixel] = sampleValue;
        newData.maxValues[currentPixel] = sampleValue;
      } else {
        newData.minValues[currentPixel] = std::min(newData.minValues[currentPixel], sampleValue);
        newData.maxValues[currentPixel] = std::max(newData.maxValues[currentPixel], sampleValue);
      }
    }

    samplesRead += samplesToRead;
  }

  newData.isValid = true;

  // Update member data (thread-safe)
  {
    juce::ScopedLock lock(m_dataLock);
    m_waveformData = std::move(newData);
  }

  DBG("WaveformDisplay: Generated waveform with " << targetWidth << " pixels, "
                                                  << newData.totalSamples << " samples");
}

void WaveformDisplay::drawWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  if (m_waveformData.minValues.empty() || m_waveformData.maxValues.empty())
    return;

  const float width = bounds.getWidth();
  const float height = bounds.getHeight();
  const float midY = bounds.getCentreY();
  const int numPixels = static_cast<int>(m_waveformData.minValues.size());

  // Draw waveform as vertical lines (min to max per pixel column)
  g.setColour(juce::Colour(0xff4a9eff)); // Light blue

  for (int i = 0; i < numPixels; ++i) {
    float x = bounds.getX() + (i / static_cast<float>(numPixels)) * width;

    float minVal = m_waveformData.minValues[i];
    float maxVal = m_waveformData.maxValues[i];

    // Scale to bounds (±1.0 maps to ±half height)
    float y1 = midY + (minVal * height * 0.45f);
    float y2 = midY + (maxVal * height * 0.45f);

    // Ensure at least 1px line
    if (std::abs(y2 - y1) < 1.0f) {
      y2 = y1 + 1.0f;
    }

    g.drawLine(x, y1, x, y2, 1.0f);
  }

  // Draw center line
  g.setColour(juce::Colours::white.withAlpha(0.2f));
  g.drawLine(bounds.getX(), midY, bounds.getRight(), midY, 1.0f);
}

void WaveformDisplay::drawTrimMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  if (m_waveformData.totalSamples == 0)
    return;

  const float width = bounds.getWidth();

  // Calculate trim marker positions
  float trimInX =
      bounds.getX() + (m_trimInSamples / static_cast<float>(m_waveformData.totalSamples)) * width;
  float trimOutX =
      bounds.getX() + (m_trimOutSamples / static_cast<float>(m_waveformData.totalSamples)) * width;

  // Trim In marker (green)
  g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
  g.drawLine(trimInX, bounds.getY(), trimInX, bounds.getBottom(), 2.0f);
  g.fillRect(trimInX - 3.0f, bounds.getY(), 6.0f, 12.0f); // Small handle at top

  // Trim Out marker (red)
  g.setColour(juce::Colour(0xffff0000).withAlpha(0.7f));
  g.drawLine(trimOutX, bounds.getY(), trimOutX, bounds.getBottom(), 2.0f);
  g.fillRect(trimOutX - 3.0f, bounds.getY(), 6.0f, 12.0f); // Small handle at top

  // Shaded regions outside trim points
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.fillRect(bounds.getX(), bounds.getY(), trimInX - bounds.getX(),
             bounds.getHeight()); // Before trim in
  g.fillRect(trimOutX, bounds.getY(), bounds.getRight() - trimOutX,
             bounds.getHeight()); // After trim out
}
