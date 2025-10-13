// SPDX-License-Identifier: MIT

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "Audio/AudioEngine.h"
#include "ClipGrid/ClipGrid.h"
#include "Session/SessionManager.h"
#include "Transport/TransportControls.h"
#include "UI/InterLookAndFeel.h"
#include "UI/TabSwitcher.h"

//==============================================================================
/**
 * Main UI component for Orpheus Clip Composer
 *
 * This is the top-level component that hosts all UI elements:
 * - Clip Grid (48 buttons MVP, 960 buttons full version: 10×12 × 8 tabs)
 * - Transport Controls
 * - Routing Panel
 * - Waveform Display
 * - Performance Monitor
 *
 * Threading Model:
 * - Runs on JUCE Message Thread (UI thread)
 * - Communicates with audio thread via lock-free commands
 * - Never blocks the audio thread
 */
class MainComponent : public juce::Component,
                      public juce::MenuBarModel
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==============================================================================
    // MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    //==============================================================================
    // Core Functionality
    void onClipRightClicked(int buttonIndex);
    void onClipTriggered(int buttonIndex);  // Trigger clip (keyboard or mouse)
    void loadClipToButton(int buttonIndex, const juce::String& filePath);
    void updateButtonFromClip(int buttonIndex);
    void onStopAll();
    void onPanic();

    // Tab management
    void onTabSelected(int tabIndex);

    // Keyboard mapping
    int getButtonIndexFromKey(const juce::KeyPress& key) const;
    juce::String getKeyboardShortcutForButton(int buttonIndex) const;

    //==============================================================================
    // UI Components
    std::unique_ptr<TabSwitcher> m_tabSwitcher;
    std::unique_ptr<ClipGrid> m_clipGrid;
    std::unique_ptr<TransportControls> m_transportControls;

    // Future components
    // std::unique_ptr<RoutingPanel> m_routingPanel;
    // std::unique_ptr<WaveformDisplay> m_waveformDisplay;

    // SDK Integration (Active!)
    std::unique_ptr<AudioEngine> m_audioEngine;

    // Session Management (Real Functionality)
    SessionManager m_sessionManager;

    // Custom Look and Feel (Inter font)
    InterLookAndFeel m_interLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
