// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

//==============================================================================
/**
 * TabSwitcher - Merged tab bar and transport controls (OCC130 Sprint B)
 *
 * Provides 8 tabs, each representing a page of 48 clips (6×8 grid).
 * Total capacity: 8 × 48 = MAX_CLIP_BUTTONS clips
 *
 * OCC130 Sprint B: Merged layout (single row):
 * | [Tab 1] [Tab 2] ... [Tab 8]  |  [●] [●]  |  [Stop All] [Panic] |
 * |      (flex space)             | Latency   |   (min space)       |
 * |                               | Heartbeat |                     |
 *
 * Features:
 * - Visual feedback for active tab
 * - Keyboard shortcuts (Cmd+1 through Cmd+8)
 * - Tab labels (editable via double-click or context menu)
 * - Transport controls (Stop All, Panic)
 * - Status indicator lights (latency, heartbeat)
 * - HK Grotesk font
 */
class TabSwitcher : public juce::Component, private juce::Timer {
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
  // OCC130 Sprint B: Transport controls
  void setLatencyInfo(double latencyMs, int bufferSize, int sampleRate);
  void setPerformanceInfo(float cpuPercent, int memoryMB);

  //==============================================================================
  // Callbacks
  std::function<void(int tabIndex)> onTabSelected;
  std::function<void()> onStopAll; // OCC130 Sprint B: Stop All button
  std::function<void()> onPanic;   // OCC130 Sprint B: Panic button

  //==============================================================================
  void paint(juce::Graphics& g) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override; // OCC130 Sprint B.4: Tab renaming
  void mouseMove(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent& e) override;

private:
  //==============================================================================
  int getTabAtPosition(int x, int y) const;
  juce::Rectangle<int> getTabBounds(int tabIndex) const;

  // OCC130 Sprint B: Timer callback for heartbeat pulse animation
  void timerCallback() override;

  // OCC130 Sprint B.4: Tab renaming support
  void showRenameEditor(int tabIndex);
  void hideRenameEditor();
  void showTabContextMenu(int tabIndex);

  //==============================================================================
  static constexpr int NUM_TABS = 8;
  static constexpr int TAB_HEIGHT = 40;
  static constexpr int TAB_GAP = 2;

  int m_activeTab = 0;
  int m_hoveredTab = -1;

  juce::Array<juce::String> m_tabLabels;

  // OCC130 Sprint B: Transport controls
  std::unique_ptr<juce::TextButton> m_stopAllButton;
  std::unique_ptr<juce::TextButton> m_panicButton;

  // OCC130 Sprint B: Status indicator state
  double m_latencyMs = 0.0;
  float m_cpuPercent = 0.0f;
  int m_heartbeatPhase = 0; // For pulse animation (0-100)

  // OCC130 Sprint B.4: Tab renaming support
  std::unique_ptr<juce::TextEditor> m_renameEditor;
  int m_editingTabIndex = -1; // -1 when not editing

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabSwitcher)
};
