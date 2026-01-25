/*
  ==============================================================================

    TransportBar.h
    Created: shmui Component Library

    Full transport control strip with transport buttons, time display,
    and additional controls.

    Features:
    - Play/Pause/Stop buttons with TransportButton
    - Time display (current position / duration)
    - Tempo/BPM display (optional)
    - Loop toggle
    - Record arm (optional)
    - Panic button (emergency stop)

  ==============================================================================
*/

#pragma once

#include "../Controls/ToggleButton.h"
#include "../Controls/TransportButton.h"
#include "../Icons/Icons.h"
#include <JuceHeader.h>

namespace shmui {

//==============================================================================
/**
 * @brief Style configuration for TransportBar.
 */
struct TransportBarStyle {
  // Colors
  juce::Colour backgroundColor = juce::Colour(0xFF1A1A1A);
  juce::Colour textColor = juce::Colours::white;
  juce::Colour dimTextColor = juce::Colour(0x80FFFFFF);
  juce::Colour separatorColor = juce::Colour(0x30FFFFFF);

  // Dimensions
  float height = 48.0f;
  float buttonSize = 36.0f;
  float buttonSpacing = 4.0f;
  float sectionSpacing = 16.0f;

  // Features
  bool showLoop = true;
  bool showRecord = false;
  bool showTempo = false;
  bool showPanic = true;
};

//==============================================================================
/**
 * @brief Transport time display format.
 */
enum class TimeDisplayFormat {
  MinutesSeconds, ///< MM:SS.mmm
  Bars,           ///< Bar.Beat.Tick
  Samples,        ///< Sample count
  Timecode        ///< HH:MM:SS:FF
};

//==============================================================================
/**
 * @brief Full transport control strip component.
 *
 * Provides a complete transport control interface:
 * - Play/Pause toggle button
 * - Stop button
 * - Record button (optional)
 * - Loop toggle (optional)
 * - Time position display
 * - Duration display
 * - Tempo/BPM display (optional)
 * - Panic button for emergency stop
 *
 * Designed for DAWs, audio players, and media applications.
 */
class TransportBar : public juce::Component {
public:
  //==============================================================================
  TransportBar();
  ~TransportBar() override = default;

  //==============================================================================
  /// @name Transport State
  /// @{

  /** Set playing state. */
  void setPlaying(bool playing);
  bool isPlaying() const {
    return m_isPlaying;
  }

  /** Set recording state. */
  void setRecording(bool recording);
  bool isRecording() const {
    return m_isRecording;
  }

  /** Set looping state. */
  void setLooping(bool looping);
  bool isLooping() const {
    return m_isLooping;
  }

  /// @}

  //==============================================================================
  /// @name Time Display
  /// @{

  /** Set current position in seconds. */
  void setPositionSeconds(double seconds);

  /** Set current position in samples. */
  void setPositionSamples(int64_t samples, int sampleRate);

  /** Set total duration in seconds. */
  void setDurationSeconds(double seconds);

  /** Set total duration in samples. */
  void setDurationSamples(int64_t samples, int sampleRate);

  /** Set time display format. */
  void setTimeFormat(TimeDisplayFormat format);

  /** Get current position in seconds. */
  double getPositionSeconds() const {
    return m_positionSeconds;
  }

  /** Get total duration in seconds. */
  double getDurationSeconds() const {
    return m_durationSeconds;
  }

  /// @}

  //==============================================================================
  /// @name Tempo (Optional)
  /// @{

  /** Set tempo in BPM. */
  void setTempo(double bpm);

  /** Get tempo in BPM. */
  double getTempo() const {
    return m_tempoBPM;
  }

  /** Set time signature. */
  void setTimeSignature(int numerator, int denominator);

  /// @}

  //==============================================================================
  /// @name Style
  /// @{

  /** Set visual style. */
  void setStyle(const TransportBarStyle& style);

  /** Get current style. */
  const TransportBarStyle& getStyle() const {
    return m_style;
  }

  /// @}

  //==============================================================================
  /// @name Callbacks
  /// @{

  /** Callback when play/pause is clicked. */
  std::function<void()> onPlayPause;

  /** Callback when stop is clicked. */
  std::function<void()> onStop;

  /** Callback when record is clicked. */
  std::function<void()> onRecord;

  /** Callback when loop toggle changes. */
  std::function<void(bool enabled)> onLoopToggle;

  /** Callback when panic is clicked (emergency stop). */
  std::function<void()> onPanic;

  /// @}

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  void setupButtons();
  juce::String formatTime(double seconds) const;
  juce::String formatTimecode(double seconds) const;
  juce::String formatBars(double seconds) const;
  juce::String formatSamples(int64_t samples) const;

  //==============================================================================
  TransportBarStyle m_style;
  TimeDisplayFormat m_timeFormat = TimeDisplayFormat::MinutesSeconds;

  // Transport state
  bool m_isPlaying = false;
  bool m_isRecording = false;
  bool m_isLooping = false;

  // Time
  double m_positionSeconds = 0.0;
  double m_durationSeconds = 0.0;
  int64_t m_positionSamples = 0;
  int64_t m_durationSamples = 0;
  int m_sampleRate = 48000;

  // Tempo
  double m_tempoBPM = 120.0;
  int m_timeSignatureNum = 4;
  int m_timeSignatureDenom = 4;

  // Buttons
  std::unique_ptr<TransportButton> m_playPauseButton;
  std::unique_ptr<TransportButton> m_stopButton;
  std::unique_ptr<TransportButton> m_recordButton;
  std::unique_ptr<ToggleButton> m_loopButton;
  std::unique_ptr<TransportButton> m_panicButton;

  // Labels
  std::unique_ptr<juce::Label> m_positionLabel;
  std::unique_ptr<juce::Label> m_durationLabel;
  std::unique_ptr<juce::Label> m_tempoLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace shmui
