/*
  ==============================================================================

    WaveformVisualizer.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of waveform visualization components.

  ==============================================================================
*/

#include "WaveformVisualizer.h"
#include "../Utils/MessageThread.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace shmui {

namespace {
float clampNormalized(float value) {
  if (std::isnan(value))
    return 0.0f;
  if (value == std::numeric_limits<float>::infinity())
    return 1.0f;
  if (value == -std::numeric_limits<float>::infinity())
    return 0.0f;
  return juce::jlimit(0.0f, 1.0f, value);
}

void sanitizeWaveformStyle(WaveformStyle& style) {
  const auto finiteOr = [](float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
  };

  style.barWidth = juce::jlimit(0.5f, 128.0f, finiteOr(style.barWidth, 4.0f));
  style.barHeight = juce::jlimit(0.0f, 4096.0f, finiteOr(style.barHeight, 4.0f));
  style.barGap = juce::jlimit(0.0f, 128.0f, finiteOr(style.barGap, 2.0f));
  style.barRadius = juce::jlimit(0.0f, 128.0f, finiteOr(style.barRadius, 2.0f));
  style.fadeWidth = juce::jlimit(0.0f, 4096.0f, finiteOr(style.fadeWidth, 24.0f));
  style.alphaMin = juce::jlimit(0.0f, 1.0f, finiteOr(style.alphaMin, 0.3f));
  style.alphaMax = juce::jlimit(style.alphaMin, 1.0f, finiteOr(style.alphaMax, 1.0f));
  style.heightScale = juce::jlimit(0.0f, 1.0f, finiteOr(style.heightScale, 0.8f));
}
} // namespace

//==============================================================================
// WaveformVisualizer

WaveformVisualizer::WaveformVisualizer() {
  setOpaque(false);
  style = WaveformStyle::fromTheme(defaultTheme());
  addDefaultThemeListener(this);
}

WaveformVisualizer::~WaveformVisualizer() {
  removeDefaultThemeListener(this);
}

void WaveformVisualizer::setData(const std::vector<float>& data) {
  if (!requireMessageThread())
    return;

  const auto count = std::min(data.size(), static_cast<std::size_t>(kMaxDataPoints));
  waveformData.assign(count, 0.0f);
  for (std::size_t i = 0; i < count; ++i)
    waveformData[i] = clampNormalized(data[i]);
  repaint();
}

void WaveformVisualizer::setStyle(const WaveformStyle& newStyle) {
  if (!requireMessageThread())
    return;

  style = newStyle;
  sanitizeWaveformStyle(style);
  usesDefaultThemeStyle_ = false;
  repaint();
}

void WaveformVisualizer::useDefaultThemeStyle() {
  if (!requireMessageThread())
    return;

  usesDefaultThemeStyle_ = true;
  style = WaveformStyle::fromTheme(defaultTheme());
  sanitizeWaveformStyle(style);
  repaint();
}

void WaveformVisualizer::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !usesDefaultThemeStyle_)
    return;

  style = WaveformStyle::fromTheme(theme);
  sanitizeWaveformStyle(style);
  repaint();
}

void WaveformVisualizer::setBarColour(const juce::Colour& colour) {
  if (!requireMessageThread())
    return;

  style.barColour = colour;
  usesDefaultThemeStyle_ = false;
  repaint();
}

void WaveformVisualizer::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  renderWaveform(g, bounds);

  if (style.fadeEdges && style.fadeWidth > 0.0f && bounds.getWidth() > 0.0f) {
    applyEdgeFade(g, bounds);
  }
}

void WaveformVisualizer::renderWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  if (waveformData.empty())
    return;

  const int barCount = getBarCount();
  if (barCount <= 0)
    return;

  const float centerY = bounds.getCentreY();
  const float maxHeight = bounds.getHeight() * style.heightScale;

  for (int i = 0; i < barCount; ++i) {
    // Map bar index to data index
    const int dataIndex =
        static_cast<int>((static_cast<float>(i) / barCount) * waveformData.size());
    const float value = (dataIndex >= 0 && dataIndex < static_cast<int>(waveformData.size()))
                            ? waveformData[dataIndex]
                            : 0.0f;

    // Calculate bar dimensions
    const float barHeight = std::max(style.barHeight, value * maxHeight);
    const float x = bounds.getX() + i * (style.barWidth + style.barGap);
    const float y = centerY - barHeight / 2.0f;

    // Set alpha based on value
    const float alpha = style.alphaMin + value * (style.alphaMax - style.alphaMin);
    g.setColour(style.barColour.withAlpha(alpha));

    // Draw bar
    if (style.barRadius > 0.0f) {
      g.fillRoundedRectangle(x, y, style.barWidth, barHeight, style.barRadius);
    } else {
      g.fillRect(x, y, style.barWidth, barHeight);
    }
  }
}

void WaveformVisualizer::applyEdgeFade(juce::Graphics& g, const juce::Rectangle<float>& bounds) {
  // Create edge fade using destination-out compositing
  // In JUCE, we simulate this by drawing transparent gradients

  const float fadePercent = std::min(0.2f, style.fadeWidth / bounds.getWidth());

  juce::ColourGradient leftGradient = juce::ColourGradient::horizontal(
      juce::Colours::white, bounds.getX(), juce::Colours::transparentWhite,
      bounds.getX() + bounds.getWidth() * fadePercent);

  // Right fade
  juce::ColourGradient rightGradient = juce::ColourGradient::horizontal(
      juce::Colours::transparentWhite, bounds.getRight() - bounds.getWidth() * fadePercent,
      juce::Colours::white, bounds.getRight());

  // Note: True destination-out compositing requires custom blending
  // For now, this creates a visual approximation
  g.setGradientFill(leftGradient);
  g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth() * fadePercent, bounds.getHeight());

  g.setGradientFill(rightGradient);
  g.fillRect(bounds.getRight() - bounds.getWidth() * fadePercent, bounds.getY(),
             bounds.getWidth() * fadePercent, bounds.getHeight());
}

void WaveformVisualizer::mouseDown(const juce::MouseEvent& e) {
  if (!requireMessageThread() || !onBarClick || waveformData.empty())
    return;

  const int barCount = getBarCount();
  if (barCount <= 0)
    return;

  const float step = style.barWidth + style.barGap;
  const int barIndex = juce::jlimit(0, barCount - 1, static_cast<int>(e.position.x / step));
  const int dataIndex =
      static_cast<int>((static_cast<float>(barIndex) / barCount) * waveformData.size());

  if (dataIndex >= 0 && dataIndex < static_cast<int>(waveformData.size())) {
    const float value = waveformData[static_cast<std::size_t>(dataIndex)];
    juce::Component::SafePointer<WaveformVisualizer> safeThis(this);
    onBarClick(dataIndex, value);
    juce::ignoreUnused(safeThis);
  }
}

void WaveformVisualizer::resized() {
  // Trigger repaint on resize
  repaint();
}

int WaveformVisualizer::getBarCount() const {
  const float step = style.barWidth + style.barGap;
  if (!std::isfinite(step) || step <= 0.0f || getWidth() <= 0)
    return 0;

  const int count = static_cast<int>(static_cast<float>(getWidth()) / step);
  return juce::jlimit(0, static_cast<int>(kMaxDataPoints), count);
}

//==============================================================================
// ScrollingWaveformVisualizer

ScrollingWaveformVisualizer::ScrollingWaveformVisualizer() {
  setOpaque(false);
}

ScrollingWaveformVisualizer::~ScrollingWaveformVisualizer() {
  stopTimer();
}

void ScrollingWaveformVisualizer::setSpeed(float pixelsPerSecond) {
  if (!requireMessageThread())
    return;

  scrollSpeed =
      std::isfinite(pixelsPerSecond) ? juce::jlimit(0.0f, 10000.0f, pixelsPerSecond) : 0.0f;
}

void ScrollingWaveformVisualizer::setBarCount(int count) {
  if (!requireMessageThread())
    return;

  targetBarCount = juce::jlimit(1, 8192, count);
}

void ScrollingWaveformVisualizer::start() {
  if (!requireMessageThread())
    return;

  scrollRequested = true;
  lastTime = juce::Time::currentTimeMillis();
  updateTimerState();
}

void ScrollingWaveformVisualizer::stop() {
  if (!requireMessageThread())
    return;

  scrollRequested = false;
  updateTimerState();
}

void ScrollingWaveformVisualizer::setDataSource(const std::vector<float>* source) {
  if (!requireMessageThread())
    return;

  dataSource.clear();
  if (source != nullptr) {
    const auto count = std::min(source->size(), static_cast<std::size_t>(kMaxDataPoints));
    dataSource.assign(count, 0.0f);
    for (std::size_t i = 0; i < count; ++i)
      dataSource[i] = clampNormalized((*source)[i]);
  }
  dataIndex = 0;
}

void ScrollingWaveformVisualizer::setSeed(uint32_t newSeed) {
  if (!requireMessageThread())
    return;

  randomSeed = newSeed;
}

void ScrollingWaveformVisualizer::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  const float centerY = bounds.getCentreY();
  const float maxHeight = bounds.getHeight() * 0.6f; // From shmui

  // Draw all bars
  for (const auto& bar : bars) {
    if (bar.x >= 0.0f && bar.x + style.barWidth <= bounds.getWidth()) {
      const float barHeight = std::max(style.barHeight, bar.height * maxHeight);
      const float y = centerY - barHeight / 2.0f;

      const float alpha = style.alphaMin + bar.height * (style.alphaMax - style.alphaMin);
      g.setColour(style.barColour.withAlpha(alpha));

      if (style.barRadius > 0.0f) {
        g.fillRoundedRectangle(bar.x, y, style.barWidth, barHeight, style.barRadius);
      } else {
        g.fillRect(bar.x, y, style.barWidth, barHeight);
      }
    }
  }

  // Apply edge fade
  if (style.fadeEdges && style.fadeWidth > 0.0f && bounds.getWidth() > 0.0f) {
    applyEdgeFade(g, bounds);
  }
}

void ScrollingWaveformVisualizer::resized() {
  if (!requireMessageThread() || !bars.empty())
    return;

  const float step = style.barWidth + style.barGap;
  if (!std::isfinite(step) || step <= 0.0f)
    return;

  float currentX = static_cast<float>(getWidth());
  Interpolation::SeedRandom rng(randomSeed);
  const std::size_t maxBars = static_cast<std::size_t>(targetBarCount) * 2U;
  std::size_t created = 0;

  while (currentX > -step && created < maxBars) {
    bars.push_back({currentX, 0.2f + rng.next() * 0.6f});
    currentX -= step;
    ++created;
  }
}

void ScrollingWaveformVisualizer::timerCallback() {
  if (!requireMessageThread())
    return;
  if (!isShowing() || !scrollRequested) {
    updateTimerState();
    return;
  }

  const int64_t currentTime = juce::Time::currentTimeMillis();
  const float elapsed = static_cast<float>(currentTime - lastTime) / 1000.0f;
  const float deltaTime = std::isfinite(elapsed) ? juce::jlimit(0.0f, 0.25f, elapsed) : 0.0f;
  lastTime = currentTime;

  for (auto& bar : bars)
    bar.x -= scrollSpeed * deltaTime;

  removeOldBars();
  while (bars.empty() || bars.back().x < getWidth()) {
    addNewBar();
    if (bars.size() > static_cast<std::size_t>(targetBarCount * 2))
      break;
  }
  repaint();
}

void ScrollingWaveformVisualizer::visibilityChanged() {
  if (requireMessageThread())
    updateTimerState();
}

void ScrollingWaveformVisualizer::updateTimerState() {
  if (!requireMessageThread())
    return;
  if (isShowing() && scrollRequested)
    startTimerHz(60);
  else
    stopTimer();
}

void ScrollingWaveformVisualizer::addNewBar() {
  const float step = style.barWidth + style.barGap;
  const float lastX = bars.empty() ? static_cast<float>(getWidth()) : bars.back().x + step;

  float newHeight = 0.3f;
  if (!dataSource.empty()) {
    newHeight = dataSource[static_cast<std::size_t>(dataIndex) % dataSource.size()];
    dataIndex = (dataIndex + 1) % static_cast<int>(dataSource.size());
  } else {
    const float time = static_cast<float>(juce::Time::currentTimeMillis()) / 1000.0f;
    const float uniqueIndex = static_cast<float>(bars.size()) + time * 0.01f;

    const float wave1 = std::sin(uniqueIndex * 0.1f) * 0.2f;
    const float wave2 = std::cos(uniqueIndex * 0.05f) * 0.15f;
    const float randomComponent =
        Interpolation::seededRandom(static_cast<float>(randomSeed) * 10000.0f +
                                    uniqueIndex * 137.5f) *
        0.4f;
    newHeight = juce::jlimit(0.1f, 0.9f, 0.3f + wave1 + wave2 + randomComponent);
  }

  bars.push_back({lastX, clampNormalized(newHeight)});
}

void ScrollingWaveformVisualizer::removeOldBars() {
  const float step = style.barWidth + style.barGap;

  bars.erase(std::remove_if(bars.begin(), bars.end(),
                            [step](const Bar& bar) { return bar.x + step < 0.0f; }),
             bars.end());
}

//==============================================================================
// AudioScrubberVisualizer

AudioScrubberVisualizer::AudioScrubberVisualizer() {
  // Scrubber defaults
  style.barWidth = 3.0f;
  style.barGap = 1.0f;
  style.barRadius = 1.0f;
  style.fadeEdges = false;

  setOpaque(false);
}

void AudioScrubberVisualizer::setCurrentTime(float time) {
  if (!requireMessageThread())
    return;

  if (!isDragging && duration > 0.0f) {
    currentTime = std::isfinite(time) ? juce::jlimit(0.0f, duration, time) : 0.0f;
    localProgress = currentTime / duration;
    repaint();
  }
}

void AudioScrubberVisualizer::setDuration(float dur) {
  if (!requireMessageThread())
    return;

  duration = std::isfinite(dur) ? std::max(0.0f, dur) : 0.0f;
  if (duration > 0.0f) {
    currentTime = juce::jlimit(0.0f, duration, currentTime);
    localProgress = currentTime / duration;
  } else {
    currentTime = 0.0f;
    localProgress = 0.0f;
  }
  repaint();
}

float AudioScrubberVisualizer::getProgress() const {
  return localProgress;
}

void AudioScrubberVisualizer::setShowHandle(bool show) {
  if (!requireMessageThread())
    return;

  showHandle = show;
  repaint();
}

void AudioScrubberVisualizer::setPlayheadColour(const juce::Colour& colour) {
  if (!requireMessageThread())
    return;

  playheadColour = colour;
  repaint();
}

void AudioScrubberVisualizer::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  WaveformVisualizer::paint(g);

  const float progressX = bounds.getX() + localProgress * bounds.getWidth();
  g.setColour(playheadColour.withAlpha(0.2f));
  g.fillRect(bounds.getX(), bounds.getY(), progressX - bounds.getX(), bounds.getHeight());

  g.setColour(playheadColour);
  g.drawVerticalLine(static_cast<int>(progressX), bounds.getY(), bounds.getBottom());

  if (showHandle) {
    const float handleSize = 16.0f;
    const float handleY = bounds.getCentreY();
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillEllipse(progressX - handleSize / 2.0f + 1.0f, handleY - handleSize / 2.0f + 1.0f,
                  handleSize, handleSize);
    g.setColour(playheadColour);
    g.fillEllipse(progressX - handleSize / 2.0f, handleY - handleSize / 2.0f, handleSize,
                  handleSize);
    g.setColour(juce::Colours::white);
    g.drawEllipse(progressX - handleSize / 2.0f, handleY - handleSize / 2.0f, handleSize,
                  handleSize, 2.0f);
  }
}

void AudioScrubberVisualizer::mouseDown(const juce::MouseEvent& e) {
  if (!requireMessageThread())
    return;
  isDragging = true;
  handleScrub(e.position.x);
}

void AudioScrubberVisualizer::mouseDrag(const juce::MouseEvent& e) {
  if (!requireMessageThread())
    return;
  if (isDragging)
    handleScrub(e.position.x);
}

void AudioScrubberVisualizer::mouseUp(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);
  if (requireMessageThread())
    isDragging = false;
}

void AudioScrubberVisualizer::handleScrub(float x) {
  if (!requireMessageThread())
    return;

  const auto bounds = getLocalBounds().toFloat();
  if (bounds.getWidth() <= 0.0f || duration <= 0.0f)
    return;

  const float clampedX = juce::jlimit(bounds.getX(), bounds.getRight(), x);
  localProgress = juce::jlimit(0.0f, 1.0f, (clampedX - bounds.getX()) / bounds.getWidth());
  const float newTime = localProgress * duration;

  if (onSeek) {
    juce::Component::SafePointer<AudioScrubberVisualizer> safeThis(this);
    onSeek(newTime);
    if (safeThis == nullptr)
      return;
  }
  repaint();
}

//==============================================================================
// LiveWaveformVisualizer

LiveWaveformVisualizer::LiveWaveformVisualizer() {
  style = WaveformStyle::fromTheme(defaultTheme());
  style.barWidth = 3.0f;
  style.barGap = 1.0f;
  style.barRadius = 1.0f;
  sanitizeWaveformStyle(style);

  history.resize(static_cast<std::size_t>(historySize), 0.0f);
  historySnapshot.reserve(static_cast<std::size_t>(historySize));

  setOpaque(false);
  addDefaultThemeListener(this);
}

LiveWaveformVisualizer::~LiveWaveformVisualizer() {
  removeDefaultThemeListener(this);
  stopTimer();
}

void LiveWaveformVisualizer::setAudioAnalyzer(std::shared_ptr<AudioAnalyzer> analyzer) {
  if (!requireMessageThread())
    return;

  audioAnalyzer = std::move(analyzer);
  updateTimerState();
}

void LiveWaveformVisualizer::setActive(bool isActive) {
  if (!requireMessageThread())
    return;

  if (active != isActive) {
    active = isActive;
    if (active)
      clearHistory();
  }

  updateTimerState();
}

void LiveWaveformVisualizer::setHistorySize(int size) {
  if (!requireMessageThread())
    return;

  const int boundedSize = juce::jlimit(1, 8192, size);
  if (historySize == boundedSize)
    return;

  historySize = boundedSize;
  history.assign(static_cast<std::size_t>(historySize), 0.0f);
  historyWriteIndex = 0;
  historyCount = 0;
  historySnapshot.clear();
  historySnapshotDirty = true;
  repaint();
}

void LiveWaveformVisualizer::setUpdateRate(int milliseconds) {
  if (!requireMessageThread())
    return;

  updateRate = juce::jlimit(1, 1000, milliseconds);
  updateTimerState();
}

void LiveWaveformVisualizer::setSensitivity(float sens) {
  if (!requireMessageThread() || !std::isfinite(sens))
    return;

  sensitivity = juce::jlimit(0.0f, 16.0f, sens);
}

void LiveWaveformVisualizer::setStyle(const WaveformStyle& newStyle) {
  if (!requireMessageThread())
    return;

  style = newStyle;
  sanitizeWaveformStyle(style);
  usesDefaultThemeStyle_ = false;
  repaint();
}

void LiveWaveformVisualizer::useDefaultThemeStyle() {
  if (!requireMessageThread())
    return;

  usesDefaultThemeStyle_ = true;
  style = WaveformStyle::fromTheme(defaultTheme());
  style.barWidth = 3.0f;
  style.barGap = 1.0f;
  style.barRadius = 1.0f;
  sanitizeWaveformStyle(style);
  repaint();
}

void LiveWaveformVisualizer::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !usesDefaultThemeStyle_)
    return;

  style = WaveformStyle::fromTheme(theme);
  style.barWidth = 3.0f;
  style.barGap = 1.0f;
  style.barRadius = 1.0f;
  sanitizeWaveformStyle(style);
  repaint();
}

void LiveWaveformVisualizer::clearHistory() {
  if (!requireMessageThread())
    return;

  std::fill(history.begin(), history.end(), 0.0f);
  historyWriteIndex = 0;
  historyCount = 0;
  historySnapshot.clear();
  historySnapshotDirty = true;
  repaint();
}

void LiveWaveformVisualizer::paint(juce::Graphics& g) {
  if (historyCount <= 0 || history.empty())
    return;

  const auto bounds = getLocalBounds().toFloat();
  const float step = style.barWidth + style.barGap;
  if (!std::isfinite(step) || step <= 0.0f)
    return;

  const int barCount = static_cast<int>(bounds.getWidth() / step);
  const float centerY = bounds.getCentreY();
  const float maxHeight = bounds.getHeight() * 0.7f;
  for (int i = 0; i < barCount && i < historyCount; ++i) {
    const std::size_t offset = static_cast<std::size_t>(i);
    const std::size_t dataIndex =
        (historyWriteIndex + history.size() - 1 - offset) % history.size();
    const float value = clampNormalized(history[dataIndex]);
    const float x = bounds.getRight() - (i + 1) * step;
    const float barHeight = std::max(style.barHeight, value * maxHeight);
    const float y = centerY - barHeight / 2.0f;

    const float alpha = style.alphaMin + value * (style.alphaMax - style.alphaMin);
    g.setColour(style.barColour.withAlpha(alpha));

    if (style.barRadius > 0.0f)
      g.fillRoundedRectangle(x, y, style.barWidth, barHeight, style.barRadius);
    else
      g.fillRect(x, y, style.barWidth, barHeight);
  }

  if (style.fadeEdges && style.fadeWidth > 0.0f && bounds.getWidth() > 0.0f) {
    const float fadePercent = std::min(0.2f, style.fadeWidth / bounds.getWidth());

    juce::ColourGradient leftGradient = juce::ColourGradient::horizontal(
        juce::Colours::white, bounds.getX(), juce::Colours::transparentWhite,
        bounds.getX() + bounds.getWidth() * fadePercent);
    juce::ColourGradient rightGradient = juce::ColourGradient::horizontal(
        juce::Colours::transparentWhite, bounds.getRight() - bounds.getWidth() * fadePercent,
        juce::Colours::white, bounds.getRight());

    g.setGradientFill(leftGradient);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth() * fadePercent, bounds.getHeight());
    g.setGradientFill(rightGradient);
    g.fillRect(bounds.getRight() - bounds.getWidth() * fadePercent, bounds.getY(),
               bounds.getWidth() * fadePercent, bounds.getHeight());
  }
}

void LiveWaveformVisualizer::resized() {
  repaint();
}

void LiveWaveformVisualizer::timerCallback() {
  if (!isShowing() || !active || audioAnalyzer == nullptr) {
    updateTimerState();
    return;
  }

  const auto analyzer = audioAnalyzer;
  const float level = analyzer != nullptr ? analyzer->getRMSLevel() * sensitivity : 0.0f;
  const float clampedLevel = std::isfinite(level) ? juce::jlimit(0.05f, 1.0f, level) : 0.05f;

  history[historyWriteIndex] = clampedLevel;
  historyWriteIndex = (historyWriteIndex + 1) % history.size();
  historyCount = std::min(historyCount + 1, historySize);
  historySnapshotDirty = true;
  repaint();
}

const std::vector<float>& LiveWaveformVisualizer::getHistory() const {
  if (!historySnapshotDirty)
    return historySnapshot;

  historySnapshot.resize(static_cast<std::size_t>(historyCount));
  if (historyCount > 0 && !history.empty()) {
    const std::size_t oldest = historyCount == historySize ? historyWriteIndex : 0;
    for (int i = 0; i < historyCount; ++i) {
      const std::size_t index = (oldest + static_cast<std::size_t>(i)) % history.size();
      historySnapshot[static_cast<std::size_t>(i)] = history[index];
    }
  }

  historySnapshotDirty = false;
  return historySnapshot;
}

void LiveWaveformVisualizer::visibilityChanged() {
  updateTimerState();
}

void LiveWaveformVisualizer::updateTimerState() {
  if (isShowing() && active && audioAnalyzer != nullptr)
    startTimer(updateRate);
  else
    stopTimer();
}

} // namespace shmui
