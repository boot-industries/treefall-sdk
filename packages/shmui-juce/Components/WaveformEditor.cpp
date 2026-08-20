/*
  ==============================================================================

    WaveformEditor.cpp
    Created: shmui Component Library

    Waveform editor implementation.

  ==============================================================================
*/

#include "WaveformEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace shmui {

namespace {
constexpr int kWaveformColumns = 2048;
constexpr int kWaveformReadChunk = 65536;
constexpr int kWaveformMaxChannels = 32;
} // namespace

class WaveformEditor::WaveformLoadJob : public juce::ThreadPoolJob {
public:
  WaveformLoadJob(const juce::String& path, juce::Component::SafePointer<WaveformEditor> owner,
                  std::atomic<uint64_t>& generation, uint64_t requestedGeneration)
      : juce::ThreadPoolJob("shmui waveform load"), m_path(path), m_owner(std::move(owner)),
        m_generation(generation), m_requestedGeneration(requestedGeneration) {}

  JobStatus runJob() override {
    WaveformData data;
    const bool success = WaveformEditor::loadWaveformData(juce::File(m_path), m_generation,
                                                          m_requestedGeneration, data);
    if (shouldExit() || m_generation.load(std::memory_order_acquire) != m_requestedGeneration)
      return jobHasFinished;

    auto owner = m_owner;
    const auto path = m_path;
    juce::MessageManager::callAsync([owner, generation = m_requestedGeneration, path,
                                     data = std::move(data), success]() mutable {
      if (owner != nullptr)
        owner->applyLoadedWaveform(generation, path, success ? std::move(data) : WaveformData{});
    });
    return jobHasFinished;
  }

private:
  juce::String m_path;
  juce::Component::SafePointer<WaveformEditor> m_owner;
  std::atomic<uint64_t>& m_generation;
  uint64_t m_requestedGeneration;
};

//==============================================================================
WaveformEditor::WaveformEditor() {
  setMouseCursor(juce::MouseCursor::NormalCursor);
  m_style = WaveformEditorStyle::fromTheme(defaultTheme());
  sanitizeStyle();
  addDefaultThemeListener(this);
}

WaveformEditor::~WaveformEditor() {
  removeDefaultThemeListener(this);
  m_playheadRepaint.cancel();
  m_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
  while (!m_loadPool.removeAllJobs(true, 5000)) {
  }
}

//==============================================================================
void WaveformEditor::setAudioFile(const juce::File& audioFile) {
  if (!requireMessageThread())
    return;

  const auto path = audioFile.getFullPathName();
  if (path == m_cachedFilePath)
    return; // Already loaded

  m_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
  m_loadPool.removeAllJobs(false, 0);
  m_isLoading.store(false, std::memory_order_release);

  const auto cacheIt =
      std::find_if(m_waveformCache.begin(), m_waveformCache.end(),
                   [&path](const WaveformCacheEntry& entry) { return entry.path == path; });
  if (cacheIt != m_waveformCache.end()) {
    const WaveformData cachedData = cacheIt->data;
    m_waveformCache.splice(m_waveformCache.end(), m_waveformCache, cacheIt);
    juce::ScopedLock sl(m_dataLock);
    m_waveformData = cachedData;
    m_cachedFilePath = path;
    m_trimInSamples = 0;
    m_trimOutSamples = m_waveformData.totalSamples;
    repaint();
    return;
  }

  {
    juce::ScopedLock sl(m_dataLock);
    m_waveformData = WaveformData{};
    m_cachedFilePath.clear();
    m_trimInSamples = 0;
    m_trimOutSamples = 0;
  }
  repaint();

  generateWaveformData(audioFile);
}

void WaveformEditor::setWaveformData(const WaveformData& data) {
  if (!requireMessageThread())
    return;

  m_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
  m_loadPool.removeAllJobs(false, 0);
  m_isLoading.store(false, std::memory_order_release);

  WaveformData safeData = data;
  const auto boundedSize =
      std::min(safeData.minValues.size(), static_cast<size_t>(kWaveformColumns));
  safeData.minValues.resize(boundedSize);
  safeData.maxValues.resize(boundedSize);
  safeData.sampleRate = juce::jmax(1, safeData.sampleRate);
  safeData.numChannels = juce::jlimit(1, kWaveformMaxChannels, safeData.numChannels);
  safeData.totalSamples = juce::jmax(int64_t(0), safeData.totalSamples);
  for (size_t i = 0; i < boundedSize; ++i) {
    safeData.minValues[i] = std::isfinite(safeData.minValues[i])
                                ? juce::jlimit(-1.0f, 1.0f, safeData.minValues[i])
                                : 0.0f;
    safeData.maxValues[i] = std::isfinite(safeData.maxValues[i])
                                ? juce::jlimit(-1.0f, 1.0f, safeData.maxValues[i])
                                : 0.0f;
  }
  safeData.isValid = safeData.isValid && !safeData.minValues.empty() &&
                     safeData.minValues.size() == safeData.maxValues.size() &&
                     safeData.totalSamples > 0;

  juce::ScopedLock sl(m_dataLock);
  m_waveformData = std::move(safeData);
  m_cachedFilePath.clear();
  m_trimInSamples = 0;
  m_trimOutSamples = m_waveformData.totalSamples;
  repaint();
}

void WaveformEditor::clear() {
  if (!requireMessageThread())
    return;

  m_loadGeneration.fetch_add(1, std::memory_order_acq_rel);
  m_loadPool.removeAllJobs(false, 0);
  m_isLoading.store(false, std::memory_order_release);

  juce::ScopedLock sl(m_dataLock);
  m_waveformData = WaveformData();
  m_cachedFilePath.clear();
  m_trimInSamples = 0;
  m_trimOutSamples = 0;
  m_playheadPosition = 0;
  m_selectionStart = 0;
  m_selectionEnd = 0;
  repaint();
}

//==============================================================================
void WaveformEditor::setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples) {
  if (!requireMessageThread())
    return;

  const int64_t total = m_waveformData.totalSamples;
  m_trimInSamples = juce::jlimit(int64_t(0), total, trimInSamples);
  m_trimOutSamples = juce::jlimit(m_trimInSamples, total, trimOutSamples);
  repaint();
}

void WaveformEditor::setTrimPointsNormalized(float trimIn, float trimOut) {
  if (!requireMessageThread())
    return;

  const int64_t total = m_waveformData.totalSamples;
  const float safeIn = std::isfinite(trimIn) ? juce::jlimit(0.0f, 1.0f, trimIn) : 0.0f;
  const float safeOut = std::isfinite(trimOut) ? juce::jlimit(0.0f, 1.0f, trimOut) : 0.0f;
  setTrimPoints(static_cast<int64_t>(safeIn * total), static_cast<int64_t>(safeOut * total));
}

void WaveformEditor::setFadeInSamples(int64_t samples) {
  if (!requireMessageThread())
    return;
  m_fadeInSamples = juce::jmax(int64_t(0), samples);
  repaint();
}

void WaveformEditor::setFadeOutSamples(int64_t samples) {
  if (!requireMessageThread())
    return;
  m_fadeOutSamples = juce::jmax(int64_t(0), samples);
  repaint();
}

//==============================================================================
void WaveformEditor::setPlayheadPosition(int64_t samplePosition) {
  if (!requireMessageThread())
    return;

  const int64_t clamped = juce::jlimit(int64_t(0), m_waveformData.totalSamples, samplePosition);
  if (m_playheadPosition != clamped) {
    m_playheadPosition = clamped;
    if (m_followMode != FollowMode::Off)
      followPlayhead();
    m_playheadRepaint.requestRepaint();
  }
}

void WaveformEditor::setPlayheadNormalized(float position) {
  if (!requireMessageThread())
    return;

  const float safePosition = std::isfinite(position) ? juce::jlimit(0.0f, 1.0f, position) : 0.0f;
  setPlayheadPosition(static_cast<int64_t>(safePosition * m_waveformData.totalSamples));
}

//==============================================================================
void WaveformEditor::setSelection(int64_t startSamples, int64_t endSamples) {
  if (!requireMessageThread())
    return;

  const int64_t total = m_waveformData.totalSamples;
  m_selectionStart = juce::jlimit(int64_t(0), total, startSamples);
  m_selectionEnd = juce::jlimit(m_selectionStart, total, endSamples);
  repaint();
}

void WaveformEditor::clearSelection() {
  if (!requireMessageThread())
    return;
  m_selectionStart = 0;
  m_selectionEnd = 0;
  repaint();
}

//==============================================================================
void WaveformEditor::setZoomLevel(float zoom) {
  if (!requireMessageThread())
    return;
  m_zoomLevel = std::isfinite(zoom) ? juce::jlimit(1.0f, 256.0f, zoom) : 1.0f;
  repaint();
}

void WaveformEditor::setScrollPosition(float position) {
  if (!requireMessageThread())
    return;
  m_scrollPosition = std::isfinite(position) ? juce::jlimit(0.0f, 1.0f, position) : 0.0f;
  repaint();
}

void WaveformEditor::zoomToFit() {
  if (!requireMessageThread())
    return;
  m_zoomLevel = 1.0f;
  m_scrollPosition = 0.0f;
  repaint();
}

void WaveformEditor::zoomToSelection() {
  if (!requireMessageThread() || !hasSelection() || m_waveformData.totalSamples <= 0)
    return;

  const float selectionRatio = static_cast<float>(m_selectionEnd - m_selectionStart) /
                               static_cast<float>(m_waveformData.totalSamples);
  if (selectionRatio <= 0.0f)
    return;

  m_zoomLevel = juce::jlimit(1.0f, 256.0f, 1.0f / selectionRatio);
  m_scrollPosition = juce::jlimit(0.0f, 1.0f,
                                  static_cast<float>(m_selectionStart) /
                                      static_cast<float>(m_waveformData.totalSamples));
  repaint();
}

//==============================================================================
void WaveformEditor::setStyle(const WaveformEditorStyle& style) {
  if (!requireMessageThread())
    return;
  m_style = style;
  sanitizeStyle();
  m_usesDefaultThemeStyle = false;
  repaint();
}

void WaveformEditor::useDefaultThemeStyle() {
  if (!requireMessageThread())
    return;
  m_usesDefaultThemeStyle = true;
  m_style = WaveformEditorStyle::fromTheme(defaultTheme());
  sanitizeStyle();
  repaint();
}

void WaveformEditor::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !m_usesDefaultThemeStyle)
    return;

  m_style = WaveformEditorStyle::fromTheme(theme);
  sanitizeStyle();
  repaint();
}
void WaveformEditor::sanitizeStyle() {
  const auto finiteOr = [](float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
  };
  m_style.playheadWidth = juce::jlimit(0.5f, 64.0f, finiteOr(m_style.playheadWidth, 2.0f));
  m_style.trimHandleWidth = juce::jlimit(1.0f, 128.0f, finiteOr(m_style.trimHandleWidth, 8.0f));
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
  drawAuditionRegion(g, waveformBounds);
  drawWaveform(g, waveformBounds);
  drawFadeCurves(g, waveformBounds);
  drawTrimMarkers(g, waveformBounds);
  drawCueMarkers(g, waveformBounds);
  drawPlayhead(g, waveformBounds);

  if (m_style.showTimeScale)
    drawTimeScale(g, bounds.removeFromBottom(20.0f));
}

void WaveformEditor::resized() {
  // Regenerate waveform data at new resolution if needed
}

//==============================================================================
void WaveformEditor::mouseDown(const juce::MouseEvent& e) {
  if (!requireMessageThread() || !m_waveformData.isValid)
    return;

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);

  const float x = static_cast<float>(e.getPosition().x);
  const float y = static_cast<float>(e.getPosition().y);
  m_draggedHandle = getHandleAt(x, y);
  m_dragStartPoint = {x, y};
  m_draggedCueIndex = -1;

  const int cueIdx = cueMarkerIndexAt(x, bounds.getWidth());
  if (m_draggedHandle == DragHandle::None && cueIdx >= 0) {
    m_draggedHandle = DragHandle::CueMarker;
    m_draggedCueIndex = cueIdx;
  }

  if (m_draggedHandle == DragHandle::TrimIn) {
    m_dragStartValue = m_trimInSamples;
  } else if (m_draggedHandle == DragHandle::TrimOut) {
    m_dragStartValue = m_trimOutSamples;
  } else if (m_draggedHandle == DragHandle::CueMarker) {
    m_dragStartValue = m_cueMarkers[static_cast<size_t>(m_draggedCueIndex)].sample;
  } else if (e.mods.isShiftDown()) {
    m_isSelecting = true;
    m_selectionStart = xToSample(x, bounds.getWidth());
    m_selectionEnd = m_selectionStart;
  } else if (m_draggedHandle == DragHandle::None && onSeek) {
    const int64_t sample = xToSample(x, bounds.getWidth());
    juce::Component::SafePointer<WaveformEditor> safeThis(this);
    onSeek(sample);
    if (safeThis == nullptr)
      return;
  }
}

void WaveformEditor::mouseDoubleClick(const juce::MouseEvent& e) {
  if (!requireMessageThread() || !m_waveformData.isValid || !hasAuditionRegion())
    return;

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);

  const int64_t sample = xToSample(static_cast<float>(e.getPosition().x), bounds.getWidth());
  if (sample >= m_auditionStart && sample <= m_auditionEnd && onAuditionRequested) {
    juce::Component::SafePointer<WaveformEditor> safeThis(this);
    onAuditionRequested();
    juce::ignoreUnused(safeThis);
  }
}

void WaveformEditor::mouseDrag(const juce::MouseEvent& e) {
  if (!requireMessageThread() || !m_waveformData.isValid)
    return;

  auto bounds = getLocalBounds().toFloat();
  if (m_style.showTimeScale)
    bounds.removeFromBottom(20.0f);
  const float x = static_cast<float>(e.getPosition().x);

  if (m_isSelecting) {
    m_selectionEnd = xToSample(x, bounds.getWidth());
    repaint();
    if (onSelectionChanged) {
      const int64_t start = juce::jmin(m_selectionStart, m_selectionEnd);
      const int64_t end = juce::jmax(m_selectionStart, m_selectionEnd);
      juce::Component::SafePointer<WaveformEditor> safeThis(this);
      onSelectionChanged(start, end);
      if (safeThis == nullptr)
        return;
    }
  } else if (m_draggedHandle == DragHandle::TrimIn) {
    int64_t newTrimIn = xToSample(x, bounds.getWidth());
    newTrimIn = juce::jlimit(int64_t(0), m_trimOutSamples - 1, newTrimIn);
    if (newTrimIn != m_trimInSamples) {
      m_trimInSamples = newTrimIn;
      repaint();
      if (onTrimPointsChanged) {
        juce::Component::SafePointer<WaveformEditor> safeThis(this);
        onTrimPointsChanged(m_trimInSamples, m_trimOutSamples);
        if (safeThis == nullptr)
          return;
      }
    }
  } else if (m_draggedHandle == DragHandle::TrimOut) {
    int64_t newTrimOut = xToSample(x, bounds.getWidth());
    newTrimOut = juce::jlimit(m_trimInSamples + 1, m_waveformData.totalSamples, newTrimOut);
    if (newTrimOut != m_trimOutSamples) {
      m_trimOutSamples = newTrimOut;
      repaint();
      if (onTrimPointsChanged) {
        juce::Component::SafePointer<WaveformEditor> safeThis(this);
        onTrimPointsChanged(m_trimInSamples, m_trimOutSamples);
        if (safeThis == nullptr)
          return;
      }
    }
  } else if (m_draggedHandle == DragHandle::CueMarker && m_draggedCueIndex >= 0 &&
             m_draggedCueIndex < static_cast<int>(m_cueMarkers.size())) {
    int64_t newSample = xToSample(x, bounds.getWidth());
    newSample = juce::jlimit(int64_t(0), m_waveformData.totalSamples, newSample);
    auto& marker = m_cueMarkers[static_cast<size_t>(m_draggedCueIndex)];
    if (marker.sample != newSample) {
      marker.sample = newSample;
      const auto markerId = marker.id;
      repaint();
      if (onCueMarkerMoved) {
        juce::Component::SafePointer<WaveformEditor> safeThis(this);
        onCueMarkerMoved(markerId, newSample);
        if (safeThis == nullptr)
          return;
      }
    }
  }
}

void WaveformEditor::mouseUp(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);
  if (!requireMessageThread())
    return;

  if (m_isSelecting && m_selectionStart > m_selectionEnd)
    std::swap(m_selectionStart, m_selectionEnd);

  m_draggedHandle = DragHandle::None;
  m_isSelecting = false;
  m_draggedCueIndex = -1;
  updateCursor(DragHandle::None);
}

void WaveformEditor::mouseMove(const juce::MouseEvent& e) {
  if (!requireMessageThread() || !m_waveformData.isValid)
    return;

  const float x = static_cast<float>(e.getPosition().x);
  const float y = static_cast<float>(e.getPosition().y);
  updateCursor(getHandleAt(x, y));
}

void WaveformEditor::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel) {
  if (!requireMessageThread() || !m_waveformData.isValid)
    return;

  if (e.mods.isCommandDown()) {
    const float zoomDelta = wheel.deltaY * 0.5f;
    setZoomLevel(m_zoomLevel * (1.0f + zoomDelta));
  } else {
    const float scrollDelta = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
    setScrollPosition(m_scrollPosition - scrollDelta * 0.1f);
  }
}

//==============================================================================
void WaveformEditor::generateWaveformData(const juce::File& audioFile) {
  if (!requireMessageThread())
    return;

  if (m_isLoading.exchange(true, std::memory_order_acq_rel))
    return;

  const uint64_t generation = m_loadGeneration.load(std::memory_order_acquire);
  juce::Component::SafePointer<WaveformEditor> safeThis(this);
  m_loadPool.addJob(
      new WaveformLoadJob(audioFile.getFullPathName(), safeThis, m_loadGeneration, generation),
      true);
}

bool WaveformEditor::loadWaveformData(const juce::File& audioFile,
                                      std::atomic<uint64_t>& generation,
                                      uint64_t requestedGeneration, WaveformData& result) {
  if (generation.load(std::memory_order_acquire) != requestedGeneration)
    return false;

  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
  if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0 ||
      !std::isfinite(reader->sampleRate) || reader->sampleRate <= 0.0)
    return false;

  const double boundedRate =
      juce::jlimit(1.0, static_cast<double>(std::numeric_limits<int>::max()), reader->sampleRate);
  result.sampleRate = static_cast<int>(boundedRate);
  result.numChannels = juce::jlimit(1, kWaveformMaxChannels, static_cast<int>(reader->numChannels));
  result.totalSamples = reader->lengthInSamples;
  result.minValues.assign(static_cast<size_t>(kWaveformColumns), 0.0f);
  result.maxValues.assign(static_cast<size_t>(kWaveformColumns), 0.0f);

  const int64_t samplesPerPixel = result.totalSamples / kWaveformColumns +
                                  (result.totalSamples % kWaveformColumns != 0 ? 1 : 0);
  const int bufferSamples = static_cast<int>(
      juce::jmin<int64_t>(juce::jmax<int64_t>(1, samplesPerPixel), kWaveformReadChunk));
  juce::AudioBuffer<float> buffer(result.numChannels, bufferSamples);

  for (int column = 0; column < kWaveformColumns; ++column) {
    if (generation.load(std::memory_order_acquire) != requestedGeneration)
      return false;

    const int64_t startSample = static_cast<int64_t>(column) * samplesPerPixel;
    const int64_t endSample = juce::jmin(result.totalSamples, startSample + samplesPerPixel);
    if (startSample >= endSample)
      continue;

    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool columnHasSample = false;
    for (int64_t offset = startSample; offset < endSample;) {
      if (generation.load(std::memory_order_acquire) != requestedGeneration)
        return false;

      const int numSamples =
          static_cast<int>(juce::jmin<int64_t>(bufferSamples, endSample - offset));
      if (numSamples <= 0 || !reader->read(&buffer, 0, numSamples, offset, true, true))
        return false;

      for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
        const auto range = buffer.findMinMax(channel, 0, numSamples);
        const float rangeMin = range.getStart();
        const float rangeMax = range.getEnd();
        if (!std::isfinite(rangeMin) || !std::isfinite(rangeMax))
          continue;

        if (!columnHasSample) {
          minValue = rangeMin;
          maxValue = rangeMax;
          columnHasSample = true;
        } else {
          minValue = juce::jmin(minValue, rangeMin);
          maxValue = juce::jmax(maxValue, rangeMax);
        }
      }
      offset += numSamples;
    }

    if (!columnHasSample)
      return false;

    result.minValues[static_cast<size_t>(column)] = minValue;
    result.maxValues[static_cast<size_t>(column)] = maxValue;
  }

  result.isValid = true;
  return true;
}

void WaveformEditor::applyLoadedWaveform(uint64_t generation, const juce::String& path,
                                         WaveformData data) {
  if (!requireMessageThread() || m_loadGeneration.load(std::memory_order_acquire) != generation)
    return;

  m_isLoading.store(false, std::memory_order_release);
  if (!data.isValid)
    return;

  {
    juce::ScopedLock sl(m_dataLock);
    m_waveformData = std::move(data);
    m_cachedFilePath = path;
    m_trimInSamples = 0;
    m_trimOutSamples = m_waveformData.totalSamples;
  }

  const auto cacheIt =
      std::find_if(m_waveformCache.begin(), m_waveformCache.end(),
                   [&path](const WaveformCacheEntry& entry) { return entry.path == path; });
  if (cacheIt != m_waveformCache.end()) {
    cacheIt->data = m_waveformData;
    m_waveformCache.splice(m_waveformCache.end(), m_waveformCache, cacheIt);
  } else {
    m_waveformCache.push_back({path, m_waveformData});
    if (m_waveformCache.size() > MAX_CACHE_SIZE)
      m_waveformCache.pop_front();
  }
  repaint();
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

//==============================================================================
// Cue markers (G4)
//==============================================================================
void WaveformEditor::addCueMarker(const CueMarker& marker) {
  if (!requireMessageThread())
    return;

  for (auto& m : m_cueMarkers) {
    if (m.id == marker.id) {
      m = marker;
      repaint();
      return;
    }
  }
  if (m_cueMarkers.size() >= 256)
    return;
  m_cueMarkers.push_back(marker);
  repaint();
}

void WaveformEditor::removeCueMarker(const juce::String& id) {
  if (!requireMessageThread())
    return;

  const auto before = m_cueMarkers.size();
  m_cueMarkers.erase(std::remove_if(m_cueMarkers.begin(), m_cueMarkers.end(),
                                    [&](const CueMarker& m) { return m.id == id; }),
                     m_cueMarkers.end());
  if (m_cueMarkers.size() != before)
    repaint();
}

void WaveformEditor::clearCueMarkers() {
  if (!requireMessageThread())
    return;

  if (!m_cueMarkers.empty()) {
    m_cueMarkers.clear();
    repaint();
  }
}

juce::Colour WaveformEditor::cueColour(const CueMarker& marker) const {
  // An explicit (non-transparent) colour wins; otherwise pick a per-type
  // default from the Orpheus tokens.
  if (!marker.color.isTransparent())
    return marker.color;

  switch (marker.type) {
  case CueType::Hook:
    return tokens::lab::tone();
  case CueType::Drop:
    return tokens::lab::danger();
  case CueType::Outro:
    return tokens::lab::warning();
  case CueType::Custom:
  default:
    return tokens::wave::line();
  }
}

int WaveformEditor::cueMarkerIndexAt(float x, float width) const {
  // Topmost (last-drawn) marker within tolerance wins.
  for (int i = static_cast<int>(m_cueMarkers.size()) - 1; i >= 0; --i) {
    const float markerX = sampleToX(m_cueMarkers[static_cast<size_t>(i)].sample, width);
    if (isNearHandle(x, markerX))
      return i;
  }
  return -1;
}

void WaveformEditor::drawCueMarkers(juce::Graphics& g, juce::Rectangle<float> bounds) {
  for (const auto& marker : m_cueMarkers) {
    const float mx = sampleToX(marker.sample, bounds.getWidth()) + bounds.getX();
    if (mx < bounds.getX() - 1.0f || mx > bounds.getRight() + 1.0f)
      continue;

    const juce::Colour c = cueColour(marker);
    g.setColour(c);
    g.drawLine(mx, bounds.getY(), mx, bounds.getBottom(), 1.5f);

    // Flag + optional label at the top.
    juce::Rectangle<float> flag(mx, bounds.getY(), 8.0f, 8.0f);
    g.fillRect(flag);

    if (marker.label.isNotEmpty()) {
      g.setFont(juce::Font(9.0f, juce::Font::bold));
      g.drawText(marker.label, juce::Rectangle<float>(mx + 10.0f, bounds.getY(), 80.0f, 12.0f),
                 juce::Justification::topLeft, false);
    }
  }
}

//==============================================================================
// Audition region (G5)
//==============================================================================
void WaveformEditor::setAuditionRegion(int64_t startSamples, int64_t endSamples) {
  if (!requireMessageThread())
    return;

  if (startSamples > endSamples)
    std::swap(startSamples, endSamples);
  m_auditionStart = juce::jlimit(int64_t(0), m_waveformData.totalSamples, startSamples);
  m_auditionEnd = juce::jlimit(m_auditionStart, m_waveformData.totalSamples, endSamples);
  repaint();
}

void WaveformEditor::setAuditionRegionFromEnd(double seconds) {
  if (!requireMessageThread() || !m_waveformData.isValid || m_waveformData.sampleRate <= 0 ||
      !std::isfinite(seconds))
    return;

  const int64_t span = static_cast<int64_t>(juce::jmax(0.0, seconds) * m_waveformData.sampleRate);
  const int64_t end = m_waveformData.totalSamples;
  setAuditionRegion(juce::jmax(int64_t(0), end - span), end);
}

void WaveformEditor::clearAuditionRegion() {
  if (!requireMessageThread())
    return;

  if (hasAuditionRegion()) {
    m_auditionStart = 0;
    m_auditionEnd = 0;
    repaint();
  }
}

void WaveformEditor::drawAuditionRegion(juce::Graphics& g, juce::Rectangle<float> bounds) {
  if (!hasAuditionRegion())
    return;

  const float x1 = sampleToX(m_auditionStart, bounds.getWidth()) + bounds.getX();
  const float x2 = sampleToX(m_auditionEnd, bounds.getWidth()) + bounds.getX();
  juce::Rectangle<float> region(x1, bounds.getY(), juce::jmax(1.0f, x2 - x1), bounds.getHeight());

  g.setColour(tokens::lab::tone().withAlpha(0.15f));
  g.fillRect(region.getIntersection(bounds));

  g.setColour(tokens::lab::tone().withAlpha(0.5f));
  g.drawLine(x1, bounds.getY(), x1, bounds.getBottom(), 1.0f);
  g.drawLine(x2, bounds.getY(), x2, bounds.getBottom(), 1.0f);
}

//==============================================================================
// Play-follow (G6)
//==============================================================================
void WaveformEditor::setFollowMode(FollowMode mode) {
  if (!requireMessageThread())
    return;
  m_followMode = mode;
  if (mode != FollowMode::Off)
    followPlayhead();
}

void WaveformEditor::setFollowThrottleHz(float hz) {
  if (!requireMessageThread())
    return;
  m_followThrottleHz = std::isfinite(hz) ? juce::jlimit(1.0f, 120.0f, hz) : 30.0f;
}

void WaveformEditor::followPlayhead() {
  if (m_followMode == FollowMode::Off || m_zoomLevel <= 1.0f || m_waveformData.totalSamples <= 0)
    return;

  // Throttle: coalesce follow updates to m_followThrottleHz.
  const int64_t now = juce::Time::currentTimeMillis();
  const int64_t minInterval = static_cast<int64_t>(1000.0f / m_followThrottleHz);
  if (now - m_lastFollowMs < minInterval)
    return;
  m_lastFollowMs = now;

  const float visibleRatio = 1.0f / m_zoomLevel; // fraction visible
  const float playheadRatio =
      static_cast<float>(m_playheadPosition) / static_cast<float>(m_waveformData.totalSamples);
  const float viewStart = m_scrollPosition;
  const float viewEnd = viewStart + visibleRatio;

  float newScroll = m_scrollPosition;
  if (m_followMode == FollowMode::Center) {
    newScroll = playheadRatio - visibleRatio * 0.5f;
  } else // Page: only jump when the playhead leaves the viewport
  {
    if (playheadRatio < viewStart || playheadRatio > viewEnd)
      newScroll = playheadRatio; // page so playhead sits at the left edge
  }

  newScroll = juce::jlimit(0.0f, 1.0f - visibleRatio, newScroll);
  if (std::abs(newScroll - m_scrollPosition) > 1.0e-4f)
    setScrollPosition(newScroll); // repaints
}

} // namespace shmui
