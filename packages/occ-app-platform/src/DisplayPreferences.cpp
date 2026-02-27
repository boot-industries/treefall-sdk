/*
  ==============================================================================

    DisplayPreferences.cpp
    Created: 12 Jan 2026
    Author:  Orpheus Clip Composer

    Sprint 15: Display Preferences System (OCC117)

  ==============================================================================
*/

#include <orpheus/app/DisplayPreferences.h>

namespace orpheus {

//==============================================================================
DisplayPreferences::DisplayPreferences() {
  load();
}

//==============================================================================
// Setters with persistence

void DisplayPreferences::setPageTabHeight(Size size) {
  if (m_pageTabHeight != size) {
    m_pageTabHeight = size;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setStatusBarHeight(Size size) {
  if (m_statusBarHeight != size) {
    m_statusBarHeight = size;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setBevelWidth(BevelWidth width) {
  if (m_bevelWidth != width) {
    m_bevelWidth = width;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setButtonTriggerSize(Size size) {
  if (m_buttonTriggerSize != size) {
    m_buttonTriggerSize = size;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setButtonTextMode(ButtonTextMode mode) {
  if (m_buttonTextMode != mode) {
    m_buttonTextMode = mode;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setShowButtonTriggers(bool show) {
  if (m_showButtonTriggers != show) {
    m_showButtonTriggers = show;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setEdgedText(bool edged) {
  if (m_edgedText != edged) {
    m_edgedText = edged;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setElapsedTimeMode(bool elapsed) {
  if (m_elapsedTimeMode != elapsed) {
    m_elapsedTimeMode = elapsed;
    save();
    notifyChanged();
  }
}

void DisplayPreferences::setLevelMeterOrientation(LevelMeterOrientation orientation) {
  if (m_levelMeterOrientation != orientation) {
    m_levelMeterOrientation = orientation;
    save();
    notifyChanged();
  }
}

//==============================================================================
// Persistence

juce::PropertiesFile::Options DisplayPreferences::getPropertiesFileOptions() const {
  juce::PropertiesFile::Options options;
  options.applicationName = "OrpheusClipComposer";
  options.filenameSuffix = ".displayprefs";
  options.osxLibrarySubFolder = "Application Support";
  options.folderName = "OrpheusClipComposer";
  options.storageFormat = juce::PropertiesFile::storeAsXML;
  return options;
}

void DisplayPreferences::save() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  prefs.setValue("pageTabHeight", sizeToString(m_pageTabHeight));
  prefs.setValue("statusBarHeight", sizeToString(m_statusBarHeight));
  prefs.setValue("bevelWidth", bevelWidthToString(m_bevelWidth));
  prefs.setValue("buttonTriggerSize", sizeToString(m_buttonTriggerSize));
  prefs.setValue("buttonTextMode", buttonTextModeToString(m_buttonTextMode));
  prefs.setValue("showButtonTriggers", m_showButtonTriggers);
  prefs.setValue("edgedText", m_edgedText);
  prefs.setValue("elapsedTimeMode", m_elapsedTimeMode);
  prefs.setValue("levelMeterOrientation", levelMeterOrientationToString(m_levelMeterOrientation));

  prefs.saveIfNeeded();
}

void DisplayPreferences::load() {
  juce::PropertiesFile prefs(getPropertiesFileOptions());

  m_pageTabHeight = stringToSize(prefs.getValue("pageTabHeight", "Medium"));
  m_statusBarHeight = stringToSize(prefs.getValue("statusBarHeight", "Medium"));
  m_bevelWidth = stringToBevelWidth(prefs.getValue("bevelWidth", "Percent10"));
  m_buttonTriggerSize = stringToSize(prefs.getValue("buttonTriggerSize", "Medium"));
  m_buttonTextMode = stringToButtonTextMode(prefs.getValue("buttonTextMode", "HotKey"));
  m_showButtonTriggers = prefs.getBoolValue("showButtonTriggers", true);
  m_edgedText = prefs.getBoolValue("edgedText", false);
  m_elapsedTimeMode = prefs.getBoolValue("elapsedTimeMode", false);
  m_levelMeterOrientation =
      stringToLevelMeterOrientation(prefs.getValue("levelMeterOrientation", "Horizontal"));
}

void DisplayPreferences::resetToDefaults() {
  m_pageTabHeight = Size::Medium;
  m_statusBarHeight = Size::Medium;
  m_bevelWidth = BevelWidth::Percent10;
  m_buttonTriggerSize = Size::Medium;
  m_buttonTextMode = ButtonTextMode::HotKey;
  m_showButtonTriggers = true;
  m_edgedText = false;
  m_elapsedTimeMode = false;
  m_levelMeterOrientation = LevelMeterOrientation::Horizontal;

  save();
  notifyChanged();
}

void DisplayPreferences::notifyChanged() {
  if (onPreferencesChanged) {
    onPreferencesChanged();
  }
}

//==============================================================================
// Enum conversions

juce::String DisplayPreferences::sizeToString(Size size) {
  switch (size) {
  case Size::Small:
    return "Small";
  case Size::Medium:
    return "Medium";
  case Size::Large:
    return "Large";
  }
  return "Medium";
}

DisplayPreferences::Size DisplayPreferences::stringToSize(const juce::String& str) {
  if (str == "Small")
    return Size::Small;
  if (str == "Large")
    return Size::Large;
  return Size::Medium;
}

juce::String DisplayPreferences::bevelWidthToString(BevelWidth width) {
  switch (width) {
  case BevelWidth::None:
    return "None";
  case BevelWidth::Percent5:
    return "Percent5";
  case BevelWidth::Percent10:
    return "Percent10";
  case BevelWidth::Percent15:
    return "Percent15";
  case BevelWidth::Percent20:
    return "Percent20";
  }
  return "Percent10";
}

DisplayPreferences::BevelWidth DisplayPreferences::stringToBevelWidth(const juce::String& str) {
  if (str == "None")
    return BevelWidth::None;
  if (str == "Percent5")
    return BevelWidth::Percent5;
  if (str == "Percent15")
    return BevelWidth::Percent15;
  if (str == "Percent20")
    return BevelWidth::Percent20;
  return BevelWidth::Percent10;
}

juce::String DisplayPreferences::buttonTextModeToString(ButtonTextMode mode) {
  switch (mode) {
  case ButtonTextMode::None:
    return "None";
  case ButtonTextMode::HotKey:
    return "HotKey";
  case ButtonTextMode::MidiNote:
    return "MidiNote";
  }
  return "HotKey";
}

DisplayPreferences::ButtonTextMode
DisplayPreferences::stringToButtonTextMode(const juce::String& str) {
  if (str == "None")
    return ButtonTextMode::None;
  if (str == "MidiNote")
    return ButtonTextMode::MidiNote;
  return ButtonTextMode::HotKey;
}

juce::String DisplayPreferences::levelMeterOrientationToString(LevelMeterOrientation orientation) {
  switch (orientation) {
  case LevelMeterOrientation::Horizontal:
    return "Horizontal";
  case LevelMeterOrientation::Vertical:
    return "Vertical";
  }
  return "Horizontal";
}

DisplayPreferences::LevelMeterOrientation
DisplayPreferences::stringToLevelMeterOrientation(const juce::String& str) {
  if (str == "Vertical")
    return LevelMeterOrientation::Vertical;
  return LevelMeterOrientation::Horizontal;
}

//==============================================================================
// Pixel values

int DisplayPreferences::getPageTabHeightPixels(Size size) {
  switch (size) {
  case Size::Small:
    return 28;
  case Size::Medium:
    return 36;
  case Size::Large:
    return 48;
  }
  return 36;
}

int DisplayPreferences::getStatusBarHeightPixels(Size size) {
  switch (size) {
  case Size::Small:
    return 20;
  case Size::Medium:
    return 28;
  case Size::Large:
    return 36;
  }
  return 28;
}

int DisplayPreferences::getButtonTriggerSizePixels(Size size) {
  switch (size) {
  case Size::Small:
    return 8;
  case Size::Medium:
    return 12;
  case Size::Large:
    return 16;
  }
  return 12;
}

float DisplayPreferences::getBevelWidthPercent(BevelWidth width) {
  switch (width) {
  case BevelWidth::None:
    return 0.0f;
  case BevelWidth::Percent5:
    return 0.05f;
  case BevelWidth::Percent10:
    return 0.10f;
  case BevelWidth::Percent15:
    return 0.15f;
  case BevelWidth::Percent20:
    return 0.20f;
  }
  return 0.10f;
}

} // namespace orpheus
