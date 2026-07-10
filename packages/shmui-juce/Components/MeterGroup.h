/*
  ==============================================================================

    MeterGroup.h
    Created: shmui Component Library

    Composite "N groups + Master" level-meter layout.

    A single component that lays out N per-group LevelMeters plus an optional
    Master meter, with group-coloured labels. This is the shared primitive
    behind broadcast/theatre layouts like Clip Composer's 4 playback groups +
    Master (OCC153 G3), so apps stop hand-laying the arrangement.

  ==============================================================================
*/

#pragma once

#include "../Utils/DesignTokens.h"
#include "LevelMeter.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace shmui {

//==============================================================================
/**
 * @brief Composite multi-meter: N group meters + optional Master.
 *
 * Owns N+1 LevelMeters. Group colours default to the Orpheus --group-*
 * palette (A/B/C/D → blue/green/orange/red). Forwards level-history config to
 * the child meters so a single onLevelEvent hook logs the whole group.
 *
 * Message-thread only (like LevelMeter).
 */
class MeterGroup : public juce::Component {
public:
  //==============================================================================
  /**
   * @brief Create a meter group.
   * @param numGroups  number of group meters (>= 1)
   * @param withMaster append a Master meter after the groups
   */
  explicit MeterGroup(int numGroups, bool withMaster = true);
  ~MeterGroup() override = default;

  //==============================================================================
  /// @name Levels (thread-safe — delegates to child meters)
  /// @{

  /** Set a group's level (linear 0..1+). */
  void setGroupLevel(int group, float linear);

  /** Set a group's level in dB. */
  void setGroupLevelDB(int group, float dB);

  /** Set the Master level (linear). No-op if created without a master. */
  void setMasterLevel(float linear);

  /** Set the Master level in dB. */
  void setMasterLevelDB(float dB);

  /** Reset all meters. */
  void reset();

  /// @}

  //==============================================================================
  /// @name Appearance
  /// @{

  /** Set a group's label (drawn under the meter). */
  void setGroupLabel(int group, const juce::String& label);

  /** Set the Master label (default "MASTER"). */
  void setMasterLabel(const juce::String& label);

  /** Set a group's accent colour (defaults from --group-*). */
  void setGroupColour(int group, juce::Colour colour);

  /** Apply a style to every child meter. */
  void setMeterStyle(const LevelMeterStyle& style);

  /** Set ballistics on every child meter. */
  void setBallistics(MeterBallistics ballistics);

  /// @}

  //==============================================================================
  /// @name Access
  /// @{

  int getNumGroups() const {
    return static_cast<int>(m_groups.size());
  }
  bool hasMaster() const {
    return m_master != nullptr;
  }

  /** Direct access to a group meter (for advanced config). */
  LevelMeter& getGroupMeter(int group);

  /** Direct access to the Master meter (asserts if none). */
  LevelMeter& getMasterMeter();

  /// @}

  //==============================================================================
  /// @name Level history (fans out to all child meters)
  /// @{

  /**
   * @brief Enable history on every child meter and route their events to
   * this group's onLevelEvent, stamping the LevelEvent::channel with the
   * group index (Master reported as numGroups).
   */
  void enableHistory(int capacityPerMeter);

  /** Disable history on all child meters. */
  void disableHistory();

  /** Fired for any child meter's level event (channel = group index). */
  std::function<void(const LevelEvent&)> onLevelEvent;

  /// @}

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  //==============================================================================
  struct GroupStrip {
    std::unique_ptr<LevelMeter> meter;
    juce::String label;
    juce::Colour colour;
  };

  std::vector<GroupStrip> m_groups;
  std::unique_ptr<LevelMeter> m_master;
  juce::String m_masterLabel{"MASTER"};
  juce::Colour m_masterColour{tokens::lab::text()};

  static constexpr float LABEL_HEIGHT = 16.0f;
  static constexpr float STRIP_GAP = 4.0f;
  static constexpr float MASTER_GAP = 10.0f; // wider separator before master

  void layoutStrip(juce::Rectangle<int> area, LevelMeter& meter);
  void drawLabel(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                 juce::Colour colour);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterGroup)
};

} // namespace shmui
