/*
  ==============================================================================

    RepaintThrottle.h
    Created: shmui Utilities

    Coalesce repaint() calls to a maximum frequency.

    Real-time audio UIs (meters, waveforms, grids) can be told to repaint far
    faster than a display can show — per-sample updates at 60 FPS on a large
    grid waste CPU. RepaintThrottle collapses bursts of repaint requests into at
    most one actual repaint per 1/maxHz interval, tracking a union "dirty" rect
    so only the changed area is invalidated (OCC153 G9).

    Header-only, like Interpolation.h. Message-thread only.

    Usage:
      class MyView : public juce::Component {
        RepaintThrottle throttle { *this, 60.0f };
        void onFastUpdate() { throttle.requestRepaint(); }  // coalesced
      };

  ==============================================================================
*/

#pragma once

#include "MessageThread.h"
#include <JuceHeader.h>
namespace shmui {

//==============================================================================
/**
 * @brief Rate-limits repaint() on an owning component.
 *
 * A pending request is flushed on the next timer tick (running at maxHz), so at
 * most one repaint happens per interval no matter how many requests arrive.
 * When a dirty rectangle is supplied, requests accumulate into a union and only
 * that region is repainted.
 */
class RepaintThrottle : private juce::Timer {
public:
  //==============================================================================
  /**
   * @param owner  component to repaint
   * @param maxHz  maximum repaints per second (clamped to [1, 240])
   */
  explicit RepaintThrottle(juce::Component& owner, float maxHz = 60.0f) : m_owner(owner) {
    setMaxHz(maxHz);
  }

  ~RepaintThrottle() override {
    if (requireMessageThread())
      cancel();
    else
      stopTimer();
  }

  //==============================================================================
  /** Set the maximum repaint rate (Hz). */
  void setMaxHz(float maxHz) {
    if (!requireMessageThread())
      return;

    m_maxHz = std::isfinite(maxHz) ? juce::jlimit(1.0f, 240.0f, maxHz) : 60.0f;
  }

  //==============================================================================
  /** Request a full repaint (coalesced). */
  void requestRepaint() {
    if (!requireMessageThread())
      return;

    m_fullPending = true;
    ensureRunning();
  }

  /** Request a dirty-region repaint (coalesced; regions are unioned). */
  void requestRepaint(juce::Rectangle<int> dirty) {
    if (!requireMessageThread())
      return;

    if (m_regionPending)
      m_dirty = m_dirty.getUnion(dirty);
    else {
      m_dirty = dirty;
      m_regionPending = true;
    }
    ensureRunning();
  }

  /** Immediately flush any pending repaint and stop the timer. */
  void flush() {
    if (!requireMessageThread())
      return;

    stopTimer();
    doRepaint();
  }

  /** Discard any pending request without repainting. */
  void cancel() {
    if (!requireMessageThread())
      return;

    stopTimer();
    m_fullPending = false;
    m_regionPending = false;
    m_dirty = {};
  }

private:
  //==============================================================================
  void ensureRunning() {
    if (!isTimerRunning())
      startTimerHz(juce::roundToInt(m_maxHz));
  }

  void timerCallback() override {
    doRepaint();
    // Nothing pending after a flush → stop until the next request.
    if (!m_fullPending && !m_regionPending)
      stopTimer();
  }

  void doRepaint() {
    if (m_fullPending)
      m_owner.repaint();
    else if (m_regionPending)
      m_owner.repaint(m_dirty);

    m_fullPending = false;
    m_regionPending = false;
  }

  juce::Component& m_owner;
  float m_maxHz = 60.0f;
  bool m_fullPending = false;
  bool m_regionPending = false;
  juce::Rectangle<int> m_dirty;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RepaintThrottle)
};

} // namespace shmui
