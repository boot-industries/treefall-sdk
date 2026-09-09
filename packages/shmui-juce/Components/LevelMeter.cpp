/*
  ==============================================================================

    LevelMeter.cpp
    Created: shmui Component Library

    Level meter implementation.

  ==============================================================================
*/

#include "LevelMeter.h"
#include <algorithm>
#include <cmath>
namespace shmui {

class LevelMeter::ShowingStateWatcher final : public juce::ComponentMovementWatcher {
public:
  explicit ShowingStateWatcher(LevelMeter& owner)
      : juce::ComponentMovementWatcher(&owner), m_owner(owner) {}

  void componentMovedOrResized(bool, bool) override {}

  void componentPeerChanged() override {
    m_owner.updateTimerState();
  }

  void componentVisibilityChanged() override {
    m_owner.updateTimerState();
  }

private:
  LevelMeter& m_owner;
};

//==============================================================================
LevelMeter::LevelMeter() : LevelMeter(1) {}

LevelMeter::LevelMeter(int numChannels)
    : m_numChannels(juce::jlimit(1, MAX_CHANNELS, numChannels)) {
  for (int i = 0; i < MAX_CHANNELS; ++i) {
    m_inputLevelPairs[i].store(packLevelPair({}), std::memory_order_relaxed);
    m_displayLevels[i] = 0.0f;
    m_displayRmsLevels[i] = 0.0f;
    m_peakHolds[i] = 0.0f;
    m_peakHoldTimes[i] = 0;
    m_clipped[i] = false;
    m_peakRmsNeedleDb[i] = m_minDB;
    m_peakRmsNeedleTimes[i] = 0;
  }

  m_style = LevelMeterStyle::fromTheme(defaultTheme());
  sanitizeStyle();
  setBallistics(MeterBallistics::Peak);
  addDefaultThemeListener(this);
  m_showingStateWatcher = std::make_unique<ShowingStateWatcher>(*this);
  updateTimerState();
}

LevelMeter::~LevelMeter() {
  m_showingStateWatcher.reset();
  stopTimer();
  removeDefaultThemeListener(this);
}

uint64_t LevelMeter::packLevelPair(LevelPair pair) noexcept {
  const auto peakBits = static_cast<uint64_t>(std::bit_cast<uint32_t>(pair.peak));
  const auto rmsBits = static_cast<uint64_t>(std::bit_cast<uint32_t>(pair.rms));
  return (peakBits << 32U) | rmsBits;
}

LevelMeter::LevelPair LevelMeter::unpackLevelPair(uint64_t packed) noexcept {
  return {std::bit_cast<float>(static_cast<uint32_t>(packed >> 32U)),
          std::bit_cast<float>(static_cast<uint32_t>(packed))};
}

LevelMeter::SegmentLayout LevelMeter::calculateSegmentLayout(float signalAxisLength,
                                                             float segmentLength,
                                                             float requestedGap,
                                                             float displayScale) noexcept {
  const auto finiteOr = [](float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
  };
  const float safeLength = juce::jlimit(
      1.0f, 1000.0f, std::isfinite(segmentLength) && segmentLength > 0.0f ? segmentLength : 4.0f);
  const float safeGap = juce::jlimit(0.0f, 1000.0f, finiteOr(requestedGap, 1.0f));
  const float safeScale = juce::jlimit(
      0.001f, 1000.0f, std::isfinite(displayScale) && displayScale > 0.0f ? displayScale : 1.0f);
  const float resolvedGap = safeGap > 0.0f ? std::ceil(safeGap * safeScale) / safeScale : 0.0f;
  const float safeAxisLength = juce::jmax(0.0f, finiteOr(signalAxisLength, 0.0f));
  const float pitch = safeLength + resolvedGap;
  const float requestedCount =
      pitch > 0.0f ? std::floor((safeAxisLength + resolvedGap) / pitch) : 0.0f;
  const int count = static_cast<int>(
      juce::jlimit(0.0f, 1024.0f, std::isfinite(requestedCount) ? requestedCount : 0.0f));
  const float occupied = count > 0 ? count * safeLength + (count - 1) * resolvedGap : 0.0f;
  return {count, safeLength, resolvedGap, juce::jmax(0.0f, (safeAxisLength - occupied) * 0.5f)};
}

//==============================================================================
void LevelMeter::setLevel(int channel, float level) {
  setLevelPair(channel, level, level);
}

void LevelMeter::setLevelDB(int channel, float dB) {
  setLevelPairDB(channel, dB, dB);
}

void LevelMeter::setLevelPair(int channel, float peakLevel, float rmsLevel) {
  if (channel < 0 || channel >= m_numChannels)
    return;

  const auto sanitize = [](float level) {
    return std::isfinite(level) ? juce::jmax(0.0f, level) : 0.0f;
  };
  const float peak = sanitize(peakLevel);
  const float rms = juce::jmin(peak, sanitize(rmsLevel));
  m_inputLevelPairs[static_cast<size_t>(channel)].store(packLevelPair({peak, rms}),
                                                        std::memory_order_relaxed);
}

void LevelMeter::setLevelPairDB(int channel, float peakDb, float rmsDb) {
  const auto linearFromDb = [](float dB) {
    const float safeDb =
        std::isfinite(dB) ? juce::jlimit(-200.0f, 200.0f, dB) : (dB > 0.0f ? 200.0f : -200.0f);
    return std::pow(10.0f, safeDb / 20.0f);
  };
  setLevelPair(channel, linearFromDb(peakDb), linearFromDb(rmsDb));
}

void LevelMeter::setLevels(const std::vector<float>& levels) {
  const int count = juce::jmin(static_cast<int>(levels.size()), m_numChannels);
  for (int i = 0; i < count; ++i)
    setLevel(i, levels[static_cast<size_t>(i)]);
}

void LevelMeter::reset() {
  if (!requireMessageThread())
    return;

  for (int i = 0; i < m_numChannels; ++i) {
    m_inputLevelPairs[static_cast<size_t>(i)].store(packLevelPair({}), std::memory_order_relaxed);
    m_displayLevels[static_cast<size_t>(i)] = 0.0f;
    m_displayRmsLevels[static_cast<size_t>(i)] = 0.0f;
    m_peakHolds[static_cast<size_t>(i)] = 0.0f;
    m_peakHoldTimes[static_cast<size_t>(i)] = 0;
    m_clipped[static_cast<size_t>(i)] = false;
    m_peakRmsNeedleDb[static_cast<size_t>(i)] = m_minDB;
    m_peakRmsNeedleTimes[static_cast<size_t>(i)] = 0;
  }
  repaint();
}

//==============================================================================
void LevelMeter::setNumChannels(int numChannels) {
  if (!requireMessageThread())
    return;
  m_numChannels = juce::jlimit(1, MAX_CHANNELS, numChannels);
  reset();
}

void LevelMeter::setBallistics(MeterBallistics ballistics) {
  if (!requireMessageThread())
    return;

  m_ballistics = ballistics;
  switch (ballistics) {
  case MeterBallistics::Peak:
    m_attackCoeff = 1.0f;
    m_releaseCoeff = 0.05f;
    break;
  case MeterBallistics::VU:
    m_attackCoeff = 0.3f;
    m_releaseCoeff = 0.3f;
    break;
  case MeterBallistics::PPM:
    m_attackCoeff = 0.8f;
    m_releaseCoeff = 0.02f;
    break;
  case MeterBallistics::PeakRms:
    m_attackCoeff = 1.0f;
    m_releaseCoeff = 0.0f;
    break;
  }
}

void LevelMeter::setVertical(bool vertical) {
  if (!requireMessageThread())
    return;
  if (m_isVertical != vertical) {
    m_isVertical = vertical;
    repaint();
  }
}

void LevelMeter::setPeakHoldTime(int milliseconds) {
  if (!requireMessageThread())
    return;
  m_peakHoldTimeMs = juce::jlimit(0, 60000, milliseconds);
}

void LevelMeter::setDBRange(float minDB, float maxDB) {
  if (!requireMessageThread())
    return;

  const float safeMin = std::isfinite(minDB) ? juce::jlimit(-200.0f, 199.999f, minDB) : -60.0f;
  const float safeMax = std::isfinite(maxDB) ? juce::jlimit(-200.0f, 200.0f, maxDB) : 6.0f;
  m_minDB = safeMin;
  m_maxDB = juce::jlimit(m_minDB + 0.001f, 200.0f, safeMax);
  repaint();
}

void LevelMeter::setStyle(const LevelMeterStyle& style) {
  if (!requireMessageThread())
    return;
  m_style = style;
  sanitizeStyle();
  m_usesDefaultThemeStyle = false;
  repaint();
}

void LevelMeter::useDefaultThemeStyle() {
  if (!requireMessageThread())
    return;
  m_usesDefaultThemeStyle = true;
  m_style = LevelMeterStyle::fromTheme(defaultTheme());
  sanitizeStyle();
  repaint();
}

void LevelMeter::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread() || !m_usesDefaultThemeStyle)
    return;

  m_style = LevelMeterStyle::fromTheme(theme);
  sanitizeStyle();
  repaint();
}

void LevelMeter::sanitizeStyle() {
  const auto finiteOr = [](float value, float fallback) {
    return std::isfinite(value) ? value : fallback;
  };
  m_style.yellowThreshold =
      juce::jlimit(-200.0f, 200.0f, finiteOr(m_style.yellowThreshold, -12.0f));
  m_style.redThreshold = juce::jlimit(-200.0f, 200.0f, finiteOr(m_style.redThreshold, -3.0f));
  if (m_style.redThreshold < m_style.yellowThreshold)
    m_style.redThreshold = m_style.yellowThreshold;
  m_style.clipThreshold = juce::jlimit(-200.0f, 200.0f, finiteOr(m_style.clipThreshold, 0.0f));
  m_style.meterWidth = juce::jlimit(1.0f, 1000.0f, finiteOr(m_style.meterWidth, 8.0f));
  m_style.meterGap = juce::jmax(0.0f, finiteOr(m_style.meterGap, 2.0f));
  m_style.cornerRadius = juce::jlimit(0.0f, 1000.0f, finiteOr(m_style.cornerRadius, 2.0f));
  m_style.peakHoldWidth = juce::jlimit(0.0f, 1000.0f, finiteOr(m_style.peakHoldWidth, 2.0f));
  m_style.segmentLength = juce::jlimit(
      1.0f, 1000.0f,
      std::isfinite(m_style.segmentLength) && m_style.segmentLength > 0.0f ? m_style.segmentLength
                                                                           : 4.0f);
  m_style.segmentGap = juce::jlimit(0.0f, 1000.0f, finiteOr(m_style.segmentGap, 1.0f));
  m_style.greenToYellowTransitionSegments =
      juce::jlimit(0, 256, m_style.greenToYellowTransitionSegments);
  m_style.peakRmsNeedleWidth =
      juce::jlimit(0.0f, 1000.0f, finiteOr(m_style.peakRmsNeedleWidth, 2.0f));
  m_style.peakRmsNeedleReleaseDbPerSecond =
      juce::jlimit(0.0f, 200.0f, finiteOr(m_style.peakRmsNeedleReleaseDbPerSecond, 14.5f));
}

void LevelMeter::clearClip() {
  if (!requireMessageThread())
    return;
  for (int i = 0; i < m_numChannels; ++i)
    m_clipped[static_cast<size_t>(i)] = false;
  repaint();
}

bool LevelMeter::hasClipped() const {
  for (int i = 0; i < m_numChannels; ++i) {
    if (m_clipped[i])
      return true;
  }
  return false;
}

bool LevelMeter::hasClipped(int channel) const {
  if (channel >= 0 && channel < m_numChannels)
    return m_clipped[channel];
  return false;
}

//==============================================================================
void LevelMeter::enableHistory(int capacity) {
  if (!requireMessageThread())
    return;
  capacity = juce::jlimit(1, MAX_HISTORY_EVENTS, capacity);
  m_history.assign(static_cast<size_t>(capacity), LevelEvent{});
  m_historyHead = 0;
  m_historyCount = 0;
}

void LevelMeter::disableHistory() {
  if (!requireMessageThread())
    return;
  m_history.clear();
  m_history.shrink_to_fit();
  m_historyHead = 0;
  m_historyCount = 0;
}

int LevelMeter::getHistorySize() const {
  return m_historyCount;
}

LevelEvent LevelMeter::getHistoryEntry(int indexFromNewest) const {
  if (indexFromNewest < 0 || indexFromNewest >= m_historyCount || m_history.empty())
    return LevelEvent{};
  const int cap = static_cast<int>(m_history.size());
  const int idx = ((m_historyHead - 1 - indexFromNewest) % cap + cap) % cap;
  return m_history[static_cast<size_t>(idx)];
}

void LevelMeter::clearHistory() {
  if (!requireMessageThread())
    return;
  m_historyHead = 0;
  m_historyCount = 0;
}

void LevelMeter::setEventTag(uint32_t tag) {
  if (requireMessageThread())
    m_eventTag = tag;
}

void LevelMeter::recordEvent(int channel, float peakDb) {
  LevelEvent ev;
  ev.timestampMs = juce::Time::currentTimeMillis();
  ev.channel = channel;
  ev.peakDb = peakDb;
  ev.tag = m_eventTag;

  if (!m_history.empty()) {
    m_history[static_cast<size_t>(m_historyHead)] = ev;
    m_historyHead = (m_historyHead + 1) % static_cast<int>(m_history.size());
    if (m_historyCount < static_cast<int>(m_history.size()))
      ++m_historyCount;
  }

  if (onLevelEvent) {
    juce::Component::SafePointer<LevelMeter> safeThis(this);
    onLevelEvent(ev);
    juce::ignoreUnused(safeThis);
  }
}

//==============================================================================
void LevelMeter::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.fillAll(m_style.backgroundColor);

  // Calculate meter layout
  float scaleWidth = m_style.showScale ? 30.0f : 0.0f;

  juce::Rectangle<float> meterArea, scaleArea;

  if (m_isVertical) {
    scaleArea = bounds.removeFromLeft(scaleWidth);
    meterArea = bounds;
  } else {
    scaleArea = bounds.removeFromBottom(scaleWidth);
    meterArea = bounds;
  }

  // Draw scale
  if (m_style.showScale) {
    drawScale(g, scaleArea);
  }

  // Calculate meter bounds for each channel
  float totalMeterWidth =
      m_numChannels * m_style.meterWidth + (m_numChannels - 1) * m_style.meterGap;

  float startOffset;
  if (m_isVertical) {
    startOffset = meterArea.getX() + (meterArea.getWidth() - totalMeterWidth) * 0.5f;
  } else {
    startOffset = meterArea.getY() + (meterArea.getHeight() - totalMeterWidth) * 0.5f;
  }

  for (int ch = 0; ch < m_numChannels; ++ch) {
    juce::Rectangle<float> meterBounds;

    if (m_isVertical) {
      float x = startOffset + ch * (m_style.meterWidth + m_style.meterGap);
      meterBounds =
          juce::Rectangle<float>(x, meterArea.getY(), m_style.meterWidth, meterArea.getHeight());
    } else {
      float y = startOffset + ch * (m_style.meterWidth + m_style.meterGap);
      meterBounds =
          juce::Rectangle<float>(meterArea.getX(), y, meterArea.getWidth(), m_style.meterWidth);
    }

    drawMeter(g, meterBounds, ch);
  }
}

void LevelMeter::resized() {
  // No child components to layout
}

void LevelMeter::mouseDown(const juce::MouseEvent& e) {
  juce::ignoreUnused(e);
  if (requireMessageThread())
    clearClip();
}

//==============================================================================
void LevelMeter::timerCallback() {
  if (!requireMessageThread())
    return;
  if (!isShowing()) {
    updateTimerState();
    return;
  }

  juce::Component::SafePointer<LevelMeter> safeThis(this);
  updateMeter();
  if (safeThis == nullptr)
    return;
  repaint();
}

void LevelMeter::updateTimerState() {
  if (!requireMessageThread())
    return;
  if (isShowing())
    startTimerHz(60);
  else
    stopTimer();
}

void LevelMeter::updateMeter() {
  const int64_t currentTime = juce::Time::currentTimeMillis();

  for (int ch = 0; ch < m_numChannels; ++ch) {
    const auto pair =
        unpackLevelPair(m_inputLevelPairs[static_cast<size_t>(ch)].load(std::memory_order_relaxed));
    const float inputNorm = linearToNormalized(pair.peak);
    const float rmsNorm = linearToNormalized(pair.rms);
    m_displayRmsLevels[static_cast<size_t>(ch)] = rmsNorm;
    float& displayLevel = m_displayLevels[static_cast<size_t>(ch)];

    if (m_ballistics == MeterBallistics::PeakRms) {
      // The segmented fill is intentionally unsmoothed. The separate needle
      // supplies the only release motion in this presentation.
      displayLevel = inputNorm;

      float& needleDb = m_peakRmsNeedleDb[static_cast<size_t>(ch)];
      int64_t& needleTime = m_peakRmsNeedleTimes[static_cast<size_t>(ch)];
      const int64_t elapsed = currentTime - needleTime;
      if (needleTime == 0 || elapsed <= 0 || elapsed >= 250) {
        // A first timer tick or a suspended UI clock only seeds timing. It
        // must not synthesize a release across an arbitrary gap.
        needleTime = currentTime;
      } else {
        const float peakDb = normalizedToDB(inputNorm);
        const float rmsDb = normalizedToDB(rmsNorm);
        if (peakDb > needleDb) {
          needleDb = peakDb;
        } else {
          needleDb = juce::jmax(rmsDb, needleDb - m_style.peakRmsNeedleReleaseDbPerSecond *
                                                      static_cast<float>(elapsed) / 1000.0f);
        }
        needleTime = currentTime;
      }
      needleDb = juce::jlimit(m_minDB, m_maxDB, std::isfinite(needleDb) ? needleDb : m_minDB);
    } else {
      const float coeff = inputNorm > displayLevel ? m_attackCoeff : m_releaseCoeff;
      displayLevel = juce::jlimit(0.0f, 1.0f, displayLevel + (inputNorm - displayLevel) * coeff);
    }

    if (displayLevel >= m_peakHolds[static_cast<size_t>(ch)]) {
      m_peakHolds[static_cast<size_t>(ch)] = displayLevel;
      m_peakHoldTimes[static_cast<size_t>(ch)] = currentTime;
    } else if (currentTime - m_peakHoldTimes[static_cast<size_t>(ch)] > m_peakHoldTimeMs) {
      m_peakHolds[static_cast<size_t>(ch)] = displayLevel;
    }

    const float clipThreshNorm = dbToNormalized(m_style.clipThreshold);
    if (inputNorm >= clipThreshNorm && !m_clipped[static_cast<size_t>(ch)]) {
      m_clipped[static_cast<size_t>(ch)] = true;
      if (onClip) {
        juce::Component::SafePointer<LevelMeter> safeThis(this);
        onClip(ch);
        if (safeThis == nullptr)
          return;
      }
      {
        juce::Component::SafePointer<LevelMeter> safeThis(this);
        recordEvent(ch, normalizedToDB(inputNorm));
        if (safeThis == nullptr)
          return;
      }
    }
  }
}

float LevelMeter::linearToNormalized(float linear) const {
  if (!std::isfinite(linear) || linear <= 0.0f)
    return 0.0f;
  return dbToNormalized(20.0f * std::log10(linear));
}

float LevelMeter::dbToNormalized(float dB) const {
  if (!std::isfinite(dB) || !std::isfinite(m_minDB) || !std::isfinite(m_maxDB) ||
      m_maxDB <= m_minDB)
    return 0.0f;
  return juce::jlimit(0.0f, 1.0f, (dB - m_minDB) / (m_maxDB - m_minDB));
}

float LevelMeter::normalizedToDB(float normalized) const {
  const float safe = std::isfinite(normalized) ? juce::jlimit(0.0f, 1.0f, normalized) : 0.0f;
  return m_minDB + safe * (m_maxDB - m_minDB);
}

juce::Colour LevelMeter::getColorForLevel(float normalized) const {
  const float dB = normalizedToDB(normalized);

  if (dB >= m_style.clipThreshold)
    return m_style.clipColor;
  if (dB >= m_style.redThreshold)
    return m_style.meterColorHigh;
  if (dB >= m_style.yellowThreshold) {
    const float denominator = juce::jmax(0.001f, m_style.redThreshold - m_style.yellowThreshold);
    const float t = juce::jlimit(0.0f, 1.0f, (dB - m_style.yellowThreshold) / denominator);
    return m_style.meterColorMid.interpolatedWith(m_style.meterColorHigh, t);
  }

  const float denominator = juce::jmax(0.001f, m_style.yellowThreshold - m_minDB);
  const float t = juce::jlimit(0.0f, 1.0f, (dB - m_minDB) / denominator);
  return m_style.meterColorLow.interpolatedWith(m_style.meterColorMid, t * t);
}

juce::Colour LevelMeter::getSegmentColorForLevel(float dB, int greenIndex,
                                                 int greenSegmentCount) const {
  if (dB >= m_style.clipThreshold)
    return m_style.clipColor;
  if (dB >= m_style.redThreshold)
    return m_style.meterColorHigh;
  if (dB >= m_style.yellowThreshold) {
    const float denominator = juce::jmax(0.001f, m_style.redThreshold - m_style.yellowThreshold);
    const float t = juce::jlimit(0.0f, 1.0f, (dB - m_style.yellowThreshold) / denominator);
    return m_style.meterColorMid.interpolatedWith(m_style.meterColorHigh, t);
  }

  const int transitionCount =
      juce::jmin(m_style.greenToYellowTransitionSegments, greenSegmentCount);
  if (transitionCount > 0 && greenIndex >= greenSegmentCount - transitionCount) {
    const int transitionIndex = greenIndex - (greenSegmentCount - transitionCount);
    const float t = static_cast<float>(transitionIndex + 1) / static_cast<float>(transitionCount);
    return m_style.meterColorLow.interpolatedWith(m_style.meterColorMid, t);
  }
  return m_style.meterColorLow;
}

void LevelMeter::drawSegmentedMeter(juce::Graphics& g, juce::Rectangle<float> trackBounds,
                                    int channel) {
  const float peakLevel = juce::jlimit(
      0.0f, 1.0f, std::isfinite(m_displayLevels[channel]) ? m_displayLevels[channel] : 0.0f);
  const float rmsLevel = juce::jlimit(
      0.0f, 1.0f, std::isfinite(m_displayRmsLevels[channel]) ? m_displayRmsLevels[channel] : 0.0f);
  const bool clipped = m_clipped[channel];

  g.setColour(m_style.backgroundColor.brighter(0.1f));
  g.fillRoundedRectangle(trackBounds, m_style.cornerRadius);

  const auto contentBounds = trackBounds.reduced(1.0f);
  const float signalAxisLength =
      m_isVertical ? contentBounds.getHeight() : contentBounds.getWidth();
  const float displayScale =
      std::isfinite(getDesktopScaleFactor()) && getDesktopScaleFactor() > 0.0f
          ? getDesktopScaleFactor()
          : 1.0f;
  const auto layout = calculateSegmentLayout(signalAxisLength, m_style.segmentLength,
                                             m_style.segmentGap, displayScale);
  const float peakDb = normalizedToDB(peakLevel);
  const float rmsDb = normalizedToDB(rmsLevel);
  const auto segmentDb = [&](int index) {
    const float offset =
        layout.leadingInset + static_cast<float>(index) * (layout.bodyLength + layout.gap);
    const float normalized =
        signalAxisLength > 0.0f
            ? juce::jlimit(0.0f, 1.0f, (offset + layout.bodyLength * 0.5f) / signalAxisLength)
            : 0.0f;
    return normalizedToDB(normalized);
  };

  int greenSegmentCount = 0;
  for (int index = 0; index < layout.count; ++index) {
    if (segmentDb(index) < m_style.yellowThreshold)
      ++greenSegmentCount;
  }

  const auto snapToPixel = [displayScale](float value) {
    return std::round(value * displayScale) / displayScale;
  };
  bool hasLitSegment = false;
  float topLitDb = m_minDB;
  int greenIndex = 0;
  for (int index = 0; index < layout.count; ++index) {
    const float dB = segmentDb(index);
    const bool isGreen = dB < m_style.yellowThreshold;
    const int colorGreenIndex = greenIndex;
    if (isGreen)
      ++greenIndex;
    if (dB > peakDb)
      continue;

    const float offset =
        layout.leadingInset + static_cast<float>(index) * (layout.bodyLength + layout.gap);
    g.setColour(getSegmentColorForLevel(dB, colorGreenIndex, greenSegmentCount));
    if (m_isVertical) {
      const float bottom = snapToPixel(contentBounds.getBottom() - offset);
      const float top = snapToPixel(bottom - layout.bodyLength);
      g.fillRect(contentBounds.getX(), juce::jmin(top, bottom), contentBounds.getWidth(),
                 std::abs(bottom - top));
    } else {
      const float left = snapToPixel(contentBounds.getX() + offset);
      const float right = snapToPixel(left + layout.bodyLength);
      g.fillRect(juce::jmin(left, right), contentBounds.getY(), std::abs(right - left),
                 contentBounds.getHeight());
    }
    hasLitSegment = true;
    topLitDb = dB;
  }

  const bool isSilent = peakDb <= m_minDB && rmsDb <= m_minDB;
  if (m_style.showPeakRmsNeedle && !isSilent) {
    const float rawNeedleDb = m_peakRmsNeedleDb[static_cast<size_t>(channel)];
    const float drawnDb = hasLitSegment ? juce::jmax(rawNeedleDb, topLitDb) : rawNeedleDb;
    const float needleNormalized = dbToNormalized(drawnDb);
    g.setColour(m_style.peakHoldColor);
    if (m_isVertical) {
      const float center =
          snapToPixel(contentBounds.getBottom() - contentBounds.getHeight() * needleNormalized);
      g.fillRect(contentBounds.getX(), center - m_style.peakRmsNeedleWidth * 0.5f,
                 contentBounds.getWidth(), m_style.peakRmsNeedleWidth);
    } else {
      const float center =
          snapToPixel(contentBounds.getX() + contentBounds.getWidth() * needleNormalized);
      g.fillRect(center - m_style.peakRmsNeedleWidth * 0.5f, contentBounds.getY(),
                 m_style.peakRmsNeedleWidth, contentBounds.getHeight());
    }
  }

  if (m_style.showClipIndicator && clipped) {
    const auto clipBounds =
        m_isVertical ? trackBounds.removeFromTop(6.0f) : trackBounds.removeFromRight(6.0f);
    g.setColour(m_style.clipColor);
    g.fillRoundedRectangle(clipBounds, m_style.cornerRadius);
  }
}

void LevelMeter::drawMeter(juce::Graphics& g, juce::Rectangle<float> trackBounds, int channel) {
  const float displayLevel = juce::jlimit(
      0.0f, 1.0f, std::isfinite(m_displayLevels[channel]) ? m_displayLevels[channel] : 0.0f);
  const float peakHold =
      juce::jlimit(0.0f, 1.0f, std::isfinite(m_peakHolds[channel]) ? m_peakHolds[channel] : 0.0f);
  const bool clipped = m_clipped[channel];
  if (m_style.fillStyle == MeterFillStyle::Segmented) {
    drawSegmentedMeter(g, trackBounds, channel);
    return;
  }

  g.setColour(m_style.backgroundColor.brighter(0.1f));
  g.fillRoundedRectangle(trackBounds, m_style.cornerRadius);

  juce::Rectangle<float> fillBounds;
  if (m_isVertical) {
    const float fillHeight = trackBounds.getHeight() * displayLevel;
    fillBounds = trackBounds.withTop(trackBounds.getBottom() - fillHeight);
  } else {
    const float fillWidth = trackBounds.getWidth() * displayLevel;
    fillBounds = trackBounds.withRight(trackBounds.getX() + fillWidth);
  }

  // Thresholds are defined in the same normalized signal space as displayLevel.
  // Vertical meters increase upward; horizontal meters increase rightward.
  const float yellowNorm = dbToNormalized(m_style.yellowThreshold);
  const float redNorm = dbToNormalized(m_style.redThreshold);
  const float clipNorm = dbToNormalized(m_style.clipThreshold);

  juce::ColourGradient gradient;
  if (m_isVertical) {
    gradient = juce::ColourGradient::vertical(m_style.clipColor, trackBounds.getY(),
                                              m_style.meterColorLow, trackBounds.getBottom());
    gradient.addColour(0.0, m_style.clipColor);
    gradient.addColour(1.0 - clipNorm, m_style.clipColor);
    gradient.addColour(1.0 - redNorm, m_style.meterColorHigh);
    gradient.addColour(1.0 - yellowNorm, m_style.meterColorMid);
    gradient.addColour(1.0, m_style.meterColorLow);
  } else {
    gradient = juce::ColourGradient::horizontal(m_style.meterColorLow, trackBounds.getX(),
                                                m_style.clipColor, trackBounds.getRight());
    gradient.addColour(0.0, m_style.meterColorLow);
    gradient.addColour(yellowNorm, m_style.meterColorMid);
    gradient.addColour(redNorm, m_style.meterColorHigh);
    gradient.addColour(clipNorm, m_style.clipColor);
    gradient.addColour(1.0, m_style.clipColor);
  }
  g.setGradientFill(gradient);
  g.fillRoundedRectangle(fillBounds, m_style.cornerRadius);

  if (m_style.showPeakHold && peakHold > 0.01f) {
    g.setColour(m_style.peakHoldColor);
    if (m_isVertical) {
      const float peakY = trackBounds.getBottom() - trackBounds.getHeight() * peakHold;
      g.fillRect(trackBounds.getX(), peakY - m_style.peakHoldWidth * 0.5f, trackBounds.getWidth(),
                 m_style.peakHoldWidth);
    } else {
      const float peakX = trackBounds.getX() + trackBounds.getWidth() * peakHold;
      g.fillRect(peakX - m_style.peakHoldWidth * 0.5f, trackBounds.getY(), m_style.peakHoldWidth,
                 trackBounds.getHeight());
    }
  }

  if (m_style.showClipIndicator && clipped) {
    const auto clipBounds =
        m_isVertical ? trackBounds.removeFromTop(6.0f) : trackBounds.removeFromRight(6.0f);
    g.setColour(m_style.clipColor);
    g.fillRoundedRectangle(clipBounds, m_style.cornerRadius);
  }
}

void LevelMeter::drawScale(juce::Graphics& g, juce::Rectangle<float> bounds) {
  g.setColour(m_style.textColor);
  g.setFont(9.0f);

  // Draw dB markers
  std::vector<float> markers = {0.0f, -3.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f};

  for (float dB : markers) {
    if (dB < m_minDB || dB > m_maxDB)
      continue;

    float normalized = dbToNormalized(dB);

    juce::String text = juce::String(static_cast<int>(dB));
    if (dB >= 0)
      text = "0";

    if (m_isVertical) {
      float y = bounds.getBottom() - bounds.getHeight() * normalized;

      // Draw tick
      if (m_style.showTicks) {
        g.setColour(m_style.tickColor);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getRight() - 5, bounds.getRight());
      }

      // Draw text
      g.setColour(m_style.textColor);
      g.drawText(text, bounds.getX(), y - 6, bounds.getWidth() - 6, 12,
                 juce::Justification::centredRight, false);
    } else {
      float x = bounds.getX() + bounds.getWidth() * normalized;

      // Draw tick
      if (m_style.showTicks) {
        g.setColour(m_style.tickColor);
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getY() + 5);
      }

      // Draw text
      g.setColour(m_style.textColor);
      g.drawText(text, x - 15, bounds.getY() + 6, 30, 12, juce::Justification::centred, false);
    }
  }
}

} // namespace shmui
