/*
  ==============================================================================

    DisplayPreferences.h
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 15: Display Preferences System (OCC117)
    Stores application-wide display settings for UI customization.

  ==============================================================================
*/

#pragma once

#include <functional>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace orpheus {

/**
    Manages application-wide display preferences.
    Settings are persisted via JUCE PropertiesFile (platform-specific location).

    NOT session-specific - these are user preferences that persist across sessions.
*/
class DisplayPreferences {
public:
  //==============================================================================
  // Enums for settings

  enum class Size { Small, Medium, Large };

  enum class BevelWidth { None, Percent5, Percent10, Percent15, Percent20 };

  enum class ButtonTextMode { None, HotKey, MidiNote };

  enum class LevelMeterOrientation { Horizontal, Vertical };

  enum class GridLayout {
    Columns6Rows6,
    Columns8Rows6,
    Columns10Rows6,
    Columns6Rows8,
    Columns8Rows8,
    Columns10Rows8,
    Columns12Rows8,
    Columns6Rows10,
    Columns8Rows10,
    Columns10Rows10
  };

  //==============================================================================
  DisplayPreferences();
  ~DisplayPreferences() = default;

  //==============================================================================
  // Page Tab Settings

  void setPageTabHeight(Size size);
  Size getPageTabHeight() const {
    return m_pageTabHeight;
  }

  //==============================================================================
  // Status Bar Settings

  void setStatusBarHeight(Size size);
  Size getStatusBarHeight() const {
    return m_statusBarHeight;
  }

  //==============================================================================
  // Button Appearance

  void setBevelWidth(BevelWidth width);
  BevelWidth getBevelWidth() const {
    return m_bevelWidth;
  }

  void setButtonTriggerSize(Size size);
  Size getButtonTriggerSize() const {
    return m_buttonTriggerSize;
  }

  void setGridLayout(GridLayout layout);
  GridLayout getGridLayout() const {
    return m_gridLayout;
  }

  //==============================================================================
  // Display Modes

  void setButtonTextMode(ButtonTextMode mode);
  ButtonTextMode getButtonTextMode() const {
    return m_buttonTextMode;
  }

  void setShowButtonTriggers(bool show);
  bool getShowButtonTriggers() const {
    return m_showButtonTriggers;
  }

  void setEdgedText(bool edged);
  bool getEdgedText() const {
    return m_edgedText;
  }

  /** Set elapsed time mode (false = countdown, true = elapsed/count-up) */
  void setElapsedTimeMode(bool elapsed);
  bool getElapsedTimeMode() const {
    return m_elapsedTimeMode;
  }

  //==============================================================================
  // Level Meter Settings

  void setLevelMeterOrientation(LevelMeterOrientation orientation);
  LevelMeterOrientation getLevelMeterOrientation() const {
    return m_levelMeterOrientation;
  }

  //==============================================================================
  // Persistence

  /** Save all preferences to disk */
  void save();

  /** Load preferences from disk */
  void load();

  /** Reset all preferences to defaults */
  void resetToDefaults();

  //==============================================================================
  // Callbacks

  /** Called when any preference changes (for UI updates) */
  std::function<void()> onPreferencesChanged;

  //==============================================================================
  // Utility: Convert enums to/from strings for storage

  static juce::String sizeToString(Size size);
  static Size stringToSize(const juce::String& str);

  static juce::String bevelWidthToString(BevelWidth width);
  static BevelWidth stringToBevelWidth(const juce::String& str);

  static juce::String buttonTextModeToString(ButtonTextMode mode);
  static ButtonTextMode stringToButtonTextMode(const juce::String& str);

  static juce::String levelMeterOrientationToString(LevelMeterOrientation orientation);
  static LevelMeterOrientation stringToLevelMeterOrientation(const juce::String& str);

  static juce::String gridLayoutToString(GridLayout layout);
  static GridLayout stringToGridLayout(const juce::String& str);

  //==============================================================================
  // Utility: Get pixel values for Size enum

  static int getPageTabHeightPixels(Size size);
  static int getStatusBarHeightPixels(Size size);
  static int getButtonTriggerSizePixels(Size size);
  static float getBevelWidthPercent(BevelWidth width);
  static int getGridLayoutColumns(GridLayout layout);
  static int getGridLayoutRows(GridLayout layout);

private:
  //==============================================================================
  void notifyChanged();
  juce::PropertiesFile::Options getPropertiesFileOptions() const;

  //==============================================================================
  // Settings
  Size m_pageTabHeight = Size::Medium;
  Size m_statusBarHeight = Size::Medium;
  BevelWidth m_bevelWidth = BevelWidth::Percent5;
  Size m_buttonTriggerSize = Size::Medium;
  GridLayout m_gridLayout = GridLayout::Columns8Rows6;
  ButtonTextMode m_buttonTextMode = ButtonTextMode::HotKey;
  bool m_showButtonTriggers = true;
  bool m_edgedText = false;
  bool m_elapsedTimeMode = false; // false = countdown (default)
  LevelMeterOrientation m_levelMeterOrientation = LevelMeterOrientation::Horizontal;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DisplayPreferences)
};

} // namespace orpheus
