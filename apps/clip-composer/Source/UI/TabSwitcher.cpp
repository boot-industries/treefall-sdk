// SPDX-License-Identifier: MIT

#include "TabSwitcher.h"

//==============================================================================
TabSwitcher::TabSwitcher()
{
    // Initialize default tab labels
    const char* defaultLabels[NUM_TABS] = {
        "Tab 1", "Tab 2", "Tab 3", "Tab 4",
        "Tab 5", "Tab 6", "Tab 7", "Tab 8"
    };
    for (int i = 0; i < NUM_TABS; ++i)
    {
        m_tabLabels.add(juce::String(defaultLabels[i]));
    }

    setSize(800, TAB_HEIGHT);
}

//==============================================================================
void TabSwitcher::setActiveTab(int tabIndex)
{
    if (tabIndex >= 0 && tabIndex < NUM_TABS && tabIndex != m_activeTab)
    {
        m_activeTab = tabIndex;
        repaint();

        // Notify listeners
        if (onTabSelected)
            onTabSelected(m_activeTab);
    }
}

void TabSwitcher::setTabLabel(int tabIndex, const juce::String& label)
{
    if (tabIndex >= 0 && tabIndex < NUM_TABS)
    {
        m_tabLabels.set(tabIndex, label);
        repaint();
    }
}

juce::String TabSwitcher::getTabLabel(int tabIndex) const
{
    if (tabIndex >= 0 && tabIndex < NUM_TABS)
        return m_tabLabels[tabIndex];
    return "";
}

//==============================================================================
void TabSwitcher::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));  // Very dark grey

    // Draw tabs
    for (int i = 0; i < NUM_TABS; ++i)
    {
        auto tabBounds = getTabBounds(i);

        // Determine tab colors based on state
        juce::Colour tabColor;
        juce::Colour textColor;

        if (i == m_activeTab)
        {
            // Active tab - bright highlight
            tabColor = juce::Colour(0xff2a9d8f);  // Teal
            textColor = juce::Colours::white;
        }
        else if (i == m_hoveredTab)
        {
            // Hovered tab - subtle highlight
            tabColor = juce::Colour(0xff2a2a2a);  // Light grey
            textColor = juce::Colour(0xffcccccc);
        }
        else
        {
            // Inactive tab - dark
            tabColor = juce::Colour(0xff1e1e1e);  // Dark grey
            textColor = juce::Colour(0xff888888);  // Medium grey
        }

        // Draw tab background
        g.setColour(tabColor);
        g.fillRoundedRectangle(tabBounds.toFloat(), 4.0f);

        // Draw tab border (subtle)
        if (i == m_activeTab)
        {
            g.setColour(juce::Colour(0xff3ab7a8));  // Lighter teal
            g.drawRoundedRectangle(tabBounds.toFloat(), 4.0f, 2.0f);
        }

        // Draw tab label (larger, centered)
        g.setColour(textColor);
        g.setFont(juce::FontOptions("Inter", 15.0f, juce::Font::bold));
        g.drawText(m_tabLabels[i], tabBounds, juce::Justification::centred);

        // Draw keyboard shortcut hint in top-right corner (subtle)
        g.setFont(juce::FontOptions("Inter", 9.0f, juce::Font::plain));
        g.setColour(textColor.withAlpha(0.4f));
        juce::String shortcut("⌘");
        shortcut << (i + 1);
        auto shortcutArea = tabBounds.reduced(6, 4).removeFromTop(12).removeFromRight(18);
        g.drawText(shortcut, shortcutArea, juce::Justification::centredRight);
    }
}

void TabSwitcher::resized()
{
    // Tabs are laid out in paint() dynamically
}

//==============================================================================
void TabSwitcher::mouseDown(const juce::MouseEvent& e)
{
    int clickedTab = getTabAtPosition(e.x, e.y);
    if (clickedTab >= 0)
    {
        setActiveTab(clickedTab);
    }
}

void TabSwitcher::mouseMove(const juce::MouseEvent& e)
{
    int hoveredTab = getTabAtPosition(e.x, e.y);
    if (hoveredTab != m_hoveredTab)
    {
        m_hoveredTab = hoveredTab;
        repaint();
    }
}

void TabSwitcher::mouseExit(const juce::MouseEvent&)
{
    if (m_hoveredTab != -1)
    {
        m_hoveredTab = -1;
        repaint();
    }
}

//==============================================================================
int TabSwitcher::getTabAtPosition(int x, int y) const
{
    for (int i = 0; i < NUM_TABS; ++i)
    {
        if (getTabBounds(i).contains(x, y))
            return i;
    }
    return -1;
}

juce::Rectangle<int> TabSwitcher::getTabBounds(int tabIndex) const
{
    if (tabIndex < 0 || tabIndex >= NUM_TABS)
        return {};

    auto bounds = getLocalBounds();
    int totalWidth = bounds.getWidth();
    int tabWidth = (totalWidth - (TAB_GAP * (NUM_TABS - 1))) / NUM_TABS;

    int x = tabIndex * (tabWidth + TAB_GAP);
    int y = 0;

    return juce::Rectangle<int>(x, y, tabWidth, TAB_HEIGHT);
}
