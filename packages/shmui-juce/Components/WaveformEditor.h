/*
  ==============================================================================

    WaveformEditor.h
    Created: shmui Component Library

    Advanced waveform editor with trim/fade markers, playback position,
    and interactive editing. Based on OCC WaveformDisplay.

    Features:
    - Trim markers (start/end handles)
    - Fade in/out visualization curves
    - Playback position indicator (vertical line)
    - Click-to-seek support
    - Selection regions
    - Zoom/scroll support
    - Peak cache for large files

  ==============================================================================
*/

#pragma once

#include "../Utils/ColorUtils.h"
#include "../Utils/DesignTokens.h"
#include "../Utils/Interpolation.h"
#include "../Utils/RepaintThrottle.h"
#include <JuceHeader.h>
#include <map>
#include <vector>

namespace shmui {

//==============================================================================
/**
 * @brief Style configuration for WaveformEditor.
 *
 * Defaults derive from the Orpheus --wave-* token contract (see DesignTokens.h).
 * Every field remains overridable via setStyle() for per-instance theming.
 */
struct WaveformEditorStyle {
  // Waveform colors (--wave-line / --wave-bg)
  juce::Colour waveformColor = tokens::wave::line();
  juce::Colour waveformFillColor = tokens::wave::line().withAlpha(0.25f);
  juce::Colour backgroundColor = tokens::wave::bg();

  // Playhead (--wave-playhead)
  juce::Colour playheadColor = tokens::wave::playhead();
  float playheadWidth = 2.0f;

  // Trim markers (--wave-trim-in for the handle; matched region tint)
  juce::Colour trimHandleColor = tokens::wave::trimIn();
  juce::Colour trimRegionColor = tokens::wave::trimIn().withAlpha(0.12f);
  float trimHandleWidth = 8.0f;

  // Fade curves (accent-tinted; app can override)
  juce::Colour fadeColor = tokens::meter::green().withAlpha(0.5f);

  // Selection
  juce::Colour selectionColor = tokens::wave::line().withAlpha(0.25f);

  // Grid/time scale
  juce::Colour gridColor = juce::Colour(0x30FFFFFF);
  juce::Colour timeTextColor = juce::Colour(0x80FFFFFF);
  bool showGrid = true;
  bool showTimeScale = true;
};

//==============================================================================
/**
 * @brief Type of a cue marker (broadcast/DJ clip editing).
 */
enum class CueType {
  Hook,  ///< Song hook / main phrase
  Drop,  ///< Drop / impact point
  Outro, ///< Outro / tail-out cue
  Custom ///< App-defined
};

//==============================================================================
/**
 * @brief A labelled, typed, colourable cue marker at a sample position.
 *
 * First-class markers the WaveformEditor draws and lets the user drag. This is
 * table-stakes for broadcast/DJ editing and Clip Composer's Hook/Drop/Outro/
 * Custom cues (OCC153 G4).
 */
struct CueMarker {
  juce::String id;                ///< Unique key
  int64_t sample = 0;             ///< Position in samples
  CueType type = CueType::Custom; ///< Semantic type
  juce::Colour color;             ///< Explicit colour (transparent = per-type default)
  juce::String label;             ///< Optional display label
};

//==============================================================================
/**
 * @brief Viewport auto-follow mode for real-time playback (OCC153 G6).
 */
enum class FollowMode {
  Off,   ///< No auto-scroll
  Page,  ///< Jump one page when the playhead leaves the viewport
  Center ///< Keep the playhead centered
};

//==============================================================================
/**
 * @brief Waveform data storage with efficient caching.
 */
struct WaveformData {
  std::vector<float> minValues; // Min sample value per pixel column
  std::vector<float> maxValues; // Max sample value per pixel column
  int sampleRate = 48000;
  int numChannels = 2;
  int64_t totalSamples = 0;
  bool isValid = false;
};

//==============================================================================
/**
 * @brief Advanced waveform editor component.
 *
 * Provides a full-featured waveform editor for audio applications:
 * - Visual trim point markers with drag handles
 * - Fade in/out visualization
 * - Playback position indicator
 * - Click-to-seek interaction
 * - Selection regions
 * - Zoom and scroll support
 * - Efficient peak caching for large files
 *
 * Thread-safe: Waveform data generation can happen on a background thread.
 */
class WaveformEditor : public juce::Component {
public:
  //==============================================================================
  WaveformEditor();
  ~WaveformEditor() override;

  //==============================================================================
  /// @name Audio File
  /// @{

  /**
   * @brief Load an audio file and generate waveform data.
   * @param audioFile File to load
   */
  void setAudioFile(const juce::File& audioFile);

  /**
   * @brief Set waveform data directly (for pre-generated data).
   */
  void setWaveformData(const WaveformData& data);

  /**
   * @brief Clear the waveform data.
   */
  void clear();

  /**
   * @brief Check if waveform data is loaded.
   */
  bool hasData() const {
    return m_waveformData.isValid;
  }

  /// @}

  //==============================================================================
  /// @name Trim Points
  /// @{

  /**
   * @brief Set trim points in samples.
   */
  void setTrimPoints(int64_t trimInSamples, int64_t trimOutSamples);

  /**
   * @brief Set trim points as normalized positions (0.0 - 1.0).
   */
  void setTrimPointsNormalized(float trimIn, float trimOut);

  /**
   * @brief Get trim in point in samples.
   */
  int64_t getTrimInSamples() const {
    return m_trimInSamples;
  }

  /**
   * @brief Get trim out point in samples.
   */
  int64_t getTrimOutSamples() const {
    return m_trimOutSamples;
  }

  /// @}

  //==============================================================================
  /// @name Fade Points
  /// @{

  /**
   * @brief Set fade in duration in samples.
   */
  void setFadeInSamples(int64_t samples);

  /**
   * @brief Set fade out duration in samples.
   */
  void setFadeOutSamples(int64_t samples);

  /**
   * @brief Get fade in duration in samples.
   */
  int64_t getFadeInSamples() const {
    return m_fadeInSamples;
  }

  /**
   * @brief Get fade out duration in samples.
   */
  int64_t getFadeOutSamples() const {
    return m_fadeOutSamples;
  }

  /// @}

  //==============================================================================
  /// @name Playback Position
  /// @{

  /**
   * @brief Set playhead position in samples.
   */
  void setPlayheadPosition(int64_t samplePosition);

  /**
   * @brief Set playhead position as normalized (0.0 - 1.0).
   */
  void setPlayheadNormalized(float position);

  /**
   * @brief Get playhead position in samples.
   */
  int64_t getPlayheadPosition() const {
    return m_playheadPosition;
  }

  /// @}

  //==============================================================================
  /// @name Selection
  /// @{

  /**
   * @brief Set selection region in samples.
   */
  void setSelection(int64_t startSamples, int64_t endSamples);

  /**
   * @brief Clear selection.
   */
  void clearSelection();

  /**
   * @brief Check if there is an active selection.
   */
  bool hasSelection() const {
    return m_selectionStart < m_selectionEnd;
  }

  /// @}

  //==============================================================================
  /// @name Cue Markers (G4)
  /// @{

  /** Add or replace a cue marker (keyed by id). */
  void addCueMarker(const CueMarker& marker);

  /** Remove a cue marker by id. */
  void removeCueMarker(const juce::String& id);

  /** Remove all cue markers. */
  void clearCueMarkers();

  /** Access the current markers (draw order). */
  const std::vector<CueMarker>& getCueMarkers() const {
    return m_cueMarkers;
  }

  /** Fired when the user drags a marker to a new sample position. */
  std::function<void(const juce::String& id, int64_t newSample)> onCueMarkerMoved;

  /// @}

  //==============================================================================
  /// @name Audition Region (G5)
  /// @{

  /**
   * @brief Highlight a sub-region for end-of-clip audition (e.g. last 2 s).
   * Distinct from the edit selection; drawn as a tinted overlay.
   */
  void setAuditionRegion(int64_t startSamples, int64_t endSamples);

  /** Convenience: audition the final @p seconds of the clip. */
  void setAuditionRegionFromEnd(double seconds);

  /** Clear the audition region. */
  void clearAuditionRegion();

  /** @return true if an audition region is set. */
  bool hasAuditionRegion() const {
    return m_auditionStart < m_auditionEnd;
  }

  /** Fired when the user double-clicks inside the audition region. */
  std::function<void()> onAuditionRequested;

  /// @}

  //==============================================================================
  /// @name Zoom & Scroll
  /// @{

  /**
   * @brief Set zoom level (1.0 = fit all, 2.0 = 2x zoom, etc.).
   */
  void setZoomLevel(float zoom);

  /**
   * @brief Get current zoom level.
   */
  float getZoomLevel() const {
    return m_zoomLevel;
  }

  /**
   * @brief Set scroll position (0.0 - 1.0, normalized).
   */
  void setScrollPosition(float position);

  /**
   * @brief Get current scroll position.
   */
  float getScrollPosition() const {
    return m_scrollPosition;
  }

  /**
   * @brief Zoom to fit selection or full waveform.
   */
  void zoomToFit();

  /**
   * @brief Zoom to selection region.
   */
  void zoomToSelection();

  /// @}

  //==============================================================================
  /// @name Play-follow (G6)
  /// @{

  /**
   * @brief Auto-scroll the viewport to keep the playhead visible at high zoom.
   * Repaints/scrolls are throttled (see setFollowThrottleHz) so real-time
   * playback holds frame rate on large sessions.
   */
  void setFollowMode(FollowMode mode);
  FollowMode getFollowMode() const {
    return m_followMode;
  }

  /** Set the max follow-scroll update rate (Hz). */
  void setFollowThrottleHz(float hz);

  /// @}

  //==============================================================================
  /// @name Style
  /// @{

  /**
   * @brief Set visual style.
   */
  void setStyle(const WaveformEditorStyle& style);

  /**
   * @brief Get current style.
   */
  const WaveformEditorStyle& getStyle() const {
    return m_style;
  }

  /// @}

  //==============================================================================
  /// @name Callbacks
  /// @{

  /** Callback when user clicks to seek (position in samples). */
  std::function<void(int64_t samplePosition)> onSeek;

  /** Callback when trim points change (trimIn, trimOut in samples). */
  std::function<void(int64_t trimIn, int64_t trimOut)> onTrimPointsChanged;

  /** Callback when selection changes (start, end in samples). */
  std::function<void(int64_t start, int64_t end)> onSelectionChanged;

  /// @}

  //==============================================================================
  // Component overrides
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseMove(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
  //==============================================================================
  enum class DragHandle { None, TrimIn, TrimOut, Playhead, Selection, CueMarker };

  void generateWaveformData(const juce::File& audioFile);
  void drawWaveform(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawTrimMarkers(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawFadeCurves(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawSelection(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawAuditionRegion(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawCueMarkers(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawTimeScale(juce::Graphics& g, juce::Rectangle<float> bounds);
  void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

  float sampleToX(int64_t sample, float width) const;
  int64_t xToSample(float x, float width) const;
  bool isNearHandle(float mouseX, float handleX, float tolerance = 8.0f) const;
  DragHandle getHandleAt(float x, float y) const;
  void updateCursor(DragHandle handle);

  juce::Colour cueColour(const CueMarker& marker) const;
  int cueMarkerIndexAt(float x, float width) const; // -1 if none near
  void followPlayhead();                            // G6 viewport auto-scroll

  juce::String formatTime(int64_t samples) const;

  //==============================================================================
  WaveformData m_waveformData;
  WaveformEditorStyle m_style;

  // Trim points
  int64_t m_trimInSamples = 0;
  int64_t m_trimOutSamples = 0;

  // Fade points
  int64_t m_fadeInSamples = 0;
  int64_t m_fadeOutSamples = 0;

  // Playhead
  int64_t m_playheadPosition = 0;

  // Selection
  int64_t m_selectionStart = 0;
  int64_t m_selectionEnd = 0;

  // Audition region (G5) — preview highlight, distinct from selection
  int64_t m_auditionStart = 0;
  int64_t m_auditionEnd = 0;

  // Cue markers (G4)
  std::vector<CueMarker> m_cueMarkers;

  // Zoom/Scroll
  float m_zoomLevel = 1.0f;
  float m_scrollPosition = 0.0f;

  // Play-follow (G6). Playhead-advance repaints are coalesced through a
  // shared RepaintThrottle (G9) so rapid setPlayheadPosition() calls don't
  // each force a full repaint at high update rates.
  FollowMode m_followMode = FollowMode::Off;
  float m_followThrottleHz = 30.0f;
  int64_t m_lastFollowMs = 0;
  RepaintThrottle m_playheadRepaint{*this, 60.0f};

  // Interaction state
  DragHandle m_draggedHandle = DragHandle::None;
  juce::Point<float> m_dragStartPoint;
  int64_t m_dragStartValue = 0;
  bool m_isSelecting = false;
  int m_draggedCueIndex = -1; // index into m_cueMarkers while dragging a cue

  // Caching
  juce::String m_cachedFilePath;
  std::map<juce::String, WaveformData> m_waveformCache;
  std::atomic<bool> m_isLoading{false};
  juce::CriticalSection m_dataLock;

  static constexpr size_t MAX_CACHE_SIZE = 5;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformEditor)
};

} // namespace shmui
