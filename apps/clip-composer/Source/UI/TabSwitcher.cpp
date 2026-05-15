// SPDX-License-Identifier: MIT

#include "TabSwitcher.h"
#include "ConsoleTheme.h"
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
  // Timer fires every 10ms, phase increments 0-100 in 1 second (100 steps x 10ms = 1000ms)
  startTimer(10); // 10ms intervals for smooth 1Hz pulse animation

  setSize(800, m_tabHeight);
  updateModeButtonStates();
}

void TabSwitcher::setOperatorViewMode(occ::ui::OperatorViewMode mode) {
  m_operatorViewMode = mode;
  updateModeButtonStates();
  resized();
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
  auto bounds = getLocalBounds().toFloat();
  const bool livePlayout = m_operatorViewMode == occ::ui::OperatorViewMode::Playout;

  OCC::Console::fillVerticalGradient(g, bounds, juce::Colour(0xff1f2125), juce::Colour(0xff171a1d));
  g.setColour(juce::Colour(OCC::Design::kBorderDefault));
  g.drawLine(0.0f, bounds.getBottom() - 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 1.0f);

  if (livePlayout) {
    auto logo = juce::Rectangle<float>(10.0f, 7.0f, 22.0f, 22.0f);
    OCC::Console::drawMatteCap(g, logo, juce::Colour(OCC::Design::kBgComponent),
                               juce::Colour(OCC::Design::kBgInset), 3.0f);
    g.setFont(OCC::Console::consoleFont(11.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kAmber));
    g.drawText("CC", logo.toNearestInt(), juce::Justification::centred, false);

    g.setFont(OCC::Console::monoFont(10.0f, juce::Font::plain));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText("UNTITLED.OCC  .  o", 40, 0, 196, getHeight(), juce::Justification::centredLeft,
               false);
  } else {
    auto logo = juce::Rectangle<float>(16.0f, 11.0f, 26.0f, 26.0f);
    OCC::Console::drawMatteCap(g, logo, juce::Colour(OCC::Design::kBgComponent),
                               juce::Colour(OCC::Design::kBgInset), 4.0f);
    g.setFont(OCC::Console::consoleFont(14.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kAmber));
    g.drawText("CC", logo.toNearestInt(), juce::Justification::centred, false);
    g.setFont(OCC::Console::consoleFont(16.0f, juce::Font::bold));
    g.setColour(juce::Colour(OCC::Design::kTextPrimary));
    g.drawText("Orpheus Clip Composer", 52, 0, 260, 48, juce::Justification::centredLeft, false);
    g.setFont(OCC::Console::monoFont(10.0f));
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText("v0.2.2-alpha", 246, 0, 100, 48, juce::Justification::centredLeft, false);

    g.setColour(juce::Colour(OCC::Design::kBgSecondary));
    g.fillRect(0, 48, getWidth(), 40);
    g.setColour(juce::Colour(OCC::Design::kBorderDefault));
    g.drawLine(0, 48, getWidth(), 48, 1.0f);
  }

  // Draw tabs
  for (int i = 0; i < NUM_TABS; ++i) {
    auto tabBounds = getTabBounds(i).toFloat();

    const bool active = i == m_activeTab;
    const bool hovered = i == m_hoveredTab;
    auto top = active ? juce::Colour(OCC::Design::kNeveBlue).brighter(0.10f)
                      : juce::Colour(OCC::Design::kBgInset).brighter(hovered ? 0.07f : 0.0f);
    auto bottom = active ? juce::Colour(OCC::Design::kNeveBlueDark)
                         : juce::Colour(OCC::Design::kBgInset).darker(0.04f);
    OCC::Console::drawMatteCap(g, tabBounds, top, bottom, livePlayout ? 3.0f : 4.0f);

    g.setColour(active ? juce::Colours::white : juce::Colour(OCC::Design::kTextSecondary));
    g.setFont(OCC::Console::consoleFont(livePlayout ? 11.0f : 12.0f, juce::Font::bold));
    const auto label = livePlayout ? juce::String(i + 1).paddedLeft('0', 2)
                                   : juce::String(i + 1).paddedLeft('0', 2) + " (" +
                                         juce::String(i * occ::BUTTONS_PER_TAB + 1) + "-" +
                                         juce::String((i + 1) * occ::BUTTONS_PER_TAB) + ")";
    g.drawText(label, tabBounds.toNearestInt(), juce::Justification::centred, false);
  }

  // Health glance / status strip.
  auto statusArea = livePlayout ? juce::Rectangle<int>(getWidth() - 380, 0, 256, getHeight())
                                : juce::Rectangle<int>(12, 48, getWidth() - 24, 40);
  g.setFont(OCC::Console::monoFont(10.0f, juce::Font::plain));
  if (livePlayout) {
    g.setColour(juce::Colour(OCC::Design::kMeterGreen));
    g.fillEllipse(statusArea.getX(), 14, 7, 7);
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    g.drawText(juce::String(m_latencyMs, 1) + "ms  \xE2\x80\xA2  " + juce::String(m_cpuPercent, 0) +
                   "%   ",
               statusArea, juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(OCC::Design::kTextPrimary));
    g.drawText(m_sampleRate > 0
                   ? juce::String(m_sampleRate / 1000) + "k/" + juce::String(m_bufferSize)
                   : "NO I/O",
               statusArea, juce::Justification::centredRight, false);
  } else {
    g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    juce::String health = "LATENCY  ";
    health += juce::String(m_latencyMs, 1) + " ms   |   CPU  " + juce::String(m_cpuPercent, 0) +
              " %   |   MEM  " + juce::String(m_memoryMB) + " MB   |   DROPS  " +
              juce::String(m_dropoutCount) + "   |   DEVICE  " +
              (m_deviceSummary.isNotEmpty() ? m_deviceSummary : juce::String("Default Device"));
    g.drawText(health, statusArea, juce::Justification::centredLeft, false);
  }

  // Draw operator mode switcher.
  for (int i = 0; i < NUM_OPERATOR_MODES; ++i) {
    auto mode = getOperatorModeFromIndex(i);
    auto b = getOperatorModeBounds(i).toFloat();
    if (b.isEmpty())
      continue;
    const bool active = mode == m_operatorViewMode;
    if (active) {
      OCC::Console::drawMatteCap(g, b, juce::Colour(OCC::Design::kNeveBlue).brighter(0.08f),
                                 juce::Colour(OCC::Design::kNeveBlueDark), 3.0f);
      g.setColour(juce::Colours::white);
    } else {
      g.setColour(juce::Colour(OCC::Design::kTextSecondary));
    }
    g.setFont(OCC::Console::consoleFont(livePlayout ? 10.0f : 11.0f, juce::Font::bold));
    g.drawText(OCC::Console::operatorModeLabel(mode), b.toNearestInt(),
               juce::Justification::centred, false);
  }
}

void TabSwitcher::resized() {
  if (m_stopAllButton)
    m_stopAllButton->setVisible(false);
  if (m_panicButton)
    m_panicButton->setVisible(false);
  if (m_playoutButton)
    m_playoutButton->setVisible(false);
  if (m_editButton)
    m_editButton->setVisible(false);
  if (m_routingButton)
    m_routingButton->setVisible(false);
  if (m_preferencesButton)
    m_preferencesButton->setVisible(false);
}

//==============================================================================
void TabSwitcher::mouseDown(const juce::MouseEvent& e) {
  for (int i = 0; i < NUM_OPERATOR_MODES; ++i) {
    if (getOperatorModeBounds(i).contains(e.position.toInt())) {
      auto mode = getOperatorModeFromIndex(i);
      setOperatorViewMode(mode);
      if (onOperatorViewModeSelected)
        onOperatorViewModeSelected(mode);
      return;
    }
  }

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

  const bool livePlayout = m_operatorViewMode == occ::ui::OperatorViewMode::Playout;
  if (livePlayout) {
    const int tabWidth = 28;
    const int x = 252 + tabIndex * (tabWidth + TAB_GAP);
    return {x, 7, tabWidth, 22};
  }

  const int y = 56;
  const int tabWidth = 96;
  const int x = 10 + tabIndex * (tabWidth + TAB_GAP);
  return {x, y, tabWidth, 28};
}

juce::String TabSwitcher::getOperatorViewModeLabel(occ::ui::OperatorViewMode mode) const {
  return OCC::Console::operatorModeLabel(mode);
}

int TabSwitcher::getOperatorModeIndex(occ::ui::OperatorViewMode mode) const {
  return static_cast<int>(mode);
}

occ::ui::OperatorViewMode TabSwitcher::getOperatorModeFromIndex(int index) const {
  switch (index) {
  case 0:
    return occ::ui::OperatorViewMode::Playout;
  case 1:
    return occ::ui::OperatorViewMode::Edit;
  case 2:
    return occ::ui::OperatorViewMode::Routing;
  case 3:
    return occ::ui::OperatorViewMode::Preferences;
  default:
    return occ::ui::OperatorViewMode::Playout;
  }
}

juce::Rectangle<int> TabSwitcher::getOperatorModeBounds(int modeIndex) const {
  if (modeIndex < 0 || modeIndex >= NUM_OPERATOR_MODES)
    return {};

  const bool livePlayout = m_operatorViewMode == occ::ui::OperatorViewMode::Playout;
  const int widths[NUM_OPERATOR_MODES] = {82, 56, 76, 110};
  const int visibleModes = livePlayout ? 3 : NUM_OPERATOR_MODES;
  if (modeIndex >= visibleModes)
    return {};

  int totalWidth = 0;
  for (int i = 0; i < visibleModes; ++i)
    totalWidth += widths[i] + (i > 0 ? 6 : 0);

  int x = getWidth() - totalWidth - 10;
  for (int i = 0; i < modeIndex; ++i)
    x += widths[i] + 6;

  const int y = livePlayout ? 7 : 10;
  const int h = livePlayout ? 22 : 28;
  return {x, y, widths[modeIndex], h};
}
