/*
  ==============================================================================

    LevelMeter.h
    Created: shmui Component Library

    Professional VU/PPM level meter with peak hold indicator.

    Features:
    - Vertical or horizontal orientation
    - Peak hold indicator with configurable hold time
    - Stereo/multi-channel support
    - VU, PPM, and Peak ballistics
    - Clip indicator with latch
    - dB scale markings
    - Gradient coloring (green -> yellow -> red)

  ==============================================================================
*/

#pragma once

#include "../Utils/DesignTokens.h"
#include "../Utils/Interpolation.h"
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

namespace shmui {

//==============================================================================
/**
 * @brief Meter ballistics type.
 */
enum class MeterBallistics {
  Peak, ///< Fast response, slow release (digital peak)
  VU,   ///< VU meter ballistics (300ms integration)
  PPM   ///< PPM meter ballistics (fast attack, slow decay)
};

//==============================================================================
/**
 * @brief A logged level event (clip / peak crossing) for playout history.
 *
 * Broadcast/theatre operators need a diagnostic trail of what played and how
 * hot. The meter can keep an optional ring buffer of these and/or fire a
 * callback, so apps log history without reimplementing the meter (OCC153 G2).
 */
struct LevelEvent {
  int64_t timestampMs = 0; ///< juce::Time::currentTimeMillis() at the event
  int channel = 0;         ///< channel (or group index when used in MeterGroup)
  float peakDb = 0.0f;     ///< level in dBFS at the event
  uint32_t tag = 0;        ///< optional app payload (e.g. clip index)
};

//==============================================================================
/**
 * @brief Style configuration for LevelMeter.
 */
struct LevelMeterStyle {
  // Canonical Lab presentation roles; product-owned layout and scale policy
  // remain configurable below.
  juce::Colour backgroundColor = tokens::meter::surfaceLab();
  juce::Colour meterColorLow = tokens::meter::safe();
  juce::Colour meterColorMid = tokens::meter::caution();
  juce::Colour meterColorHigh = tokens::meter::warning();
  juce::Colour peakHoldColor = tokens::meter::peakLab();
  juce::Colour clipColor = tokens::meter::clip();
  juce::Colour textColor = tokens::lab::text().withAlpha(0.5f);
  juce::Colour tickColor = tokens::lab::text().withAlpha(0.25f);

  // Thresholds (in dB)
  float yellowThreshold = -12.0f; // Start yellow here
  float redThreshold = -3.0f;     // Start red here
  float clipThreshold = 0.0f;     // Clip indicator threshold

  // Appearance
  float meterWidth = 8.0f; // Width of each meter bar
  float meterGap = 2.0f;   // Gap between stereo meters
  float cornerRadius = 2.0f;
  bool showPeakHold = true;
  bool showClipIndicator = true;
  bool showScale = true;
  bool showTicks = true;
  float peakHoldWidth = 2.0f;
  [[nodiscard]] static LevelMeterStyle
  fromPresentation(const tokens::meter::Presentation& presentation) {
    LevelMeterStyle style;
    style.backgroundColor = presentation.background;
    style.meterColorLow = presentation.stops[0].color;
    style.meterColorMid = presentation.stops[1].color;
    style.meterColorHigh = presentation.stops[2].color;
    style.peakHoldColor = presentation.peak;
    style.clipColor = presentation.clip;
    return style;
  }
};

//==============================================================================
/**
 * @brief Professional level meter component.
 *
 * Provides accurate level metering with configurable ballistics:
 * - Peak: Fast response for digital peak detection
 * - VU: Classic VU meter ballistics (300ms integration)
 * - PPM: European PPM ballistics (fast attack, slow decay)
 *
 * Supports mono, stereo, or multi-channel operation.
 * Thread-safe level updates via atomic values.
 */
class LevelMeter : public juce::Component, private juce::Timer {
public:
  //==============================================================================
  /** Create a level meter (mono by default). */
  LevelMeter();

  /** Create a level meter with specified channel count. */
  explicit LevelMeter(int numChannels);

  ~LevelMeter() override;

  //==============================================================================
  /// @name Levels
  /// @{

  /**
   * @brief Set level for a channel (thread-safe).
   * @param channel Channel index (0-based)
   * @param level Level in linear scale (0.0 - 1.0+)
   */
  void setLevel(int channel, float level);

  /**
   * @brief Set level for a channel in dB (thread-safe).
   * @param channel Channel index
   * @param dB Level in decibels (-inf to 0+)
   */
  void setLevelDB(int channel, float dB);

  /**
   * @brief Set levels for all channels at once.
   * @param levels Array of linear levels
   */
  void setLevels(const std::vector<float>& levels);

  /**
   * @brief Reset all levels and peak holds.
   */
  void reset();

  /// @}

  //==============================================================================
  /// @name Configuration
  /// @{

  /**
   * @brief Set number of channels.
   */
  void setNumChannels(int numChannels);

  /**
   * @brief Get number of channels.
   */
  int getNumChannels() const {
    return m_numChannels;
  }

  /**
   * @brief Set meter ballistics.
   */
  void setBallistics(MeterBallistics ballistics);

  /**
   * @brief Get current ballistics.
   */
  MeterBallistics getBallistics() const {
    return m_ballistics;
  }

  /**
   * @brief Set meter orientation (true = vertical, false = horizontal).
   */
  void setVertical(bool vertical);

  /**
   * @brief Check if meter is vertical.
   */
  bool isVertical() const {
    return m_isVertical;
  }

  /**
   * @brief Set peak hold time in milliseconds.
   */
  void setPeakHoldTime(int milliseconds);

  /**
   * @brief Get peak hold time.
   */
  int getPeakHoldTime() const {
    return m_peakHoldTimeMs;
  }

  /**
   * @brief Set dB range (e.g., -60 to +6).
   */
  void setDBRange(float minDB, float maxDB);

  /**
   * @brief Get min dB range.
   */
  float getMinDB() const {
    return m_minDB;
  }

  /**
   * @brief Get max dB range.
   */
  float getMaxDB() const {
    return m_maxDB;
  }

  /// @}

  //==============================================================================
  /// @name Style
  /// @{

  /**
   * @brief Set visual style.
   */
  void setStyle(const LevelMeterStyle& style);

  /**
   * @brief Get current style.
   */
  const LevelMeterStyle& getStyle() const {
    return m_style;
  }

  /// @}

  //==============================================================================
  /// @name Clip Indicator
  /// @{

  /**
   * @brief Clear clip indicators for all channels.
   */
  void clearClip();

  /**
   * @brief Check if any channel has clipped.
   */
  bool hasClipped() const;

  /**
   * @brief Check if a specific channel has clipped.
   */
  bool hasClipped(int channel) const;

  /// @}

  //==============================================================================
  /// @name Level history / event log (optional; off by default)
  /// @{

  /**
   * @brief Enable a ring buffer of level events with the given capacity.
   *
   * Pre-allocates so appends are allocation-free. Events are recorded on the
   * message thread (from the meter's timer), so no audio-thread work is added.
   * Call once from the message thread before use.
   */
  void enableHistory(int capacity);

  /** Disable and release the history ring. */
  void disableHistory();

  /** @return number of events currently stored (0 if history disabled). */
  int getHistorySize() const;

  /**
   * @brief Get a stored event, newest-first (index 0 = most recent).
   * Returns a default LevelEvent if index is out of range.
   */
  LevelEvent getHistoryEntry(int indexFromNewest) const;

  /** Clear stored events (keeps history enabled). */
  void clearHistory();

  /**
   * @brief Optional payload tag applied to events recorded next
   * (e.g. the current clip index). Purely app-defined.
   */
  void setEventTag(uint32_t tag) {
    m_eventTag = tag;
  }

  /// @}

  //==============================================================================
  /// @name Callbacks
  /// @{

  /** Callback when clip indicator is triggered. */
  std::function<void(int channel)> onClip;

  /**
   * @brief Fired (message thread) for each recorded level event when history
   * is enabled — clip crossings today. Apps log playout history here.
   */
  std::function<void(const LevelEvent&)> onLevelEvent;

  /// @}

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;

private:
  //==============================================================================
  void timerCallback() override;
  void updateMeter();
  float linearToNormalized(float linear) const;
  float dbToNormalized(float dB) const;
  float normalizedToDB(float normalized) const;
  juce::Colour getColorForLevel(float normalized) const;
  void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, int channel);
  void drawScale(juce::Graphics& g, juce::Rectangle<float> bounds);

  //==============================================================================
  static constexpr int MAX_CHANNELS = 8;

  int m_numChannels = 1;
  bool m_isVertical = true;
  MeterBallistics m_ballistics = MeterBallistics::Peak;
  LevelMeterStyle m_style;

  // dB range
  float m_minDB = -60.0f;
  float m_maxDB = 6.0f;

  // Peak hold
  int m_peakHoldTimeMs = 2000;

  // Per-channel state (thread-safe via atomics)
  std::array<std::atomic<float>, MAX_CHANNELS> m_inputLevels{}; // Input levels (linear)
  std::array<float, MAX_CHANNELS> m_displayLevels{};            // Smoothed display levels
  std::array<float, MAX_CHANNELS> m_peakHolds{};                // Peak hold values
  std::array<int64_t, MAX_CHANNELS> m_peakHoldTimes{};          // Peak hold timestamps
  std::array<bool, MAX_CHANNELS> m_clipped{};                   // Clip indicators

  // Ballistics parameters
  float m_attackCoeff = 0.0f;
  float m_releaseCoeff = 0.0f;

  // Level-event history (message-thread only). m_historyCount saturates at
  // capacity; m_historyHead is the next write slot (ring). recordEvent()
  // appends + fires onLevelEvent.
  std::vector<LevelEvent> m_history; // sized to capacity when enabled
  int m_historyHead = 0;
  int m_historyCount = 0;
  uint32_t m_eventTag = 0;
  void recordEvent(int channel, float peakDb);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

//==============================================================================
/**
 * @brief Stereo level meter convenience class.
 */
class StereoLevelMeter : public LevelMeter {
public:
  StereoLevelMeter() : LevelMeter(2) {}

  /** Set left channel level. */
  void setLeftLevel(float level) {
    setLevel(0, level);
  }

  /** Set right channel level. */
  void setRightLevel(float level) {
    setLevel(1, level);
  }

  /** Set both channels. */
  void setStereoLevels(float left, float right) {
    setLevel(0, left);
    setLevel(1, right);
  }
};

} // namespace shmui
