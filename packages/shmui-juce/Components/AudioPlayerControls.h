/*
  ==============================================================================

    AudioPlayerControls.h
    Shmui - Audio/AI-focused UI component library

    Transport controls for audio playback including play/pause,
    time display, and speed control.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

namespace shmui {

//==============================================================================
/**
    Audio player controls providing transport functionality.

    Features:
    - Play/pause toggle button
    - Current time display
    - Duration display
    - Playback speed control (0.25x to 2.0x)
    - Seek callback support

    Visual Design:
    - Matches React implementation styling
    - Supports dark/light theme
    - Customizable colors and sizes
*/
class AudioPlayerControls : public juce::Component, public juce::Timer {
public:
  //==========================================================================
  /** Listener interface for transport events. */
  class Listener {
  public:
    virtual ~Listener() = default;

    /** Called when play/pause state changes. */
    virtual void playStateChanged(bool isPlaying) {}

    /** Called when playback speed changes. */
    virtual void playbackRateChanged(double rate) {}

    /** Called when user seeks to a new position. */
    virtual void seekRequested(double timeInSeconds) {}
  };

  //==========================================================================
  /** Creates the audio player controls. */
  AudioPlayerControls();

  /** Destructor. */
  ~AudioPlayerControls() override;

  //==========================================================================
  /** Sets the current playback time in seconds. */
  void setCurrentTime(double timeInSeconds);

  /** Sets the total duration in seconds. */
  void setDuration(double durationInSeconds);

  /** Sets whether audio is currently playing. */
  void setPlaying(bool shouldBePlaying);

  /** Gets whether audio is currently playing. */
  bool isPlaying() const {
    return playing;
  }

  /** Sets the playback rate (0.25 to 2.0). */
  void setPlaybackRate(double rate);

  /** Gets the current playback rate. */
  double getPlaybackRate() const {
    return playbackRate;
  }

  /** Sets whether the player is in a buffering state. */
  void setBuffering(bool isBuffering);

  //==========================================================================
  /** Adds a listener for transport events. */
  void addListener(Listener* listener);

  /** Removes a listener. */
  void removeListener(Listener* listener);

  //==========================================================================
  /** Style configuration for the controls. */
  struct Style {
    juce::Colour backgroundColor{0x00000000};
    juce::Colour buttonColor{0xff3b82f6};      // Primary blue
    juce::Colour buttonHoverColor{0xff2563eb}; // Darker blue
    juce::Colour textColor{0xffa1a1aa};        // Muted foreground
    juce::Colour iconColor{0xffffffff};        // White icons
    float buttonSize{40.0f};
    float fontSize{14.0f};
    float cornerRadius{8.0f};
    float padding{8.0f};
  };

  /** Sets the visual style. */
  void setStyle(const Style& newStyle);

  /** Gets the current style. */
  const Style& getStyle() const {
    return style;
  }

  //==========================================================================
  /** Available playback speeds. */
  static constexpr double playbackSpeeds[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
  static constexpr int numPlaybackSpeeds = 8;

  //==========================================================================
  // Component overrides
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& event) override;
  void mouseUp(const juce::MouseEvent& event) override;
  void mouseMove(const juce::MouseEvent& event) override;
  void mouseExit(const juce::MouseEvent& event) override;

  // Timer callback for buffering animation
  void timerCallback() override;

private:
  //==========================================================================
  /** Formats time in seconds to mm:ss or hh:mm:ss string. */
  static juce::String formatTime(double seconds);

  /** Gets the bounds of the play button. */
  juce::Rectangle<float> getPlayButtonBounds() const;

  /** Gets the bounds of the time display. */
  juce::Rectangle<float> getTimeDisplayBounds() const;

  /** Gets the bounds of the speed button. */
  juce::Rectangle<float> getSpeedButtonBounds() const;

  /** Draws a play icon at the given position. */
  void drawPlayIcon(juce::Graphics& g, juce::Rectangle<float> bounds);

  /** Draws a pause icon at the given position. */
  void drawPauseIcon(juce::Graphics& g, juce::Rectangle<float> bounds);

  /** Draws a spinner for buffering state. */
  void drawSpinner(juce::Graphics& g, juce::Rectangle<float> bounds);

  /** Shows the speed selection popup menu. */
  void showSpeedMenu();

  //==========================================================================
  double currentTime{0.0};
  double duration{0.0};
  double playbackRate{1.0};
  bool playing{false};
  bool buffering{false};
  float spinnerAngle{0.0f};

  // Mouse interaction state
  bool playButtonHovered{false};
  bool speedButtonHovered{false};
  bool playButtonPressed{false};

  Style style;

  juce::ListenerList<Listener> listeners;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayerControls)
};

} // namespace shmui
