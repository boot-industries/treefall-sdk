/*
  ==============================================================================

    ShmuiTheme.h
    Created: shmui JUCE Design System

    A lightweight, overridable token surface for shmui components.

    Components are custom-painted (they do not route through juce::LookAndFeel),
    so the theme is a plain value struct rather than a LookAndFeel subclass.
    Each component's *Style struct defaults its colours from defaultTheme(), so
    a component with no per-instance overrides renders with Orpheus tokens
    (Lab flavor by default) instead of stock shadcn hex.

    Flavors mirror the design-system semantic aliases:
      - Lab           (:root default — Freqfinder deep-slate)
      - Console       ([data-flavor="console"]     — Clip Composer petrol)
      - ConsoleWarm   ([data-flavor="console-warm"] — tan-grey)
      - ConsoleCool   ([data-flavor="console-cool"] — slate-grey)

    Usage:
      // App startup (message thread), pick a flavor once:
      shmui::setDefaultTheme(shmui::ShmuiTheme::console());

      // Or inject a bespoke palette that flows into shmui widgets:
      shmui::ShmuiTheme t = shmui::ShmuiTheme::lab();
      t.accent = myBrandColour;
      shmui::setDefaultTheme(t);

  ==============================================================================
*/

#pragma once

#include "DesignTokens.h"

namespace shmui
{

//==============================================================================
/** Selects one of the design-system semantic-alias flavors. */
enum class ThemeFlavor
{
    Lab,          ///< :root default — Freqfinder deep-slate lab
    Console,      ///< [data-flavor="console"] — Clip Composer petrol
    ConsoleWarm,  ///< [data-flavor="console-warm"] — tan-grey chassis
    ConsoleCool   ///< [data-flavor="console-cool"] — slate-grey rack
};

//==============================================================================
/**
 * @brief Overridable semantic token surface.
 *
 * Carries the design-system semantic roles (`--bg`, `--fg`, `--accent`, …) for
 * a chosen flavor, plus the shared metering/waveform scales that visualizers
 * read. All values resolve to concrete colours from tokens.h so consumers never
 * touch raw hex.
 */
struct ShmuiTheme
{
    //==============================================================================
    // Surfaces (--bg / --bg-panel / --bg-card / --bg-raise / --bg-inset)
    juce::Colour bg;
    juce::Colour bgPanel;
    juce::Colour bgCard;
    juce::Colour bgRaise;
    juce::Colour bgInset;

    // Foreground + stroke (--fg / --fg-muted / --stroke)
    juce::Colour fg;
    juce::Colour fgMuted;
    juce::Colour stroke;

    // Semantic accents (--accent / --danger / --warning / --info)
    juce::Colour accent;
    juce::Colour danger;
    juce::Colour warning;
    juce::Colour info;

    // Shared metering scale (--meter-*)
    struct Meter
    {
        juce::Colour green;
        juce::Colour yellow;
        juce::Colour orange;
        juce::Colour red;
    } meter;

    // Shared waveform scale (--wave-*)
    struct Wave
    {
        juce::Colour trimIn;
        juce::Colour trimOut;
        juce::Colour playhead;
        juce::Colour line;
        juce::Colour bg;
    } wave;

    //==============================================================================
    /** Shared metering + waveform scales (identical across all flavors). */
    static Meter sharedMeter()
    {
        return { tokens::meter::green(), tokens::meter::yellow(),
                 tokens::meter::orange(), tokens::meter::red() };
    }

    static Wave sharedWave()
    {
        return { tokens::wave::trimIn(), tokens::wave::trimOut(),
                 tokens::wave::playhead(), tokens::wave::line(),
                 tokens::wave::bg() };
    }

    //==============================================================================
    /** Lab flavor — :root default (Freqfinder). */
    static ShmuiTheme lab()
    {
        ShmuiTheme t;
        t.bg       = tokens::lab::surface0();
        t.bgPanel  = tokens::lab::surface1();
        t.bgCard   = tokens::lab::surface2();
        t.bgRaise  = tokens::lab::surface3();
        t.bgInset  = tokens::lab::surface0();     // --bg-inset = --lab-surface-0
        t.fg       = tokens::lab::text();
        t.fgMuted  = tokens::lab::muted();
        t.stroke   = tokens::lab::stroke();
        t.accent   = tokens::lab::tone();
        t.danger   = tokens::lab::danger();
        t.warning  = tokens::lab::warning();
        t.info     = tokens::lab::undertoneEven();
        t.meter    = sharedMeter();
        t.wave     = sharedWave();
        return t;
    }

    /** Console flavor — [data-flavor="console"] (Clip Composer). */
    static ShmuiTheme console()
    {
        ShmuiTheme t;
        t.bg       = tokens::console::bg0();
        t.bgPanel  = tokens::console::bg1();
        t.bgCard   = tokens::console::bg2();
        t.bgRaise  = tokens::console::bg3();
        t.bgInset  = tokens::console::bgInset();
        t.fg       = tokens::console::text();
        t.fgMuted  = tokens::console::textMuted();
        t.stroke   = tokens::console::border();
        t.accent   = tokens::console::blue();
        t.danger   = tokens::console::coral();
        t.warning  = tokens::console::amber();
        t.info     = tokens::console::blue();
        t.meter    = sharedMeter();
        t.wave     = sharedWave();
        return t;
    }

    /** Warm console flavor — [data-flavor="console-warm"]. */
    static ShmuiTheme consoleWarm()
    {
        ShmuiTheme t;
        t.bg       = tokens::console::warm0();
        t.bgPanel  = tokens::console::warm1();
        t.bgCard   = tokens::console::warm2();
        t.bgRaise  = tokens::console::warm3();
        t.bgInset  = tokens::console::warmInset();
        t.fg       = tokens::console::text();
        t.fgMuted  = tokens::console::textMuted();
        t.stroke   = tokens::console::warmBorder();
        t.accent   = tokens::console::amber();
        t.danger   = tokens::console::coral();
        t.warning  = tokens::console::amberGlow();
        t.info     = tokens::console::blue();
        t.meter    = sharedMeter();
        t.wave     = sharedWave();
        return t;
    }

    /** Cool console flavor — [data-flavor="console-cool"]. */
    static ShmuiTheme consoleCool()
    {
        ShmuiTheme t;
        t.bg       = tokens::console::cool0();
        t.bgPanel  = tokens::console::cool1();
        t.bgCard   = tokens::console::cool2();
        t.bgRaise  = tokens::console::cool3();
        t.bgInset  = tokens::console::coolInset();
        t.fg       = tokens::console::text();
        t.fgMuted  = tokens::console::textMuted();
        t.stroke   = tokens::console::coolBorder();
        t.accent   = tokens::console::blue();
        t.danger   = tokens::console::coral();
        t.warning  = tokens::console::amber();
        t.info     = tokens::console::blue();
        t.meter    = sharedMeter();
        t.wave     = sharedWave();
        return t;
    }

    /** Build a theme for the given flavor. */
    static ShmuiTheme forFlavor(ThemeFlavor flavor)
    {
        switch (flavor)
        {
            case ThemeFlavor::Console:     return console();
            case ThemeFlavor::ConsoleWarm: return consoleWarm();
            case ThemeFlavor::ConsoleCool: return consoleCool();
            case ThemeFlavor::Lab:
            default:                       return lab();
        }
    }
};

//==============================================================================
/**
 * @brief The process-wide default theme (message-thread only).
 *
 * Components read this when they have no per-instance override, so the whole
 * library is on-brand out of the box. Defaults to the Lab flavor. Apps may call
 * setDefaultTheme() once at startup to switch flavors or inject a custom palette.
 *
 * Not thread-safe by design — call from the JUCE message thread, consistent with
 * the visualization components' threading model.
 */
const ShmuiTheme& defaultTheme();
void setDefaultTheme(const ShmuiTheme& theme);

} // namespace shmui
