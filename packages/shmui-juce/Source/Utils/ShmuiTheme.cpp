/*
  ==============================================================================

    ShmuiTheme.cpp
    Created: shmui JUCE Design System

    Process-wide default theme singleton. Message-thread only.

  ==============================================================================
*/

#include "ShmuiTheme.h"

namespace shmui
{

//==============================================================================
namespace
{
    // Lazily-initialised to the Lab flavor on first access. Function-local static
    // so there is no static-initialisation-order dependency on tokens.h.
    ShmuiTheme& mutableDefaultTheme()
    {
        static ShmuiTheme theme = ShmuiTheme::lab();
        return theme;
    }
}

//==============================================================================
const ShmuiTheme& defaultTheme()
{
    return mutableDefaultTheme();
}

void setDefaultTheme(const ShmuiTheme& theme)
{
    mutableDefaultTheme() = theme;
}

} // namespace shmui
