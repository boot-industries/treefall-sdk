/*
  ==============================================================================

    JuceHeader.h

    JUCE module includes for the shmui-juce library. This aggregate header
    provides compatibility for the shmui sources (which #include <JuceHeader.h>,
    Projucer-style) when the package is consumed as a plain CMake library via
    add_subdirectory() rather than through a juce_add_* target that generates
    its own JuceHeader.

    This file lives on the shmui target's PUBLIC include path (SHMUI_SRC). It was
    present through v0.3.0, dropped in the v0.3.1 CMake restructure (which broke
    add_subdirectory consumers with "'JuceHeader.h' file not found"), and
    restored here.

    juce_opengl is included only when the OpenGL OrbVisualizer is enabled
    (SHMUI_JUCE_ENABLE_OPENGL); GL-free consumers do not pull the module.

  ==============================================================================
*/

#pragma once

// JUCE core modules
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#if defined(SHMUI_JUCE_ENABLE_OPENGL) && SHMUI_JUCE_ENABLE_OPENGL
#include <juce_opengl/juce_opengl.h>
#endif
