/*
  ==============================================================================

    ShmuiTheme.h
    Created: shmui JUCE Design System

    A lightweight, overridable token surface for shmui components.

    Components are custom-painted (they do not route through juce::LookAndFeel),
    so the theme is a plain value struct rather than a LookAndFeel subclass.

  ==============================================================================
*/

#pragma once

#include "DesignTokens.h"

namespace shmui {

//==============================================================================
/** Selects one of the design-system semantic-alias flavors. */
enum class ThemeFlavor {
  Lab,         ///< :root default — Freqfinder deep-slate lab
  Console,     ///< [data-flavor="console"] — Clip Composer petrol
  ConsoleWarm, ///< [data-flavor="console-warm"] — tan-grey chassis
  ConsoleCool  ///< [data-flavor="console-cool"] — slate-grey rack
};

/** Selects the color mode independently from a Console flavor. */
enum class AppearanceMode { Light, Dark };

//==============================================================================
/**
 * Overridable semantic token surface. Legacy flavor factories retain their
 * dark behavior; forProfile() exposes the full mode × flavor matrix.
 */
struct ShmuiTheme {
  juce::Colour bg;
  juce::Colour bgPanel;
  juce::Colour bgCard;
  juce::Colour bgRaise;
  juce::Colour bgInset;

  juce::Colour fg;
  juce::Colour fgMuted;
  juce::Colour stroke;

  juce::Colour accent;
  juce::Colour danger;
  juce::Colour warning;
  juce::Colour info;

  struct Meter {
    juce::Colour green;
    juce::Colour yellow;
    juce::Colour orange;
    juce::Colour red;
    juce::Colour surface;
    juce::Colour text;
    juce::Colour tick;
    juce::Colour peak;
  } meter;

  struct Wave {
    juce::Colour trimIn;
    juce::Colour trimOut;
    juce::Colour playhead;
    juce::Colour line;
    juce::Colour bg;
    juce::Colour background;
    juce::Colour grid;
    juce::Colour marker;
    juce::Colour selection;
  } wave;

  using Material = tokens::console::MaterialRecipe;
  using Illumination = tokens::console::IlluminationRecipe;
  using Display = tokens::console::DisplayPresentation;
  using PhysicalControl = tokens::console::PhysicalControlRecipe;
  using Operations = tokens::console::OperationalPalette;

  Material material;
  Illumination illumination;
  Display display;
  Operations operations;
  PhysicalControl physicalControl;

  static Meter sharedMeter() {
    return {tokens::meter::green(), tokens::meter::yellow(),     tokens::meter::orange(),
            tokens::meter::red(),   tokens::meter::surfaceLab(), tokens::lab::text(),
            tokens::lab::stroke(),  tokens::meter::peakLab()};
  }

  static Wave sharedWave() {
    const auto background = tokens::wave::bg();
    return {tokens::wave::trimIn(),
            tokens::wave::trimOut(),
            tokens::wave::playhead(),
            tokens::wave::line(),
            background,
            background,
            tokens::lab::stroke(),
            tokens::wave::playhead(),
            tokens::lab::tone()};
  }

  /** Lab flavor remains the existing dark-first default. */
  static ShmuiTheme lab() {
    ShmuiTheme t;
    t.bg = tokens::lab::surface0();
    t.bgPanel = tokens::lab::surface1();
    t.bgCard = tokens::lab::surface2();
    t.bgRaise = tokens::lab::surface3();
    t.bgInset = tokens::lab::surface0();
    t.fg = tokens::lab::text();
    t.fgMuted = tokens::lab::muted();
    t.stroke = tokens::lab::stroke();
    t.accent = tokens::lab::tone();
    t.danger = tokens::lab::danger();
    t.warning = tokens::lab::warning();
    t.info = tokens::lab::undertoneEven();
    t.meter = sharedMeter();
    t.wave = sharedWave();

    // Lab has no physical Console profile. Keep the semantic member surface
    // total by supplying the canonical Console dark recipe as a safe default.
    const auto fallback = tokens::console::forProfile(tokens::console::Flavor::Console,
                                                      tokens::console::AppearanceMode::Dark);
    t.material = fallback.material;
    t.illumination = fallback.illumination;
    t.operations = fallback.operations;
    t.display = fallback.display;
    t.physicalControl = fallback.physicalControl;
    return t;
  }

  static ShmuiTheme forProfile(ThemeFlavor flavor, AppearanceMode mode) {
    if (flavor == ThemeFlavor::Lab)
      return lab();

    auto profileFlavor = tokens::console::Flavor::Console;
    switch (flavor) {
    case ThemeFlavor::ConsoleWarm:
      profileFlavor = tokens::console::Flavor::ConsoleWarm;
      break;
    case ThemeFlavor::ConsoleCool:
      profileFlavor = tokens::console::Flavor::ConsoleCool;
      break;
    case ThemeFlavor::Console:
    case ThemeFlavor::Lab:
      break;
    }

    const auto profile = tokens::console::forProfile(
        profileFlavor, mode == AppearanceMode::Light ? tokens::console::AppearanceMode::Light
                                                     : tokens::console::AppearanceMode::Dark);

    ShmuiTheme t;
    t.bg = profile.material.chassis;
    t.bgPanel = profile.material.panel;
    t.bgCard = profile.material.raised;
    t.bgRaise = profile.material.well;
    t.bgInset = profile.material.inset;
    t.fg = profile.text;
    t.fgMuted = profile.textMuted;
    t.stroke = profile.material.seam;
    t.accent = tokens::console::blue();
    t.danger = profile.illumination.fault.core;
    t.warning = profile.illumination.attention.core;
    t.info = tokens::console::blue();
    t.meter = {tokens::meter::safe(), tokens::meter::caution(), tokens::meter::warning(),
               tokens::meter::clip(), profile.meter.surface,    profile.meter.text,
               profile.meter.tick,    profile.meter.peak};
    t.wave = {tokens::wave::trimIn(), tokens::wave::trimOut(),     tokens::wave::playhead(),
              profile.waveform.line,  profile.waveform.background, profile.waveform.background,
              profile.waveform.grid,  profile.waveform.marker,     profile.waveform.selection};
    t.material = profile.material;
    t.operations = profile.operations;
    t.illumination = profile.illumination;
    t.display = profile.display;
    t.physicalControl = profile.physicalControl;
    return t;
  }

  /** Existing startup factory; deliberately remains the dark Console profile.
   */
  static ShmuiTheme console() {
    return forProfile(ThemeFlavor::Console, AppearanceMode::Dark);
  }

  /** Existing startup factory; deliberately remains the dark warm profile. */
  static ShmuiTheme consoleWarm() {
    return forProfile(ThemeFlavor::ConsoleWarm, AppearanceMode::Dark);
  }

  /** Existing startup factory; deliberately remains the dark cool profile. */
  static ShmuiTheme consoleCool() {
    return forProfile(ThemeFlavor::ConsoleCool, AppearanceMode::Dark);
  }

  /** Existing startup factory; deliberately retains dark behavior. */
  static ShmuiTheme forFlavor(ThemeFlavor flavor) {
    return forProfile(flavor, AppearanceMode::Dark);
  }
};

class ThemeListener;

/** Message-thread-only, non-owning registration. Null and duplicate adds are
 * no-ops. */
void addDefaultThemeListener(ThemeListener* listener);
/** Message-thread-only, non-owning removal. Null and missing removes are
 * no-ops. */
void removeDefaultThemeListener(ThemeListener* listener);

/**
 * Receives a completed default-theme change. Destruction unregisters the
 * object; concrete default-following components also unregister at destructor
 * entry.
 */
class ThemeListener {
public:
  virtual ~ThemeListener();
  virtual void defaultThemeChanged(const ShmuiTheme&) = 0;
};

//==============================================================================
/**
 * The process-wide default theme. All access and listener registration is on
 * the JUCE message thread; no audio-thread state or lock is involved.
 */
const ShmuiTheme& defaultTheme();
void setDefaultTheme(const ShmuiTheme& theme);

} // namespace shmui
