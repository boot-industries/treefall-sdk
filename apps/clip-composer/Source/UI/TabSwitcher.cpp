// SPDX-License-Identifier: MIT

#include "TabSwitcher.h"
#include "DesignTokens.h"

#include <cmath>

namespace {
constexpr int kLeftMargin = 10;
constexpr int kRightMargin = 10;
constexpr int kModeStripSpacing = 10;
} // namespace

//==============================================================================
TabSwitcher::TabSwitcher() {
  // Initialize default tab labels
  const char* defaultLabels[NUM_TABS] = {"Tab 1", "Tab 2", "Tab 3", "Tab 4",
                                         "Tab 5", "Tab 6", "Tab 7", "Tab 8"};
  for (int i = 0; i < NUM_TABS; ++i) {
    m_tabLabels.add(juce::String(defaultLabels[i]));
  }

  auto makeModeButton = [this](std::unique_ptr<juce::TextButton>& button, const juce::String& label,
                               occ::ui::OperatorViewMode mode) {
    button = std::make_unique<juce::TextButton>(label);
    button->setButtonText(label);
    button->setClickingTogglesState(true);
    button->onClick = [this, mode]() {
      setOperatorViewMode(mode);
      if (onOperatorViewModeSelected)
        onOperatorViewModeSelected(mode);
    };
    button->setColour(juce::TextButton::buttonColourId, juce::Colour(OCC::Design::kBgComponent));
    button->setColour(juce::TextButton::buttonOnColourId, juce::Colour(OCC::Design::kAccentTeal));
    button->setColour(juce::TextButton::textColourOffId, juce::Colour(OCC::Design::kTextSecondary));
    button->setColour(juce::TextButton::textColourOnId, juce::Colour(OCC::Design::kTextPrimary));
    addAndMakeVisible(button.get());
  };

  makeModeButton(m_playoutButton, "Playout", occ::ui::OperatorViewMode::Playout);
  makeModeButton(m_editButton, "Edit", occ::ui::OperatorViewMode::Edit);
  makeModeButton(m_routingButton, "Routing", occ::ui::OperatorViewMode::Routing);
  makeModeButton(m_preferencesButton, "Prefs", occ::ui::OperatorViewMode::Preferences);

  // OCC130 Sprint B: Create Stop All button
  m_stopAllButton = std::make_unique<juce::TextButton>("Stop All");
  m_stopAllButton->setButtonText("Stop All");
  m_stopAllButton->onClick = [this]() {
    if (onStopAll)
      onStopAll();
  };
  addAndMakeVisible(m_stopAllButton.get());

  // OCC130 Sprint B: Create Panic button (red, emergency stop)
  m_panicButton = std::make_unique<juce::TextButton>("Panic");
  m_panicButton->setButtonText("PANIC");
  m_panicButton->onClick = [this]() {
    if (onPanic)
      onPanic();
  };
  m_panicButton->setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
  m_panicButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
  addAndMakeVisible(m_panicButton.get());

  // OCC130 Sprint B: Start heartbeat animation timer (1Hz pulse)
  // Timer fires every 10ms, phase increments 0→100 in 1 second (100 steps × 10ms = 1000ms)
  startTimer(10); // 10ms intervals for smooth 1Hz pulse animation

  setSize(800, m_tabHeight);
  updateModeButtonStates();
}

void TabSwitcher::setOperatorViewMode(occ::ui::OperatorViewMode mode) {
  m_operatorViewMode = mode;
  updateModeButtonStates();
  repaint();
}

void TabSwitcher::updateModeButtonStates() {
  if (m_playoutButton)
    m_playoutButton->setToggleState(m_operatorViewMode == occ::ui::OperatorViewMode::Playout,
                                    juce::dontSendNotification);
  if (m_editButton)
    m_editButton->setToggleState(m_operatorViewMode == occ::ui::OperatorViewMode::Edit,
                                 juce::dontSendNotification);
  if (m_routingButton)
    m_routingButton->setToggleState(m_operatorViewMode == occ::ui::OperatorViewMode::Routing,
                                    juce::dontSendNotification);
  if (m_preferencesButton)
    m_preferencesButton->setToggleState(
        m_operatorViewMode == occ::ui::OperatorViewMode::Preferences, juce::dontSendNotification);
}

void TabSwitcher::setHealthSnapshot(
    const occ::ui::AudioEngineUiSnapshot::HealthStripSnapshot& snapshot) {
  m_cpuPercent = snapshot.cpuPercent;
  m_memoryMB = snapshot.memoryMB;
  m_bufferSize = snapshot.bufferSize;
  m_sampleRate = snapshot.sampleRate;
  m_dropoutCount = snapshot.dropoutCount;
  if (snapshot.statusText.isNotEmpty()) {
    m_deviceSummary = snapshot.statusText;
  }
  repaint();
}

void TabSwitcher::setDeviceRouteStatus(
    const occ::ui::AudioEngineUiSnapshot::DeviceRouteStatus& status) {
  m_deviceSummary = status.deviceSummary;
  m_playoutRouteLabel = status.playoutRouteLabel;
  repaint();
}

//==============================================================================
void TabSwitcher::setActiveTab(int tabIndex) {
  if (tabIndex >= 0 && tabIndex < NUM_TABS && tabIndex != m_activeTab) {
    m_activeTab = tabIndex;
    repaint();

    // Notify listeners
    if (onTabSelected)
      onTabSelected(m_activeTab);
  }
}

void TabSwitcher::setTabLabel(int tabIndex, const juce::String& label) {
  if (tabIndex >= 0 && tabIndex < NUM_TABS) {
    m_tabLabels.set(tabIndex, label);
    repaint();
  }
}

juce::String TabSwitcher::getTabLabel(int tabIndex) const {
  if (tabIndex >= 0 && tabIndex < NUM_TABS)
    return m_tabLabels[tabIndex];
  return "";
}

// OCC144: Dynamic tab height from DisplayPreferences
void TabSwitcher::setTabHeight(int height) {
  if (height != m_tabHeight && height > 0) {
    m_tabHeight = height;
    setSize(getWidth(), m_tabHeight);
    resized();
    repaint();
  }
}

//==============================================================================
// OCC130 Sprint B: Status indicator updates
void TabSwitcher::setLatencyInfo(double latencyMs, int bufferSize, int sampleRate) {
  m_latencyMs = latencyMs;
  m_bufferSize = bufferSize;
  m_sampleRate = sampleRate;
  repaint(); // Trigger repaint to update status light color
}

void TabSwitcher::setPerformanceInfo(float cpuPercent, int memoryMB) {
  m_cpuPercent = cpuPercent;
  m_memoryMB = memoryMB;
  repaint(); // Trigger repaint to update status indicators
}

void TabSwitcher::timerCallback() {
  // OCC130 Sprint B: Heartbeat pulse animation (0-100 phase)
  m_heartbeatPhase = (m_heartbeatPhase + 1) % 100;
  repaint(); // Trigger repaint for heartbeat animation
}

//==============================================================================
void TabSwitcher::paint(juce::Graphics& g) {
  // Background
  g.fillAll(juce::Colour(OCC::Design::kBgSecondary));

  // Draw tabs
  for (int i = 0; i < NUM_TABS; ++i) {
    auto tabBounds = getTabBounds(i);

    // Determine tab colors based on state
    juce::Colour tabColor;
    juce::Colour textColor;

    if (i == m_activeTab) {
      // Active tab - bright highlight
      tabColor = juce::Colour(OCC::Design::kAccentTeal);
      textColor = juce::Colour(OCC::Design::kTextPrimary);
    } else if (i == m_hoveredTab) {
      // Hovered tab - subtle highlight
      tabColor = juce::Colour(OCC::Design::kBgComponent);
      textColor = juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.8f);
    } else {
      // Inactive tab - dark
      tabColor = juce::Colour(OCC::Design::kBgSecondary).brighter(0.1f);
      textColor = juce::Colour(OCC::Design::kTextSecondary);
    }

    // Draw tab background
    g.setColour(tabColor);
    g.fillRoundedRectangle(tabBounds.toFloat(), OCC::Design::kRadiusMD);

    // Draw tab border (subtle)
    if (i == m_activeTab) {
      g.setColour(juce::Colour(OCC::Design::kBorderActive));
      g.drawRoundedRectangle(tabBounds.toFloat(), OCC::Design::kRadiusMD,
                             OCC::Design::kBorderMedium);
    }

    // Draw tab label (larger, centered)
    g.setColour(textColor);
    g.setFont(juce::FontOptions("HK Grotesk", OCC::Design::kFontMD + 1.0f, juce::Font::bold));
    g.drawText(m_tabLabels[i], tabBounds, juce::Justification::centred);
  }

  // Draw status indicator lights and compact readouts on the right side.
  auto bounds = getLocalBounds();
  float lightSize = 12.0f; // Diameter of each circular indicator
  float lightGap = 4.0f;   // Vertical gap between lights
  float rightMargin = 10.0f;
  float statusTextWidth = 190.0f;
  float statusTextGap = 6.0f;

  // Calculate position (to the right of PANIC button, outside transport controls)
  float xPos = static_cast<float>(bounds.getWidth()) - (lightSize + rightMargin);
  float yStart = (static_cast<float>(bounds.getHeight()) - (2.0f * lightSize + lightGap)) /
                 2.0f; // Center vertically
  auto statusTextArea =
      juce::Rectangle<float>(xPos - statusTextGap - statusTextWidth, yStart - 1.0f, statusTextWidth,
                             2.0f * lightSize + lightGap + 2.0f);

  g.setFont(juce::FontOptions("HK Grotesk", OCC::Design::kFontSM - 1.0f, juce::Font::plain));
  g.setColour(juce::Colour(OCC::Design::kTextSecondary));
  g.drawText(m_sampleRate > 0
                 ? juce::String(m_latencyMs, 1) + " ms / " + juce::String(m_dropoutCount) + " drop"
                 : "No I/O",
             statusTextArea.removeFromTop(lightSize + 1.0f).toNearestInt(),
             juce::Justification::centredRight);
  g.drawText(m_sampleRate > 0 ? "CPU " + juce::String(m_cpuPercent, 0) + "% / " +
                                    juce::String(m_memoryMB) + " MB"
                              : "CPU --",
             statusTextArea.removeFromTop(lightSize + 2.0f).toNearestInt(),
             juce::Justification::centredRight);
  g.drawText(m_playoutRouteLabel.isNotEmpty() ? m_playoutRouteLabel : m_deviceSummary,
             statusTextArea.toNearestInt(), juce::Justification::centredRight);

  // Latency indicator (top light)
  {
    auto latencyCircle = juce::Rectangle<float>(xPos, yStart, lightSize, lightSize);

    // Color-code based on latency (green < 10ms, yellow < 20ms, red >= 20ms)
    juce::Colour latencyColor;
    if (m_sampleRate <= 0) {
      latencyColor = juce::Colours::darkgrey;
    } else if (m_latencyMs < 10.0) {
      latencyColor = juce::Colour(OCC::Design::kMeterGreen);
    } else if (m_latencyMs < 20.0) {
      latencyColor = juce::Colour(OCC::Design::kMeterYellow);
    } else {
      latencyColor = juce::Colour(OCC::Design::kMeterRed);
    }

    g.setColour(latencyColor.withAlpha(0.9f));
    g.fillEllipse(latencyCircle);

    // Subtle border
    g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.3f));
    g.drawEllipse(latencyCircle, OCC::Design::kBorderThin);
  }

  // Heartbeat indicator (bottom light)
  {
    auto heartbeatCircle =
        juce::Rectangle<float>(xPos, yStart + lightSize + lightGap, lightSize, lightSize);

    // Pulse animation: Sawtooth with exponential decay (light fast, fade once per second)
    // Phase 0-100 represents one full 1-second cycle
    // Light instantly at phase 0, then exponentially decay to dim
    float normalizedPhase = m_heartbeatPhase / 100.0f; // 0.0 to 1.0
    float pulseAlpha =
        0.2f +
        0.7f * std::exp(-5.0f * normalizedPhase); // Exponential decay (0.2 dark → 0.9 bright)

    juce::Colour heartbeatColor = juce::Colour(OCC::Design::kAccentCyan);
    if (m_cpuPercent >= 80.0f) {
      heartbeatColor = juce::Colour(OCC::Design::kMeterRed);
    } else if (m_cpuPercent >= 50.0f) {
      heartbeatColor = juce::Colour(OCC::Design::kMeterYellow);
    }

    g.setColour(heartbeatColor.withAlpha(pulseAlpha));
    g.fillEllipse(heartbeatCircle);

    // Subtle border
    g.setColour(juce::Colour(OCC::Design::kTextPrimary).withAlpha(0.3f));
    g.drawEllipse(heartbeatCircle, OCC::Design::kBorderThin);
  }
}

void TabSwitcher::resized() {
  // Layout transport buttons on right side.
  // | [Tabs (flex space)] | [Stop All] [Panic] | [status text] [●] [●] |

  auto bounds = getLocalBounds().reduced(kLeftMargin, 0); // 10px horizontal margin

  int modeStripWidth = (NUM_OPERATOR_MODES * OPERATOR_MODE_WIDTH) +
                       ((NUM_OPERATOR_MODES - 1) * OPERATOR_MODE_GAP) + kModeStripSpacing;
  auto modeArea = bounds.removeFromLeft(modeStripWidth);

  auto placeModeButton = [](juce::TextButton* button, juce::Rectangle<int>& area) {
    if (!button)
      return;

    auto buttonBounds = area.removeFromLeft(OPERATOR_MODE_WIDTH)
                            .withSizeKeepingCentre(OPERATOR_MODE_WIDTH, OPERATOR_MODE_HEIGHT);
    button->setBounds(buttonBounds);
    area.removeFromLeft(OPERATOR_MODE_GAP);
  };

  placeModeButton(m_playoutButton.get(), modeArea);
  placeModeButton(m_editButton.get(), modeArea);
  placeModeButton(m_routingButton.get(), modeArea);
  placeModeButton(m_preferencesButton.get(), modeArea);

  int buttonWidth = 100;
  int buttonHeight = 32;
  int gap = 10;
  int statusTextWidth = 196;

  bounds.removeFromRight(22 + statusTextWidth);

  // Panic button (after status lights)
  auto panicBounds = bounds.removeFromRight(buttonWidth);
  panicBounds = panicBounds.withSizeKeepingCentre(buttonWidth, buttonHeight);
  m_panicButton->setBounds(panicBounds);

  bounds.removeFromRight(gap);

  // Stop All button (left of Panic)
  auto stopBounds = bounds.removeFromRight(buttonWidth);
  stopBounds = stopBounds.withSizeKeepingCentre(buttonWidth, buttonHeight);
  m_stopAllButton->setBounds(stopBounds);

  // Tabs are laid out in paint() dynamically (flex space on left)
  // Status lights are drawn in paint() (on the far right, outside transport buttons)
}

//==============================================================================
void TabSwitcher::mouseDown(const juce::MouseEvent& e) {
  int clickedTab = getTabAtPosition(e.x, e.y);
  if (clickedTab >= 0) {
    // OCC130 Sprint B.4: Right-click shows context menu
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
      showTabContextMenu(clickedTab);
    } else {
      setActiveTab(clickedTab);
    }
  }
}

void TabSwitcher::mouseDoubleClick(const juce::MouseEvent& e) {
  // OCC130 Sprint B.4: Double-click to rename tab
  int clickedTab = getTabAtPosition(e.x, e.y);
  if (clickedTab >= 0) {
    showRenameEditor(clickedTab);
  }
}

void TabSwitcher::mouseMove(const juce::MouseEvent& e) {
  int hoveredTab = getTabAtPosition(e.x, e.y);
  if (hoveredTab != m_hoveredTab) {
    m_hoveredTab = hoveredTab;
    repaint();
  }
}

void TabSwitcher::mouseExit(const juce::MouseEvent&) {
  if (m_hoveredTab != -1) {
    m_hoveredTab = -1;
    repaint();
  }
}

//==============================================================================
// OCC130 Sprint B.4: Tab renaming support
void TabSwitcher::showRenameEditor(int tabIndex) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS)
    return;

  // Hide existing editor if any
  hideRenameEditor();

  // Create inline text editor
  m_renameEditor = std::make_unique<juce::TextEditor>();
  m_renameEditor->setText(m_tabLabels[tabIndex]);
  m_renameEditor->selectAll();
  m_renameEditor->setBounds(getTabBounds(tabIndex).reduced(4));
  m_renameEditor->setFont(juce::FontOptions("HK Grotesk", 15.0f, juce::Font::bold));
  m_renameEditor->setJustification(juce::Justification::centred);

  // Handle Enter key to confirm
  m_renameEditor->onReturnKey = [this, tabIndex]() {
    if (m_renameEditor) {
      juce::String newLabel = m_renameEditor->getText().trim();
      if (newLabel.isNotEmpty()) {
        setTabLabel(tabIndex, newLabel);
      }
      hideRenameEditor();
    }
  };

  // Handle Esc key to cancel
  m_renameEditor->onEscapeKey = [this]() { hideRenameEditor(); };

  // Handle focus loss to cancel
  m_renameEditor->onFocusLost = [this]() { hideRenameEditor(); };

  m_editingTabIndex = tabIndex;
  addAndMakeVisible(m_renameEditor.get());
  m_renameEditor->grabKeyboardFocus();
}

void TabSwitcher::hideRenameEditor() {
  if (m_renameEditor) {
    m_renameEditor.reset();
    m_editingTabIndex = -1;
    repaint();
  }
}

void TabSwitcher::showTabContextMenu(int tabIndex) {
  if (tabIndex < 0 || tabIndex >= NUM_TABS)
    return;

  juce::PopupMenu menu;
  menu.addItem(1, "Rename Tab");
  menu.addSeparator();
  menu.addItem(2, "Clear Tab", true,
               false); // Disabled for now (requires MainComponent integration)

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, tabIndex](int result) {
    if (result == 1) {
      // Rename Tab
      showRenameEditor(tabIndex);
    } else if (result == 2) {
      // Clear Tab (TODO: Implement in MainComponent)
      // For now, just show a message
      DBG("Clear Tab " << tabIndex << " - Not implemented yet");
    }
  });
}

//==============================================================================
int TabSwitcher::getTabAtPosition(int x, int y) const {
  for (int i = 0; i < NUM_TABS; ++i) {
    if (getTabBounds(i).contains(x, y))
      return i;
  }
  return -1;
}

juce::Rectangle<int> TabSwitcher::getTabBounds(int tabIndex) const {
  if (tabIndex < 0 || tabIndex >= NUM_TABS)
    return {};

  auto bounds = getLocalBounds();

  int modeStripWidth = (NUM_OPERATOR_MODES * OPERATOR_MODE_WIDTH) +
                       ((NUM_OPERATOR_MODES - 1) * OPERATOR_MODE_GAP) + kModeStripSpacing +
                       kLeftMargin;

  // OCC130 Sprint B: Reserve space for transport controls on right
  int buttonWidth = 100;
  int gap = 10;
  int statusLightsWidth = 22;
  int statusTextWidth = 196;
  int transportWidth = 2 * (buttonWidth + gap) + statusTextWidth + statusLightsWidth + 20;
  int availableWidth = bounds.getWidth() - transportWidth - modeStripWidth - kRightMargin;

  int tabWidth = (availableWidth - (TAB_GAP * (NUM_TABS - 1))) / NUM_TABS;

  int x = modeStripWidth + tabIndex * (tabWidth + TAB_GAP);
  int y = 0;

  return juce::Rectangle<int>(x, y, tabWidth, m_tabHeight);
}
