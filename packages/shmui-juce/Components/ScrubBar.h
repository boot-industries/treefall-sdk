/*
  ==============================================================================

    ScrubBar.h
    Shmui - Audio/AI-focused UI component library

    Timeline scrub bar for audio playback position control.

  ==============================================================================
*/

#pragma once
#include "../Utils/DesignTokens.h"
#include "../Utils/MessageThread.h"
#include "../Utils/ShmuiTheme.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
namespace shmui {

//==============================================================================
/**
    Timeline scrub bar for controlling audio playback position.

    Features:
    - Draggable progress indicator
    - Click-to-seek functionality
    - Progress bar visualization
    - Customizable thumb and track appearance
    - Smooth dragging interaction

    Visual Design:
    - Matches React implementation styling
    - Rounded track with filled progress
    - Circular draggable thumb
*/
class ScrubBar : public juce::Component, public ThemeListener {
public:
  //==========================================================================
  /** Listener interface for scrub events. */
  class Listener {
  public:
    virtual ~Listener() = default;

    /** Called when the user starts scrubbing. */
    virtual void scrubStarted() {}

    /** Called during scrubbing with the new position (0.0 to 1.0). */
    virtual void scrubPositionChanged(double position) {}

    /** Called when the user finishes scrubbing. */
    virtual void scrubEnded() {}

    /** Called when the user seeks to a position in seconds. */
    virtual void seekRequested(double timeInSeconds) {}
  };

  //==========================================================================
  /** Creates the scrub bar. */
  ScrubBar();

  /** Destructor. */
  ~ScrubBar() override;

  //==========================================================================
  /** Sets the current position as a value from 0.0 to 1.0. */
  void setPosition(double newPosition);

  /** Gets the current position (0.0 to 1.0). */
  double getPosition() const {
    return position;
  }

  /** Sets the current time in seconds. */
  void setCurrentTime(double timeInSeconds);

  /** Gets the current time in seconds. */
  double getCurrentTime() const {
    return currentTime;
  }

  /** Sets the total duration in seconds. */
  void setDuration(double durationInSeconds);

  /** Gets the duration in seconds. */
  double getDuration() const {
    return duration;
  }

  /** Sets whether the thumb is visible. */
  void setThumbVisible(bool shouldBeVisible);

  /** Gets whether the thumb is visible. */
  bool isThumbVisible() const {
    return showThumb;
  }

  //==========================================================================
  /** Adds a listener for scrub events. */
  void addListener(Listener* listener);

  /** Removes a listener. */
  void removeListener(Listener* listener);

  //==========================================================================
  /** Style configuration for the scrub bar. */
  struct Style {
    juce::Colour trackColor = tokens::lab::surface3();   // muted track well
    juce::Colour progressColor = tokens::lab::tone();    // accent (--lab-tone)
    juce::Colour thumbColor = tokens::lab::tone();       // accent (--lab-tone)
    juce::Colour thumbBorderColor = tokens::lab::text(); // fg border
    float trackHeight{8.0f};
    float thumbSize{16.0f};
    float thumbBorderWidth{2.0f};
    float cornerRadius{4.0f};
  };

  /** Sets the visual style. */
  void setStyle(const Style& newStyle);

  /** Gets the current style. */
  const Style& getStyle() const {
    return style;
  }

  //==========================================================================
  // Component overrides
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& event) override;
  void mouseDrag(const juce::MouseEvent& event) override;
  void mouseUp(const juce::MouseEvent& event) override;
  void mouseMove(const juce::MouseEvent& event) override;
  void mouseExit(const juce::MouseEvent& event) override;
  void defaultThemeChanged(const ShmuiTheme& theme) override;

private:
  //==========================================================================
  /** Converts an x position to a normalized value (0.0 to 1.0). */
  double xToPosition(float x) const;

  /** Converts a normalized position to an x coordinate. */
  float positionToX(double pos) const;

  /** Sanitizes dimensions that affect geometry. */
  void sanitizeStyle();

  /** Gets the bounds of the track. */
  juce::Rectangle<float> getTrackBounds() const;
  /** Gets the bounds of the thumb. */
  juce::Rectangle<float> getThumbBounds() const;

  /** Updates position from mouse event and notifies listeners. */
  void updatePositionFromMouse(const juce::MouseEvent& event);

  //==========================================================================
  double position{0.0};
  double currentTime{0.0};
  double duration{0.0};
  bool showThumb{true};

  // Interaction state
  bool isDragging{false};
  bool isHovering{false};

  Style style;
  bool customStyle{false};

  std::shared_ptr<juce::ListenerList<Listener>> listeners =
      std::make_shared<juce::ListenerList<Listener>>();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScrubBar)
};

} // namespace shmui
