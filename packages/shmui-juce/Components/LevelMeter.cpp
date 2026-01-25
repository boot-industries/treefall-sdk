/*
  ==============================================================================

    LevelMeter.cpp
    Created: shmui Component Library

    Level meter implementation.

  ==============================================================================
*/

#include "LevelMeter.h"
#include <cmath>

namespace shmui {

//==============================================================================
LevelMeter::LevelMeter() : LevelMeter(1) {}

LevelMeter::LevelMeter(int numChannels)
    : m_numChannels(juce::jlimit(1, MAX_CHANNELS, numChannels)) {
  // Initialize atomics
  for (int i = 0; i < MAX_CHANNELS; ++i) {
    m_inputLevels[i].store(0.0f);
    m_displayLevels[i] = 0.0f;
    m_peakHolds[i] = 0.0f;
    m_peakHoldTimes[i] = 0;
    m_clipped[i] = false;
  }

  setBallistics(MeterBallistics::Peak);
  startTimerHz(60);
}

LevelMeter::~LevelMeter() {
  stopTimer();
}

//==============================================================================
void LevelMeter::setLevel(int channel, float level) {
  if (channel >= 0 && channel < m_numChannels) {
    m_inputLevels[channel].store(level);
  }
}

void LevelMeter::setLevelDB(int channel, float dB) {
  // Convert dB to linear
  float linear = std::pow(10.0f, dB / 20.0f);
  setLevel(channel, linear);
}

void LevelMeter::setLevels(const std::vector<float>& levels) {
  int count = juce::jmin(static_cast<int>(levels.size()), m_numChannels);
  for (int i = 0; i < count; ++i) {
    m_inputLevels[i].store(levels[i]);
  }
}

void LevelMeter::reset() {
  for (int i = 0; i < m_numChannels; ++i) {
    m_inputLevels[i].store(0.0f);
    m_displayLevels[i] = 0.0f;
    m_peakHolds[i] = 0.0f;
    m_peakHoldTimes[i] = 0;
    m_clipped[i] = false;
  }
  repaint();
}

//==============================================================================
void LevelMeter::setNumChannels(int numChannels) {
  m_numChannels = juce::jlimit(1, MAX_CHANNELS, numChannels);
  reset();
}

void LevelMeter::setBallistics(MeterBallistics ballistics) {
  m_ballistics = ballistics;

  // Set coefficients based on ballistics type
  // Coefficients calculated for 60Hz update rate
  switch (ballistics) {
  case MeterBallistics::Peak:
    m_attackCoeff = 1.0f;   // Instant attack
    m_releaseCoeff = 0.05f; // ~200ms release
    break;

  case MeterBallistics::VU:
    m_attackCoeff = 0.3f;  // ~300ms integration
    m_releaseCoeff = 0.3f; // Same as attack (VU is symmetrical)
    break;

  case MeterBallistics::PPM:
    m_attackCoeff = 0.8f;   // Fast attack (~10ms)
    m_releaseCoeff = 0.02f; // Slow decay (~1.5s)
    break;
  }
}

void LevelMeter::setVertical(bool vertical) {
  if (m_isVertical != vertical) {
    m_isVertical = vertical;
    repaint();
  }
}

void LevelMeter::setPeakHoldTime(int milliseconds) {
  m_peakHoldTimeMs = juce::jmax(0, milliseconds);
}

void LevelMeter::setDBRange(float minDB, float maxDB) {
  m_minDB = minDB;
  m_maxDB = maxDB;
  repaint();
}

void LevelMeter::setStyle(const LevelMeterStyle& style) {
  m_style = style;
  repaint();
}

//==============================================================================
void LevelMeter::clearClip() {
  for (int i = 0; i < m_numChannels; ++i) {
    m_clipped[i] = false;
  }
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

  // Click to clear clip indicators
  clearClip();
}

//==============================================================================
void LevelMeter::timerCallback() {
  updateMeter();
  repaint();
}

void LevelMeter::updateMeter() {
  const int64_t currentTime = juce::Time::currentTimeMillis();

  for (int ch = 0; ch < m_numChannels; ++ch) {
    float inputLevel = m_inputLevels[ch].load();

    // Convert to normalized
    float inputNorm = linearToNormalized(inputLevel);

    // Apply ballistics
    float& displayLevel = m_displayLevels[ch];

    if (inputNorm > displayLevel) {
      // Attack
      displayLevel = displayLevel + (inputNorm - displayLevel) * m_attackCoeff;
    } else {
      // Release
      displayLevel = displayLevel + (inputNorm - displayLevel) * m_releaseCoeff;
    }

    // Clamp
    displayLevel = juce::jlimit(0.0f, 1.0f, displayLevel);

    // Update peak hold
    if (displayLevel >= m_peakHolds[ch]) {
      m_peakHolds[ch] = displayLevel;
      m_peakHoldTimes[ch] = currentTime;
    } else if (currentTime - m_peakHoldTimes[ch] > m_peakHoldTimeMs) {
      // Peak hold expired, start decay
      m_peakHolds[ch] = displayLevel;
    }

    // Check for clip
    float clipThreshNorm = dbToNormalized(m_style.clipThreshold);
    if (inputNorm >= clipThreshNorm && !m_clipped[ch]) {
      m_clipped[ch] = true;
      if (onClip)
        onClip(ch);
    }
  }
}

float LevelMeter::linearToNormalized(float linear) const {
  if (linear <= 0.0f)
    return 0.0f;

  // Convert to dB
  float dB = 20.0f * std::log10(linear);
  return dbToNormalized(dB);
}

float LevelMeter::dbToNormalized(float dB) const {
  return juce::jlimit(0.0f, 1.0f, (dB - m_minDB) / (m_maxDB - m_minDB));
}

float LevelMeter::normalizedToDB(float normalized) const {
  return m_minDB + normalized * (m_maxDB - m_minDB);
}

juce::Colour LevelMeter::getColorForLevel(float normalized) const {
  float dB = normalizedToDB(normalized);

  if (dB >= m_style.redThreshold) {
    return m_style.meterColorHigh;
  } else if (dB >= m_style.yellowThreshold) {
    // Interpolate between yellow and red
    float t = (dB - m_style.yellowThreshold) / (m_style.redThreshold - m_style.yellowThreshold);
    return m_style.meterColorMid.interpolatedWith(m_style.meterColorHigh, t);
  } else {
    // Interpolate between green and yellow
    float lowDB = m_minDB;
    float t = (dB - lowDB) / (m_style.yellowThreshold - lowDB);
    return m_style.meterColorLow.interpolatedWith(m_style.meterColorMid,
                                                  t * t); // Quadratic for smoother green
  }
}

void LevelMeter::drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, int channel) {
  float displayLevel = m_displayLevels[channel];
  float peakHold = m_peakHolds[channel];
  bool clipped = m_clipped[channel];

  // Draw meter track background
  g.setColour(m_style.backgroundColor.brighter(0.1f));
  g.fillRoundedRectangle(bounds, m_style.cornerRadius);

  // Draw level fill with gradient
  juce::Rectangle<float> fillBounds;

  if (m_isVertical) {
    float fillHeight = bounds.getHeight() * displayLevel;
    fillBounds = bounds.removeFromBottom(fillHeight);
  } else {
    float fillWidth = bounds.getWidth() * displayLevel;
    fillBounds = bounds.removeFromLeft(fillWidth);
  }

  // Create gradient for level
  juce::ColourGradient gradient;
  if (m_isVertical) {
    gradient = juce::ColourGradient::vertical(m_style.meterColorHigh, bounds.getY(),
                                              m_style.meterColorLow, bounds.getBottom());
  } else {
    gradient = juce::ColourGradient::horizontal(m_style.meterColorLow, bounds.getX(),
                                                m_style.meterColorHigh, bounds.getRight());
  }

  // Add color stops
  float yellowNorm = dbToNormalized(m_style.yellowThreshold);
  float redNorm = dbToNormalized(m_style.redThreshold);

  gradient.addColour(0.0, m_style.meterColorLow);
  gradient.addColour(yellowNorm, m_style.meterColorMid);
  gradient.addColour(redNorm, m_style.meterColorHigh);

  g.setGradientFill(gradient);
  g.fillRoundedRectangle(fillBounds, m_style.cornerRadius);

  // Draw peak hold indicator
  if (m_style.showPeakHold && peakHold > 0.01f) {
    g.setColour(m_style.peakHoldColor);

    if (m_isVertical) {
      float peakY = bounds.getBottom() - bounds.getHeight() * peakHold;
      g.fillRect(bounds.getX(), peakY - m_style.peakHoldWidth * 0.5f, bounds.getWidth(),
                 m_style.peakHoldWidth);
    } else {
      float peakX = bounds.getX() + bounds.getWidth() * peakHold;
      g.fillRect(peakX - m_style.peakHoldWidth * 0.5f, bounds.getY(), m_style.peakHoldWidth,
                 bounds.getHeight());
    }
  }

  // Draw clip indicator
  if (m_style.showClipIndicator && clipped) {
    juce::Rectangle<float> clipBounds;

    if (m_isVertical) {
      clipBounds = bounds.removeFromTop(6.0f);
    } else {
      clipBounds = bounds.removeFromRight(6.0f);
    }

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
