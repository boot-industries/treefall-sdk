/*
  ==============================================================================

    WaveformEditor.cpp
    Created: shmui Component Library

    Waveform editor implementation.

  ==============================================================================
*/

#include "WaveformEditor.h"

namespace shmui {

//==============================================================================
WaveformEditor::WaveformEditor() {
  setMouseCursor(juce::MouseCursor::NormalCursor);
}

WaveformEditor::~WaveformEditor() {}

//==============================================================================
void WaveformEditor::setAudioFile(const juce::File& audioFile) {
  if (audioFile.getFullPathName() == m_cachedFilePath)
    return; // Already loaded

  // Check cache first
  auto cached = m_waveformCache.find(audioFile.getFullPathName());
  if (cached != m_waveformCache.end()) {
    juce::ScopedLock sl(m_dataLock);
    m_waveformData = cached->second;
    m_cachedFilePath = audioFile.getFullPathName();

    // Reset trim points to full file
    m_trimInSamples = 0;
    m_trimOutSamples = m_waveformData.totalSamples;
    repaint();
    return;
  }

  // Generate new waveform data
  generateWaveformData(audioFile);
}

void WaveformEditor::setWaveformData(const WaveformData& data) {
  juce::ScopedLock sl(m_dataLock);
  m_waveformData = data;
  m_trimInSamples = 0;
  m_trimOutSamples = data.totalSamples;
  repaint();
}

void WaveformEditor::clear() {
  juce::ScopedLock sl(m_dataLock);
  m_waveformData = WaveformData();
  m_cachedFilePath = "";
  m_trimInSamples = 0;
  m_trimOutSamples = 0;
  m_playheadPosition = 0;
  m_selectionStart = 0;
  m_selectionEnd = 0;
  repaint();
}

//==============================================================================
void WaveformEditor::setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples) {
  const int64_t total = m_waveformData.totalSamples;
  m_trimInSamples = juce::jlimit(int64_t(0), total, trimInSamples);
  m_trimOutSamples = juce::jlimit(m_trimInSamples, total, trimOutSamples);
  repaint();
}

void WaveformEditor::setTrimPointsNormalized(float trimIn, float trimOut) {
  const int64_t total = m_waveformData.totalSamples;
  setTrimPoints(static_cast<int64_t>(trimIn * total), static_cast<int64_t>(trimOut * total));
}

void WaveformEditor::setFadeInSamples(int64_t samples) {
  m_fadeInSamples = juce::jmax(int64_t(0), samples);
  repaint();
}

void WaveformEditor::setFadeOutSamples(int64_t samples) {
  m_fadeOutSamples = juce::jmax(int64_t(0), samples);
  repaint();
}

//==============================================================================
void WaveformEditor::setPlayheadPosition(int64_t samplePosition) {
  const int64_t clamped = juce::jlimit(int64_t(0), m_waveformData.totalSamples, samplePosition);
  if (m_playheadPosition != clamped) {
    m_playheadPosition = clamped;
    repaint();
  }
}

void WaveformEditor::setPlayheadNormalized(float position) {
  setPlayheadPosition(static_cast<int64_t>(position * m_waveformData.totalSamples));
}

//==============================================================================
void WaveformEditor::setSelection(int64_t startSamples, int64_t endSamples) {
  const int64_t total = m_waveformData.totalSamples;
  m_selectionStart = juce::jlimit(int64_t(0), total, startSamples);
  m_selectionEnd = juce::jlimit(m_selectionStart, total, endSamples);
  repaint();
}

void WaveformEditor::clearSelection() {
  m_selectionStart = 0;
  m_selectionEnd = 0;
  repaint();
}

//==============================================================================
void WaveformEditor::setZoomLevel(float zoom) {
  m_zoomLevel = juce::jmax(1.0f, zoom);
  repaint();
}

void WaveformEditor::setScrollPosition(float position) {
  m_scrollPosition = juce::jlimit(0.0f, 1.0f, position);
  repaint();
}

void WaveformEditor::zoomToFit() {
  m_zoomLevel = 1.0f;
  m_scrollPosition = 0.0f;
  repaint();
}

void WaveformEditor::zoomToSelection() {
  if (!hasSelection())
    return;

  const float selectionRatio = static_cast<float>(m_selectionEnd - m_selectionStart) /
                               static_cast<float>(m_waveformData.totalSamples);

  m_zoomLevel = 1.0f / selectionRatio;
  m_scrollPosition =
      static_cast<float>(m_selectionStart) / static_cast<float>(m_waveformData.totalSamples);
  repaint();
}

//==============================================================================
void WaveformEditor::setStyle(const WaveformEditorStyle& style) {
  m_style = style;
  repaint();
}

//==============================================================================
void WaveformEditor::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.fillAll(m_style.backgroundColor);

  if (!m_waveformData.isValid) {
    // No data - show placeholder
    g.setColour(m_style.timeTextColor);
    g.drawText("No waveform loaded", bounds, juce::Justification::centred, false);
    return;
  }

  juce::ScopedLock sl(m_dataLock);

  // Calculate visible region based on zoom/scroll
  auto waveformBounds = bounds;
  if (m_style.showTimeScale)
    waveformBounds.removeFromBottom(20.0f);

  // Draw components in order (back to front)
  if (m_style.showGrid)
    drawGrid(g, waveformBounds);

  drawSelection(g, waveformBounds);
  drawWaveform(g, waveformBounds);
  drawFadeCurves(g, waveformBounds);
  drawTrimMarkers(g, waveformBounds);
  drawPlayhead(g, waveformBounds);

  if (m_style.showTimeScale)
    drawTimeScale(g, bounds.removeFromBottom(20.0f));
}

void WaveformEditor::resized() {
  // Regenerate waveform data at new resolution if needed
}

//==============================================================================
void WaveformEditor::mouseDown(const juce::MouseEvent& e) {
  if (!m_waveformData.isValid)
    return;

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);

  float x = static_cast<float>(e.getPosition().x);
  float y = static_cast<float>(e.getPosition().y);

  m_draggedHandle = getHandleAt(x, y);
  m_dragStartPoint = {x, y};

  if (m_draggedHandle == DragHandle::TrimIn) {
    m_dragStartValue = m_trimInSamples;
  } else if (m_draggedHandle == DragHandle::TrimOut) {
    m_dragStartValue = m_trimOutSamples;
  } else if (e.mods.isShiftDown()) {
    // Start selection
    m_isSelecting = true;
    m_selectionStart = xToSample(x, bounds.getWidth());
    m_selectionEnd = m_selectionStart;
  } else if (m_draggedHandle == DragHandle::None) {
    // Seek on click
    int64_t sample = xToSample(x, bounds.getWidth());
    if (onSeek)
      onSeek(sample);
  }
}

void WaveformEditor::mouseDrag(const juce::MouseEvent& e) {
  if (!m_waveformData.isValid)
    return;

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);

  float x = static_cast<float>(e.getPosition().x);

  if (m_isSelecting) {
    m_selectionEnd = xToSample(x, bounds.getWidth());
    repaint();

    if (onSelectionChanged) {
      int64_t start = juce::jmin(m_selectionStart, m_selectionEnd);
      int64_t end = juce::jmax(m_selectionStart, m_selectionEnd);
      onSelectionChanged(start, end);
    }
  } else if (m_draggedHandle == DragHandle::TrimIn) {
    int64_t newTrimIn = xToSample(x, bounds.getWidth());
    newTrimIn = juce::jlimit(int64_t(0), m_trimOutSamples - 1, newTrimIn);
    if (newTrimIn != m_trimInSamples) {
      m_trimInSamples = newTrimIn;
      repaint();

      if (onTrimPointsChanged)
        onTrimPointsChanged(m_trimInSamples, m_trimOutSamples);
    }
  } else if (m_draggedHandle == DragHandle::TrimOut) {
    int64_t newTrimOut = xToSample(x, bounds.getWidth());
    newTrimOut = juce::jlimit(m_trimInSamples + 1, m_waveformData.totalSamples, newTrimOut);
    if (newTrimOut != m_trimOutSamples) {
      m_trimOutSamples = newTrimOut;
      repaint();

      if (onTrimPointsChanged)
        onTrimPointsChanged(m_trimInSamples, m_trimOutSamples);
    }
  }
}

void WaveformEditor::mouseUp(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);

  if (m_isSelecting) {
    // Normalize selection
    if (m_selectionStart > m_selectionEnd)
      std::swap(m_selectionStart, m_selectionEnd);
  }

  m_draggedHandle = DragHandle::None;
  m_isSelecting = false;
  updateCursor(DragHandle::None);
}

void WaveformEditor::mouseMove(const juce::MouseEvent& e) {
  if (!m_waveformData.isValid)
    return;

  float x = static_cast<float>(e.getPosition().x);
  float y = static_cast<float>(e.getPosition().y);

  DragHandle handle = getHandleAt(x, y);
  updateCursor(handle);
}

void WaveformEditor::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel) {
  if (!m_waveformData.isValid)
    return;

  if (e.mods.isCommandDown()) {
    // Zoom
    float zoomDelta = wheel.deltaY * 0.5f;
    setZoomLevel(m_zoomLevel * (1.0f + zoomDelta));
  } else {
    // Scroll
    float scrollDelta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
    setScrollPosition(m_scrollPosition - scrollDelta * 0.1f);
  }
}

//==============================================================================
void WaveformEditor::generateWaveformData(const juce::File& audioFile) {
  if (m_isLoading.exchange(true))
    return; // Already loading

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));

  if (reader == nullptr) {
    m_isLoading = false;
    return;
  }

  WaveformData newData;
  newData.sampleRate = static_cast<int>(reader->sampleRate);
  newData.numChannels = static_cast<int>(reader->numChannels);
  newData.totalSamples = reader->lengthInSamples;

  // Calculate samples per pixel (assuming max width of 2048 for cache)
  const int targetWidth = 2048;
  const int64_t samplesPerPixel = juce::jmax(int64_t(1), newData.totalSamples / targetWidth);

  newData.minValues.resize(targetWidth);
  newData.maxValues.resize(targetWidth);

  // Read and downsample
  juce::AudioBuffer<float> buffer(newData.numChannels, static_cast<int>(samplesPerPixel));

  for (int i = 0; i < targetWidth; ++i) {
    int64_t startSample = i * samplesPerPixel;
    int64_t numToRead = juce::jmin(samplesPerPixel, newData.totalSamples - startSample);

    if (numToRead <= 0) {
      newData.minValues[i] = 0.0f;
      newData.maxValues[i] = 0.0f;
      continue;
    }

    buffer.clear();
    reader->read(&buffer, 0, static_cast<int>(numToRead), startSample, true, true);

    float minVal = 0.0f;
    float maxVal = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
      auto range = buffer.findMinMax(ch, 0, static_cast<int>(numToRead));
      minVal = juce::jmin(minVal, range.getStart());
      maxVal = juce::jmax(maxVal, range.getEnd());
    }

    newData.minValues[i] = minVal;
    newData.maxValues[i] = maxVal;
  }

  newData.isValid = true;

  // Update data
  {
    juce::ScopedLock sl(m_dataLock);
    m_waveformData = newData;
    m_cachedFilePath = audioFile.getFullPathName();

    // Reset trim to full file
    m_trimInSamples = 0;
    m_trimOutSamples = newData.totalSamples;

    // Add to cache (evict oldest if full)
    if (m_waveformCache.size() >= MAX_CACHE_SIZE)
      m_waveformCache.erase(m_waveformCache.begin());

    m_waveformCache[audioFile.getFullPathName()] = newData;
  }

  m_isLoading = false;

  juce::MessageManager::callAsync([this] { repaint(); });
}

void WaveformEditor::drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds) {
  if (m_waveformData.minValues.empty())
    return;

  const float width = bounds.getWidth();
  const float height = bounds.getHeight();
  const float centerY = bounds.getCentreY();

  // Calculate visible range based on zoom/scroll
  const float visibleRatio = 1.0f / m_zoomLevel;
  const float startRatio = m_scrollPosition;
  const float endRatio = startRatio + visibleRatio;

  const int dataSize = static_cast<int>(m_waveformData.minValues.size());
  const int startIdx = static_cast<int>(startRatio * dataSize);
  const int endIdx = juce::jmin(static_cast<int>(endRatio * dataSize), dataSize - 1);

  if (startIdx >= endIdx)
    return;

  // Draw waveform path
  juce::Path waveformPath;

  const float pixelsPerSample = width / static_cast<float>(endIdx - startIdx);

  // Upper half (max values)
  waveformPath.startNewSubPath(bounds.getX(), centerY);
  for (int i = startIdx; i <= endIdx; ++i) {
    float x = bounds.getX() + (i - startIdx) * pixelsPerSample;
    float y = centerY - (m_waveformData.maxValues[i] * height * 0.5f);
    waveformPath.lineTo(x, y);
  }

  // Lower half (min values) - go backwards
  for (int i = endIdx; i >= startIdx; --i) {
    float x = bounds.getX() + (i - startIdx) * pixelsPerSample;
    float y = centerY - (m_waveformData.minValues[i] * height * 0.5f);
    waveformPath.lineTo(x, y);
  }

  waveformPath.closeSubPath();

  // Fill
  g.setColour(m_style.waveformFillColor);
  g.fillPath(waveformPath);

  // Stroke
  g.setColour(m_style.waveformColor);
  g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
}

void WaveformEditor::drawTrimMarkers(juce::Graphics& g, juce::Rectangle<float> bounds) {
  const float width = bounds.getWidth();

  // Trim in handle
  float trimInX = sampleToX(m_trimInSamples, width);
  g.setColour(m_style.trimHandleColor);
  g.fillRect(juce::Rectangle<float>(bounds.getX() + trimInX - 2.0f, bounds.getY(),
                                    m_style.trimHandleWidth, bounds.getHeight()));

  // Trim out handle
  float trimOutX = sampleToX(m_trimOutSamples, width);
  g.fillRect(juce::Rectangle<float>(bounds.getX() + trimOutX - m_style.trimHandleWidth + 2.0f,
                                    bounds.getY(), m_style.trimHandleWidth, bounds.getHeight()));

  // Grayed out regions outside trim
  g.setColour(m_style.trimRegionColor);
  g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getY(), trimInX, bounds.getHeight()));
  g.fillRect(juce::Rectangle<float>(bounds.getX() + trimOutX, bounds.getY(), width - trimOutX,
                                    bounds.getHeight()));
}

void WaveformEditor::drawFadeCurves(juce::Graphics& g, juce::Rectangle<float> bounds) {
  if (m_fadeInSamples <= 0 && m_fadeOutSamples <= 0)
    return;

  const float width = bounds.getWidth();

  g.setColour(m_style.fadeColor);

  // Fade in curve
  if (m_fadeInSamples > 0) {
    float fadeInEndX = sampleToX(m_trimInSamples + m_fadeInSamples, width);
    float fadeInStartX = sampleToX(m_trimInSamples, width);

    juce::Path fadeInPath;
    fadeInPath.startNewSubPath(bounds.getX() + fadeInStartX, bounds.getBottom());
    fadeInPath.lineTo(bounds.getX() + fadeInStartX, bounds.getY());
    fadeInPath.quadraticTo(bounds.getX() + (fadeInStartX + fadeInEndX) * 0.5f, bounds.getY(),
                           bounds.getX() + fadeInEndX, bounds.getBottom());
    fadeInPath.closeSubPath();
    g.fillPath(fadeInPath);
  }

  // Fade out curve
  if (m_fadeOutSamples > 0) {
    float fadeOutStartX = sampleToX(m_trimOutSamples - m_fadeOutSamples, width);
    float fadeOutEndX = sampleToX(m_trimOutSamples, width);

    juce::Path fadeOutPath;
    fadeOutPath.startNewSubPath(bounds.getX() + fadeOutStartX, bounds.getBottom());
    fadeOutPath.quadraticTo(bounds.getX() + (fadeOutStartX + fadeOutEndX) * 0.5f, bounds.getY(),
                            bounds.getX() + fadeOutEndX, bounds.getY());
    fadeOutPath.lineTo(bounds.getX() + fadeOutEndX, bounds.getBottom());
    fadeOutPath.closeSubPath();
    g.fillPath(fadeOutPath);
  }
}

void WaveformEditor::drawPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds) {
  const float width = bounds.getWidth();
  float playheadX = sampleToX(m_playheadPosition, width);

  g.setColour(m_style.playheadColor);
  g.fillRect(juce::Rectangle<float>(bounds.getX() + playheadX - m_style.playheadWidth * 0.5f,
                                    bounds.getY(), m_style.playheadWidth, bounds.getHeight()));
}

void WaveformEditor::drawSelection(juce::Graphics& g, juce::Rectangle<float> bounds) {
  if (!hasSelection())
    return;

  const float width = bounds.getWidth();
  float startX = sampleToX(m_selectionStart, width);
  float endX = sampleToX(m_selectionEnd, width);

  g.setColour(m_style.selectionColor);
  g.fillRect(juce::Rectangle<float>(bounds.getX() + startX, bounds.getY(), endX - startX,
                                    bounds.getHeight()));
}

void WaveformEditor::drawTimeScale(juce::Graphics& g, juce::Rectangle<float> bounds) {
  g.setColour(m_style.timeTextColor);
  g.setFont(10.0f);

  // Draw time markers
  const int numMarkers = 10;
  const float width = bounds.getWidth();

  for (int i = 0; i <= numMarkers; ++i) {
    float ratio = static_cast<float>(i) / numMarkers;
    float x = bounds.getX() + ratio * width;
    int64_t sample = static_cast<int64_t>(ratio * m_waveformData.totalSamples);

    juce::String timeText = formatTime(sample);
    g.drawText(timeText, juce::Rectangle<float>(x - 30, bounds.getY(), 60, bounds.getHeight()),
               juce::Justification::centred, false);
  }
}

void WaveformEditor::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds) {
  g.setColour(m_style.gridColor);

  // Vertical grid lines
  const int numLines = 10;
  for (int i = 1; i < numLines; ++i) {
    float x = bounds.getX() + (bounds.getWidth() * i / numLines);
    g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
  }

  // Horizontal center line
  g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());
}

//==============================================================================
float WaveformEditor::sampleToX(int64_t sample, float width) const {
  if (m_waveformData.totalSamples == 0)
    return 0.0f;

  float normalizedPos =
      static_cast<float>(sample) / static_cast<float>(m_waveformData.totalSamples);

  // Apply zoom and scroll
  float visibleRatio = 1.0f / m_zoomLevel;
  float startRatio = m_scrollPosition;

  return (normalizedPos - startRatio) / visibleRatio * width;
}

int64_t WaveformEditor::xToSample(float x, float width) const {
  if (width <= 0 || m_waveformData.totalSamples == 0)
    return 0;

  // Apply zoom and scroll
  float visibleRatio = 1.0f / m_zoomLevel;
  float startRatio = m_scrollPosition;

  float normalizedPos = (x / width) * visibleRatio + startRatio;
  normalizedPos = juce::jlimit(0.0f, 1.0f, normalizedPos);

  return static_cast<int64_t>(normalizedPos * m_waveformData.totalSamples);
}

bool WaveformEditor::isNearHandle(float mouseX, float handleX, float tolerance) const {
  return std::abs(mouseX - handleX) <= tolerance;
}

WaveformEditor::DragHandle WaveformEditor::getHandleAt(float x, float y) const {
  juce::ignoreUnused(y);

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);

  const float width = bounds.getWidth();

  float trimInX = sampleToX(m_trimInSamples, width);
  if (isNearHandle(x, trimInX, m_style.trimHandleWidth))
    return DragHandle::TrimIn;

  float trimOutX = sampleToX(m_trimOutSamples, width);
  if (isNearHandle(x, trimOutX, m_style.trimHandleWidth))
    return DragHandle::TrimOut;

  return DragHandle::None;
}

void WaveformEditor::updateCursor(DragHandle handle) {
  switch (handle) {
  case DragHandle::TrimIn:
  case DragHandle::TrimOut:
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    break;
  default:
    setMouseCursor(juce::MouseCursor::NormalCursor);
    break;
  }
}

juce::String WaveformEditor::formatTime(int64_t samples) const {
  if (m_waveformData.sampleRate == 0)
    return "0:00";

  double seconds = static_cast<double>(samples) / m_waveformData.sampleRate;

  int mins = static_cast<int>(seconds / 60.0);
  int secs = static_cast<int>(std::fmod(seconds, 60.0));
  int ms = static_cast<int>(std::fmod(seconds * 1000.0, 1000.0));

  if (mins > 0)
    return juce::String::formatted("%d:%02d.%03d", mins, secs, ms);
  else
    return juce::String::formatted("%d.%03d", secs, ms);
}

} // namespace shmui
