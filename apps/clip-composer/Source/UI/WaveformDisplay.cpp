// SPDX-License-Identifier: MIT

#include "WaveformDisplay.h"

//==============================================================================
WaveformDisplay::WaveformDisplay() {
  setOpaque(false);
}

WaveformDisplay::~WaveformDisplay() {
  // Stop any background thread from accessing this component after destruction
  m_isLoading.store(false);
}

//==============================================================================
void WaveformDisplay::setAudioFile(const juce::File& audioFile) {
  if (!audioFile.existsAsFile()) {
    DBG("WaveformDisplay: Audio file does not exist: " << audioFile.getFullPathName());
    return;
  }

  juce::String filePath = audioFile.getFullPathName();

  // Check if this file is already cached
  if (filePath == m_cachedFilePath && m_waveformData.isValid) {
    DBG("WaveformDisplay: Using cached waveform for " << audioFile.getFileName());
    repaint();
    return; // Already loaded, no need to regenerate
  }

  // Check if this file exists in cache map
  auto cachedData = m_waveformCache.find(filePath);
  if (cachedData != m_waveformCache.end()) {
    DBG("WaveformDisplay: Restoring waveform from cache for " << audioFile.getFileName());
    juce::ScopedLock lock(m_dataLock);
    m_waveformData = cachedData->second;
    m_cachedFilePath = filePath;
    repaint();
    return;
  }

  // Not in cache - generate waveform data on background thread
  m_isLoading.store(true);

  // CRITICAL: Use SafePointer to prevent use-after-free if component is destroyed
  // while background thread is running
  juce::Component::SafePointer<WaveformDisplay> safeThis(this);

  juce::Thread::launch([safeThis, audioFile, filePath]() {
    // Check if component still exists before accessing it
    if (auto* self = safeThis.getComponent()) {
      self->generateWaveformData(audioFile);
      self->m_isLoading.store(false);

      // Store in cache (limit to 5 most recent waveforms to prevent memory bloat)
      {
        juce::ScopedLock lock(self->m_dataLock);
        self->m_waveformCache[filePath] = self->m_waveformData;
        self->m_cachedFilePath = filePath;

        // Limit cache size to 5 files
        if (self->m_waveformCache.size() > 5) {
          // Remove oldest entry (first in map)
          self->m_waveformCache.erase(self->m_waveformCache.begin());
          DBG("WaveformDisplay: Cache full, evicted oldest waveform");
        }
      }

      // Trigger repaint on message thread (check again if component still exists)
      juce::MessageManager::callAsync([safeThis]() {
        if (auto* self = safeThis.getComponent()) {
          self->repaint();
        }
      });

      DBG("WaveformDisplay: Cached waveform for "
          << audioFile.getFileName() << " (cache size: " << self->m_waveformCache.size() << ")");
    }
  });
}

void WaveformDisplay::setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples) {
  m_trimInSamples = trimInSamples;
  m_trimOutSamples = trimOutSamples;
  DBG("WaveformDisplay: setTrimPoints called with [" << trimInSamples << ", " << trimOutSamples
                                                     << "]");
  repaint();
}

void WaveformDisplay::setPlayheadPosition(int64_t samplePosition) {
  m_playheadPosition = samplePosition;

  // Paginated playhead chase (scroll viewport when playhead reaches edges)
  if (m_zoomFactor > 1.0f && m_waveformData.totalSamples > 0 && samplePosition > 0) {
    float playheadNormalized = samplePosition / static_cast<float>(m_waveformData.totalSamples);
    float visibleWidth = 1.0f / m_zoomFactor; // Fixed zoom window size
    float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
    float endFraction = m_zoomCenter + (visibleWidth / 2.0f);

    // Clamp viewport to boundaries
    startFraction = std::clamp(startFraction, 0.0f, 1.0f);
    endFraction = std::clamp(endFraction, 0.0f, 1.0f);

    // Check if playhead is approaching edges (within 10% margin) - then scroll viewport
    float distanceFromStart = playheadNormalized - startFraction;
    float distanceFromEnd = endFraction - playheadNormalized;
    float edgeThreshold = visibleWidth * 0.15f; // 15% margin for smooth pagination

    if (distanceFromEnd < edgeThreshold && endFraction < 1.0f) {
      // Playhead approaching right edge - scroll viewport right (page forward)
      m_zoomCenter =
          std::min(1.0f - (visibleWidth / 2.0f), playheadNormalized + (visibleWidth / 4.0f));
    } else if (distanceFromStart < edgeThreshold && startFraction > 0.0f) {
      // Playhead approaching left edge - scroll viewport left (page backward)
      m_zoomCenter = std::max((visibleWidth / 2.0f), playheadNormalized - (visibleWidth / 4.0f));
    }
  }

  repaint();
}

void WaveformDisplay::setZoomLevel(int level, float centerNormalized) {
  m_zoomLevel = std::clamp(level, 0, 4); // 0-4 for 5 levels

  // Convert level to zoom factor (1x, 2x, 4x, 8x, 16x)
  switch (m_zoomLevel) {
  case 0:
    m_zoomFactor = 1.0f;
    break;
  case 1:
    m_zoomFactor = 2.0f;
    break;
  case 2:
    m_zoomFactor = 4.0f;
    break;
  case 3:
    m_zoomFactor = 8.0f;
    break;
  case 4:
    m_zoomFactor = 16.0f;
    break;
  }

  // Update zoom center if provided (e.g., zoom to playhead position)
  if (centerNormalized >= 0.0f) {
    m_zoomCenter = std::clamp(centerNormalized, 0.0f, 1.0f);
  }

  DBG("WaveformDisplay: Zoom level set to " << m_zoomLevel << " (" << m_zoomFactor << "x)"
                                            << ", center: " << m_zoomCenter);
  repaint();
}

void WaveformDisplay::clear() {
  juce::ScopedLock lock(m_dataLock);
  m_waveformData = WaveformData();
  m_trimInSamples = 0;
  m_trimOutSamples = 0;
  m_zoomLevel = 0;
  m_zoomFactor = 1.0f;
  m_zoomCenter = 0.5f;
  repaint();
}

//==============================================================================
void WaveformDisplay::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Reserve 30px at bottom for time scale
  const float timeScaleHeight = 30.0f;
  auto waveformArea = bounds.withTrimmedBottom(timeScaleHeight);
  auto timeScaleArea = bounds.withTop(bounds.getBottom() - timeScaleHeight);

  // Background (waveform area)
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRect(waveformArea);

  // Background (time scale area) - slightly darker
  g.setColour(juce::Colour(0xff0f0f0f));
  g.fillRect(timeScaleArea);

  // Loading state
  if (m_isLoading.load()) {
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
    g.drawText("Loading waveform...", waveformArea, juce::Justification::centred);
    return;
  }

  // Draw waveform if data is valid
  {
    juce::ScopedLock lock(m_dataLock);
    if (m_waveformData.isValid) {
      drawWaveform(g, waveformArea);
      drawTrimMarkers(g, waveformArea);
      drawTimeScale(g, timeScaleArea);
    } else {
      g.setColour(juce::Colours::white.withAlpha(0.3f));
      g.setFont(juce::FontOptions("Inter", 12.0f, juce::Font::plain));
      g.drawText("No waveform data", waveformArea, juce::Justification::centred);
    }
  }
}

void WaveformDisplay::resized() {
  // Waveform data is resolution-dependent, regenerate if needed
  // For now, just repaint
  repaint();
}

bool WaveformDisplay::isNearHandle(float mouseX, float handleX, float tolerance) const {
  return std::abs(mouseX - handleX) <= tolerance;
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
  juce::ScopedLock lock(m_dataLock);

  if (!m_waveformData.isValid || m_waveformData.totalSamples == 0)
    return;

  auto bounds = getLocalBounds().toFloat();
  const float scaleWidth = 40.0f;
  auto waveformBounds = bounds.withTrimmedLeft(scaleWidth);
  float mouseX = static_cast<float>(event.x);

  // Calculate visible range based on zoom level
  float visibleWidth = 1.0f / m_zoomFactor;
  float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
  float endFraction = m_zoomCenter + (visibleWidth / 2.0f);
  startFraction = std::clamp(startFraction, 0.0f, 1.0f);
  endFraction = std::clamp(endFraction, 0.0f, 1.0f);

  // Calculate handle positions (zoom-aware)
  float trimInNormalized = m_trimInSamples / static_cast<float>(m_waveformData.totalSamples);
  float trimOutNormalized = m_trimOutSamples / static_cast<float>(m_waveformData.totalSamples);
  float trimInX =
      waveformBounds.getX() + ((trimInNormalized - startFraction) / (endFraction - startFraction)) *
                                  waveformBounds.getWidth();
  float trimOutX = waveformBounds.getX() +
                   ((trimOutNormalized - startFraction) / (endFraction - startFraction)) *
                       waveformBounds.getWidth();

  // Check if clicking near a handle (within 8 pixels) - REQUIRES Shift key to prevent accidental
  // dragging Shift+drag on handle = precise trim point adjustment
  if (event.mods.isShiftDown()) {
    if (isNearHandle(mouseX, trimInX, 8.0f)) {
      m_draggedHandle = DragHandle::TrimIn;
      setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
      DBG("WaveformDisplay: Started dragging IN handle");
      return;
    } else if (isNearHandle(mouseX, trimOutX, 8.0f)) {
      m_draggedHandle = DragHandle::TrimOut;
      setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
      DBG("WaveformDisplay: Started dragging OUT handle");
      return;
    }
  }

  // No handle grabbed - use click behavior
  m_draggedHandle = DragHandle::None;

  // Convert mouse X to sample position (zoom-aware)
  float normalizedX = (mouseX - waveformBounds.getX()) / waveformBounds.getWidth();
  normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);

  // Map from viewport coordinates to global normalized coordinates
  float globalNormalized = startFraction + normalizedX * (endFraction - startFraction);
  globalNormalized = std::clamp(globalNormalized, 0.0f, 1.0f);
  int64_t samplePosition = static_cast<int64_t>(globalNormalized * m_waveformData.totalSamples);

  // INTERACTION MODEL (v0.2.0 - Simplified Cmd-based):
  //
  // - Pure Leftclick = Jog playhead to position (Issue #3)
  // - Cmd+Leftclick = Set IN point (Issue #6)
  // - Cmd+Shift+Leftclick = Set OUT point (Issue #6)
  // - Shift+Drag on handles = Move IN/OUT points precisely (existing, kept)
  //
  // RATIONALE: Pure leftclick for jogging is most intuitive and consistent.
  // Command modifiers for IN/OUT prevent accidental changes during playback.
  // This matches standard DAW behavior (Logic Pro, Pro Tools).

  if (event.mods.isCommandDown() && event.mods.isShiftDown()) {
    // Cmd+Shift+Leftclick: Set OUT point
    if (onRightClick) {
      onRightClick(samplePosition);
    }
    DBG("WaveformDisplay: Cmd+Shift+Click → Set OUT at sample " << samplePosition);
  } else if (event.mods.isCommandDown()) {
    // Cmd+Leftclick: Set IN point
    if (onLeftClick) {
      onLeftClick(samplePosition);
    }
    DBG("WaveformDisplay: Cmd+Click → Set IN at sample " << samplePosition);
  } else if (event.mods.isLeftButtonDown()) {
    // Pure leftclick: Jog playhead (Issue #3 fix)
    if (onMiddleClick) {
      onMiddleClick(samplePosition);
    }
    DBG("WaveformDisplay: Leftclick → Jog to sample " << samplePosition);
  }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event) {
  if (m_draggedHandle == DragHandle::None)
    return;

  juce::ScopedLock lock(m_dataLock);

  if (!m_waveformData.isValid || m_waveformData.totalSamples == 0)
    return;

  auto bounds = getLocalBounds().toFloat();
  const float scaleWidth = 40.0f;
  auto waveformBounds = bounds.withTrimmedLeft(scaleWidth);
  float mouseX = static_cast<float>(event.x);

  // Calculate visible range based on zoom level
  float visibleWidth = 1.0f / m_zoomFactor;
  float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
  float endFraction = m_zoomCenter + (visibleWidth / 2.0f);
  startFraction = std::clamp(startFraction, 0.0f, 1.0f);
  endFraction = std::clamp(endFraction, 0.0f, 1.0f);

  // Convert mouse X to sample position (zoom-aware)
  float normalizedX = (mouseX - waveformBounds.getX()) / waveformBounds.getWidth();
  normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);

  // Map from viewport coordinates to global normalized coordinates
  float globalNormalized = startFraction + normalizedX * (endFraction - startFraction);
  globalNormalized = std::clamp(globalNormalized, 0.0f, 1.0f);
  int64_t samplePosition = static_cast<int64_t>(globalNormalized * m_waveformData.totalSamples);

  // Update the appropriate trim point
  if (m_draggedHandle == DragHandle::TrimIn) {
    // Don't allow dragging past OUT point
    m_trimInSamples = std::min(samplePosition, m_trimOutSamples);
    DBG("WaveformDisplay: Dragging IN handle - samplePosition="
        << samplePosition << ", m_trimOutSamples=" << m_trimOutSamples << ", result=["
        << m_trimInSamples << ", " << m_trimOutSamples << "]");
  } else if (m_draggedHandle == DragHandle::TrimOut) {
    // Don't allow dragging before IN point
    m_trimOutSamples = std::max(samplePosition, m_trimInSamples);
    DBG("WaveformDisplay: Dragging OUT handle - samplePosition="
        << samplePosition << ", m_trimInSamples=" << m_trimInSamples << ", result=["
        << m_trimInSamples << ", " << m_trimOutSamples << "]");
  }

  // Notify parent dialog
  if (onTrimPointsChanged) {
    onTrimPointsChanged(m_trimInSamples, m_trimOutSamples);
  }

  repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&) {
  if (m_draggedHandle != DragHandle::None) {
    DBG("WaveformDisplay: Finished dragging handle");
    m_draggedHandle = DragHandle::None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
  }
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
  // Quadruple resolution for fine visual edits (4 data points per pixel for 16x zoom)
  int targetWidth = getWidth() * 4;
  if (targetWidth <= 0)
    targetWidth = 3200;

  // Downsample: samples per pixel column (now 2x resolution)
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

  // Reserve space for dB scale on left (40px)
  const float scaleWidth = 40.0f;
  auto waveformBounds = bounds.withTrimmedLeft(scaleWidth);

  const float width = waveformBounds.getWidth();
  const float height = waveformBounds.getHeight();
  const float midY = waveformBounds.getCentreY();
  const int numPixels = static_cast<int>(m_waveformData.minValues.size());

  // Draw dB scale on left side (SpotOn-style)
  g.setColour(juce::Colours::white.withAlpha(0.7f));
  g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));

  // SpotOn shows: 0, -10, -20, -30, -40, -50 dB
  const float dbValues[] = {0.0f, -10.0f, -20.0f, -30.0f, -40.0f, -50.0f};
  for (float db : dbValues) {
    // Map dB to Y position (-60dB to +10dB range, but only show -50 to 0)
    // 0dB = top, -50dB = bottom (using 90% of height)
    float normalizedY = (-db / 50.0f); // 0.0 at top, 1.0 at bottom
    float y = bounds.getY() + normalizedY * height * 0.9f + height * 0.05f;

    // Draw tick mark
    g.drawLine(scaleWidth - 5.0f, y, scaleWidth - 2.0f, y, 1.0f);

    // Draw label
    juce::String label = (db == 0.0f) ? "0" : juce::String(static_cast<int>(db));
    g.drawText(label, 2, static_cast<int>(y - 6), static_cast<int>(scaleWidth - 8), 12,
               juce::Justification::centredRight, false);
  }

  // Calculate visible range based on zoom level
  float visibleWidth = 1.0f / m_zoomFactor; // Fraction of total waveform visible
  float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
  float endFraction = m_zoomCenter + (visibleWidth / 2.0f);

  // Clamp to [0, 1]
  startFraction = std::clamp(startFraction, 0.0f, 1.0f);
  endFraction = std::clamp(endFraction, 0.0f, 1.0f);

  int startPixel = static_cast<int>(startFraction * numPixels);
  int endPixel = static_cast<int>(endFraction * numPixels);

  // Draw waveform as vertical lines (min to max per pixel column)
  g.setColour(juce::Colour(0xff4a9eff)); // Light blue

  for (int i = startPixel; i < endPixel; ++i) {
    if (i < 0 || i >= numPixels)
      continue;

    // Map pixel index to screen X position
    float normalizedX = (i - startPixel) / static_cast<float>(endPixel - startPixel);
    float x = waveformBounds.getX() + normalizedX * width;

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
  g.drawLine(waveformBounds.getX(), midY, waveformBounds.getRight(), midY, 1.0f);
}

void WaveformDisplay::drawTrimMarkers(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  if (m_waveformData.totalSamples == 0)
    return;

  // Account for dB scale offset
  const float scaleWidth = 40.0f;
  auto waveformBounds = bounds.withTrimmedLeft(scaleWidth);
  const float width = waveformBounds.getWidth();

  // Calculate visible range based on zoom level (same as drawWaveform)
  float visibleWidth = 1.0f / m_zoomFactor; // Fraction of total waveform visible
  float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
  float endFraction = m_zoomCenter + (visibleWidth / 2.0f);

  // Clamp to [0, 1]
  startFraction = std::clamp(startFraction, 0.0f, 1.0f);
  endFraction = std::clamp(endFraction, 0.0f, 1.0f);

  // Calculate trim marker positions in normalized space [0, 1]
  float trimInNormalized = m_trimInSamples / static_cast<float>(m_waveformData.totalSamples);
  float trimOutNormalized = m_trimOutSamples / static_cast<float>(m_waveformData.totalSamples);

  // Map to zoomed viewport coordinates
  // If a marker is outside the visible range, it won't be drawn
  float trimInX = waveformBounds.getX() +
                  ((trimInNormalized - startFraction) / (endFraction - startFraction)) * width;
  float trimOutX = waveformBounds.getX() +
                   ((trimOutNormalized - startFraction) / (endFraction - startFraction)) * width;

  // Trim In marker (MAGENTA - SpotOn standard)
  g.setColour(juce::Colour(0xffff00ff).withAlpha(0.8f));
  g.drawLine(trimInX, bounds.getY(), trimInX, bounds.getBottom(), 2.0f);
  g.fillRect(trimInX - 3.0f, bounds.getY(), 6.0f, 12.0f); // Small handle at top

  // Trim Out marker (CYAN - SpotOn standard)
  g.setColour(juce::Colour(0xff00ffff).withAlpha(0.8f));
  g.drawLine(trimOutX, bounds.getY(), trimOutX, bounds.getBottom(), 2.0f);
  g.fillRect(trimOutX - 3.0f, bounds.getY(), 6.0f, 12.0f); // Small handle at top

  // Shaded regions outside trim points (only draw if markers are visible)
  g.setColour(juce::Colours::black.withAlpha(0.5f));

  // Shade before IN point
  if (trimInNormalized >= startFraction && trimInNormalized <= endFraction) {
    float shadeStart = waveformBounds.getX();
    float shadeEnd = trimInX;
    if (shadeEnd > shadeStart) {
      g.fillRect(shadeStart, waveformBounds.getY(), shadeEnd - shadeStart,
                 waveformBounds.getHeight());
    }
  }

  // Shade after OUT point
  if (trimOutNormalized >= startFraction && trimOutNormalized <= endFraction) {
    float shadeStart = trimOutX;
    float shadeEnd = waveformBounds.getRight();
    if (shadeEnd > shadeStart) {
      g.fillRect(shadeStart, waveformBounds.getY(), shadeEnd - shadeStart,
                 waveformBounds.getHeight());
    }
  }

  // Draw playhead (transport position bar) - YELLOW, thicker (SpotOn standard)
  if (m_playheadPosition > 0) {
    float playheadNormalized = m_playheadPosition / static_cast<float>(m_waveformData.totalSamples);

    // Only draw if playhead is in visible range
    if (playheadNormalized >= startFraction && playheadNormalized <= endFraction) {
      float playheadX =
          waveformBounds.getX() +
          ((playheadNormalized - startFraction) / (endFraction - startFraction)) * width;
      g.setColour(juce::Colour(0xffffff00).withAlpha(0.9f)); // Yellow
      g.drawLine(playheadX, waveformBounds.getY(), playheadX, waveformBounds.getBottom(),
                 3.0f); // Thicker (3.0f)
    }
  }
}

void WaveformDisplay::drawTimeScale(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  if (m_waveformData.totalSamples == 0)
    return;

  // Account for dB scale offset (same as waveform)
  const float scaleWidth = 40.0f;
  auto timeScaleBounds = bounds.withTrimmedLeft(scaleWidth);
  const float width = timeScaleBounds.getWidth();

  // Calculate visible range based on zoom level
  float visibleWidth = 1.0f / m_zoomFactor;
  float startFraction = m_zoomCenter - (visibleWidth / 2.0f);
  float endFraction = m_zoomCenter + (visibleWidth / 2.0f);
  startFraction = std::clamp(startFraction, 0.0f, 1.0f);
  endFraction = std::clamp(endFraction, 0.0f, 1.0f);

  // Calculate visible time range
  double startTime = startFraction *
                     (m_waveformData.totalSamples / static_cast<double>(m_waveformData.sampleRate));
  double endTime =
      endFraction * (m_waveformData.totalSamples / static_cast<double>(m_waveformData.sampleRate));
  double visibleDuration = endTime - startTime;
  double totalDuration =
      m_waveformData.totalSamples / static_cast<double>(m_waveformData.sampleRate);

  // ADAPTIVE TIME INTERVAL LOGIC
  // Goal: Show ~16 time markers at 1x zoom, scaled proportionally with zoom level
  // For 3-minute track at 1x zoom: 180s / 16 = ~11s intervals (round to 10s)
  // At 16x zoom: visible duration is 180s/16 = 11.25s, so 11.25s / 16 = ~0.7s (round to 1s or 0.5s)

  double targetMarkerCount = 16.0; // Target number of markers visible
  double roughInterval = visibleDuration / targetMarkerCount;

  // Snap interval to nice round numbers for readability
  double timeInterval;
  if (roughInterval >= 60.0) {
    // >= 1 minute per marker: round to nearest minute
    timeInterval = std::ceil(roughInterval / 60.0) * 60.0;
  } else if (roughInterval >= 30.0) {
    timeInterval = 30.0; // 30 second intervals
  } else if (roughInterval >= 10.0) {
    timeInterval = 10.0; // 10 second intervals
  } else if (roughInterval >= 5.0) {
    timeInterval = 5.0; // 5 second intervals
  } else if (roughInterval >= 2.0) {
    timeInterval = 2.0; // 2 second intervals
  } else if (roughInterval >= 1.0) {
    timeInterval = 1.0; // 1 second intervals
  } else if (roughInterval >= 0.5) {
    timeInterval = 0.5; // 500ms intervals
  } else if (roughInterval >= 0.25) {
    timeInterval = 0.25; // 250ms intervals
  } else if (roughInterval >= 0.1) {
    timeInterval = 0.1; // 100ms intervals
  } else {
    timeInterval = 0.05; // 50ms intervals (very high zoom)
  }

  // Draw time markers with collision prevention
  g.setColour(juce::Colours::white.withAlpha(0.7f));
  g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));

  double firstMarker = std::ceil(startTime / timeInterval) * timeInterval;

  // Minimum spacing between labels to prevent collision (in pixels)
  const float minLabelSpacing = 80.0f; // Width of typical label + padding
  float lastLabelX = -minLabelSpacing; // Initialize to allow first label

  for (double time = firstMarker; time < endTime; time += timeInterval) {
    float normalizedTime = (time - startTime) / visibleDuration;
    float x = timeScaleBounds.getX() + normalizedTime * width;

    // Skip this label if it would collide with the previous one
    if (x - lastLabelX < minLabelSpacing) {
      continue;
    }

    // Draw tick mark
    g.drawLine(x, bounds.getY(), x, bounds.getY() + 8.0f, 1.0f);

    // Format time label (adaptive based on interval size)
    int minutes = static_cast<int>(time / 60.0);
    double seconds = std::fmod(time, 60.0);
    juce::String timeLabel;

    if (timeInterval < 1.0) {
      // Show milliseconds for sub-second intervals
      int wholeSecs = static_cast<int>(seconds);
      int millis = static_cast<int>((seconds - wholeSecs) * 1000.0);
      timeLabel = juce::String(minutes) + ":" + juce::String(wholeSecs).paddedLeft('0', 2) + "." +
                  juce::String(millis).paddedLeft('0', 3);
    } else if (timeInterval < 10.0) {
      // Show one decimal place for 1-9 second intervals
      timeLabel = juce::String(minutes) + ":" + juce::String(seconds, 1).paddedLeft('0', 4);
    } else {
      // Show whole seconds for 10+ second intervals
      timeLabel =
          juce::String(minutes) + ":" + juce::String(static_cast<int>(seconds)).paddedLeft('0', 2);
    }

    // Draw label (centered on tick)
    int labelWidth = 70;
    g.drawText(timeLabel, static_cast<int>(x - labelWidth / 2),
               static_cast<int>(bounds.getY()) + 10, labelWidth, 18, juce::Justification::centred,
               false);

    // Update last label position
    lastLabelX = x;
  }

  DBG("WaveformDisplay: Time scale - totalDuration=" << totalDuration
                                                     << "s, visibleDuration=" << visibleDuration
                                                     << "s, interval=" << timeInterval << "s");
}
