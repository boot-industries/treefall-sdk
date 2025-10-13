// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

//==============================================================================
/**
 * TabSwitcher - Horizontal tab selector for switching between clip pages
 *
 * Provides 8 tabs, each representing a page of 48 clips (6×8 grid).
 * Total capacity: 8 × 48 = 384 clips
 *
 * Features:
 * - Visual feedback for active tab
 * - Keyboard shortcuts (Cmd+1 through Cmd+8)
 * - Tab labels (editable in future)
 * - Inter font consistent with rest of UI
 */
class TabSwitcher : public juce::Component {
public:
  //==============================================================================
  TabSwitcher();
  ~TabSwitcher() override = default;

  //==============================================================================
  // Tab management
  void setActiveTab(int tabIndex);
  int getActiveTab() const {
    return m_activeTab;
  }
  int getTabCount() const {
    return NUM_TABS;
  }

  // Tab labels (for future session metadata)
  void setTabLabel(int tabIndex, const juce::String& label);
  juce::String getTabLabel(int tabIndex) const;

  //==============================================================================
  // Callbacks
  std::function<void(int tabIndex)> onTabSelected;

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseMove(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent& e) override;

private:
  //==============================================================================
  int getTabAtPosition(int x, int y) const;
  juce::Rectangle<int> getTabBounds(int tabIndex) const;

  //==============================================================================
  static constexpr int NUM_TABS = 8;
  static constexpr int TAB_HEIGHT = 40;
  static constexpr int TAB_GAP = 2;

  int m_activeTab = 0;
  int m_hoveredTab = -1;

  juce::Array<juce::String> m_tabLabels;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabSwitcher)
};
