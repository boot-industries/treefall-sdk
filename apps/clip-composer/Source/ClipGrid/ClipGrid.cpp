// SPDX-License-Identifier: MIT

#include "ClipGrid.h"

//==============================================================================
ClipGrid::ClipGrid()
{
    createButtons();
}

//==============================================================================
void ClipGrid::createButtons()
{
    m_buttons.clear();

    // Create 48 buttons (6×8)
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        auto button = std::make_unique<ClipButton>(i);

        // Wire up callbacks
        button->onClick = [this](int index) { handleButtonLeftClick(index); };
        button->onRightClick = [this](int index) { handleButtonRightClick(index); };

        // All buttons start empty - clips will be loaded by SessionManager
        addAndMakeVisible(button.get());
        m_buttons.push_back(std::move(button));
    }
}

ClipButton* ClipGrid::getButton(int index)
{
    if (index >= 0 && index < BUTTON_COUNT)
        return m_buttons[static_cast<size_t>(index)].get();
    return nullptr;
}

//==============================================================================
void ClipGrid::handleButtonLeftClick(int buttonIndex)
{
    DBG("ClipGrid: Button " + juce::String(buttonIndex) + " left-clicked");

    // Forward to MainComponent via callback
    if (onButtonClicked)
        onButtonClicked(buttonIndex);
}

void ClipGrid::handleButtonRightClick(int buttonIndex)
{
    DBG("ClipGrid: Button " + juce::String(buttonIndex) + " right-clicked");

    // Forward to MainComponent via callback
    if (onButtonRightClicked)
        onButtonRightClicked(buttonIndex);
}

//==============================================================================
void ClipGrid::paint(juce::Graphics& g)
{
    // Grid background
    g.fillAll(juce::Colour(0xff1a1a1a));  // Very dark grey
}

void ClipGrid::resized()
{
    auto bounds = getLocalBounds();

    // Calculate button size based on grid dimensions
    int availableWidth = bounds.getWidth() - (GAP * (COLUMNS + 1));
    int availableHeight = bounds.getHeight() - (GAP * (ROWS + 1));

    int buttonWidth = availableWidth / COLUMNS;
    int buttonHeight = availableHeight / ROWS;

    // Layout buttons in grid
    for (int row = 0; row < ROWS; ++row)
    {
        for (int col = 0; col < COLUMNS; ++col)
        {
            int index = row * COLUMNS + col;
            auto button = getButton(index);

            if (button)
            {
                int x = GAP + col * (buttonWidth + GAP);
                int y = GAP + row * (buttonHeight + GAP);

                button->setBounds(x, y, buttonWidth, buttonHeight);
            }
        }
    }
}
