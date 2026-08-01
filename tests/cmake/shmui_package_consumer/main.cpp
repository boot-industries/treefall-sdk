// SPDX-License-Identifier: MIT
#include <ShmUI.h>
#include <orpheus/audio_analysis.h>
#include <orpheus/version.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace {

bool require(bool condition) {
  return condition;
}

double linear(double component) {
  return component <= 0.03928 ? component / 12.92 : std::pow((component + 0.055) / 1.055, 2.4);
}

double luminance(juce::Colour color) {
  return 0.2126 * linear(color.getFloatRed()) + 0.7152 * linear(color.getFloatGreen()) +
         0.0722 * linear(color.getFloatBlue());
}

double contrast(juce::Colour foreground, juce::Colour background) {
  const auto top = background.overlaidWith(foreground);
  const auto foregroundLuminance = luminance(top);
  const auto backgroundLuminance = luminance(background);
  return (std::max(foregroundLuminance, backgroundLuminance) + 0.05) /
         (std::min(foregroundLuminance, backgroundLuminance) + 0.05);
}

bool opaque(juce::Colour color) {
  return color.getAlpha() != 0;
}

struct RecordingListener final : shmui::ThemeListener {
  explicit RecordingListener(int identifier, std::vector<int>& events)
      : identifier(identifier), events(events) {}

  void defaultThemeChanged(const shmui::ShmuiTheme&) override {
    events.push_back(identifier);
    if (onChange)
      onChange();
  }

  int identifier;
  std::vector<int>& events;
  std::function<void()> onChange;
};

bool testAppearanceProfiles() {
  using Flavor = shmui::ThemeFlavor;
  using Mode = shmui::AppearanceMode;
  using Texture = shmui::tokens::console::MaterialTextureStyle;

  struct Expected {
    Flavor flavor;
    Mode mode;
    std::array<uint32_t, 5> material;
    Texture texture;
    float intensity;
    float scale;
  };
  constexpr std::array<Expected, 6> expected{{
      {Flavor::Console,
       Mode::Light,
       {0xFFB9C1B7, 0xFFA6B1A6, 0xFFD4D9D2, 0xFFA0ACA2, 0xFF687770},
       Texture::enamelSpeckle,
       0.045f,
       3.0f},
      {Flavor::ConsoleWarm,
       Mode::Light,
       {0xFFC8B894, 0xFFB9A475, 0xFFD9CBA9, 0xFFB39A6E, 0xFF786C4C},
       Texture::moldedABS,
       0.075f,
       18.0f},
      {Flavor::ConsoleCool,
       Mode::Light,
       {0xFFBFC3C1, 0xFFAAB2B5, 0xFFD5D9D8, 0xFFA4AFB4, 0xFF74867C},
       Texture::brushedAluminum,
       0.055f,
       2.0f},
      {Flavor::Console,
       Mode::Dark,
       {0xFF071316, 0xFF102329, 0xFF1D353B, 0xFF294950, 0xFF0D1B1D},
       Texture::powderCoat,
       0.040f,
       4.0f},
      {Flavor::ConsoleWarm,
       Mode::Dark,
       {0xFF120E0F, 0xFF21191D, 0xFF38282B, 0xFF49342B, 0xFF150F0F},
       Texture::bakeliteMottle,
       0.060f,
       24.0f},
      {Flavor::ConsoleCool,
       Mode::Dark,
       {0xFF101418, 0xFF211D21, 0xFF32281B, 0xFF493A28, 0xFF15150F},
       Texture::smoothAnodized,
       0.000f,
       0.0f},
  }};

  for (const auto& expectation : expected) {
    const auto theme = shmui::ShmuiTheme::forProfile(expectation.flavor, expectation.mode);
    const auto& material = theme.material;
    if (!require(material.chassis.getARGB() == expectation.material[0]) ||
        !require(material.panel.getARGB() == expectation.material[1]) ||
        !require(material.raised.getARGB() == expectation.material[2]) ||
        !require(material.well.getARGB() == expectation.material[3]) ||
        !require(material.inset.getARGB() == expectation.material[4]) ||
        !require(material.texture.style == expectation.texture) ||
        !require(std::abs(material.texture.intensity - expectation.intensity) < 0.0001f) ||
        !require(std::abs(material.texture.scale - expectation.scale) < 0.0001f)) {
      return false;
    }

    const auto& display = theme.display;
    const auto& controls = theme.physicalControl;
    if (!require(contrast(theme.fg, material.chassis) >= 4.5) ||
        !require(contrast(display.ink, display.surface) >= 4.5) ||
        !require(opaque(display.raised)) || !require(opaque(display.border)) ||
        !require(opaque(display.grid)) || !require(opaque(display.marker)) ||
        !require(opaque(display.selection)) || !require(opaque(controls.surface)) ||
        !require(opaque(controls.metalLight)) || !require(opaque(controls.metalMid)) ||
        !require(opaque(controls.metalDark)) || !require(controls.pressedOffset == 1) ||
        !require(controls.disabledOpacity > 0.0) || !require(opaque(theme.meter.surface)) ||
        !require(opaque(theme.meter.text)) || !require(opaque(theme.meter.tick)) ||
        !require(opaque(theme.meter.peak)) || !require(opaque(theme.wave.background)) ||
        !require(opaque(theme.wave.grid)) || !require(opaque(theme.wave.line)) ||
        !require(opaque(theme.wave.marker)) || !require(opaque(theme.wave.selection))) {
      return false;
    }
  }

  return shmui::ShmuiTheme::console().material.chassis ==
             shmui::ShmuiTheme::forProfile(Flavor::Console, Mode::Dark).material.chassis &&
         shmui::ShmuiTheme::consoleWarm().material.chassis ==
             shmui::ShmuiTheme::forProfile(Flavor::ConsoleWarm, Mode::Dark).material.chassis &&
         shmui::ShmuiTheme::consoleCool().material.chassis ==
             shmui::ShmuiTheme::forProfile(Flavor::ConsoleCool, Mode::Dark).material.chassis;
}

bool testDefaultThemeListeners() {
  using Flavor = shmui::ThemeFlavor;
  using Mode = shmui::AppearanceMode;
  const auto light = shmui::ShmuiTheme::forProfile(Flavor::Console, Mode::Light);
  const auto warm = shmui::ShmuiTheme::forProfile(Flavor::ConsoleWarm, Mode::Dark);
  const auto cool = shmui::ShmuiTheme::forProfile(Flavor::ConsoleCool, Mode::Dark);

  std::vector<int> events;
  shmui::addDefaultThemeListener(nullptr);
  {
    RecordingListener listener{1, events};
    shmui::addDefaultThemeListener(&listener);
    shmui::addDefaultThemeListener(&listener);
    shmui::setDefaultTheme(light);
    shmui::removeDefaultThemeListener(nullptr);
    shmui::removeDefaultThemeListener(&listener);
  }
  if (!require(events == std::vector<int>{1}))
    return false;

  events.clear();
  {
    auto listener = std::make_unique<RecordingListener>(2, events);
    shmui::addDefaultThemeListener(listener.get());
  }
  shmui::setDefaultTheme(warm);
  if (!require(events.empty()))
    return false;

  events.clear();
  {
    RecordingListener selfRemoving{3, events};
    selfRemoving.onChange = [&selfRemoving] { shmui::removeDefaultThemeListener(&selfRemoving); };
    shmui::addDefaultThemeListener(&selfRemoving);
    shmui::setDefaultTheme(light);
    shmui::setDefaultTheme(warm);
  }
  if (!require(events == std::vector<int>{3}))
    return false;

  events.clear();
  {
    RecordingListener first{4, events};
    RecordingListener later{5, events};
    first.onChange = [&later] { shmui::removeDefaultThemeListener(&later); };
    shmui::addDefaultThemeListener(&first);
    shmui::addDefaultThemeListener(&later);
    shmui::setDefaultTheme(light);
    shmui::removeDefaultThemeListener(&first);
  }
  if (!require(events == std::vector<int>{4}))
    return false;

  events.clear();
  {
    RecordingListener first{6, events};
    RecordingListener added{7, events};
    bool didAdd = false;
    first.onChange = [&] {
      if (!didAdd) {
        didAdd = true;
        shmui::addDefaultThemeListener(&added);
      }
    };
    shmui::addDefaultThemeListener(&first);
    shmui::setDefaultTheme(light);
    if (!require(events == std::vector<int>{6})) {
      shmui::removeDefaultThemeListener(&first);
      shmui::removeDefaultThemeListener(&added);
      return false;
    }
    shmui::setDefaultTheme(warm);
    shmui::removeDefaultThemeListener(&first);
    shmui::removeDefaultThemeListener(&added);
  }
  if (!require(events == std::vector<int>{6, 6, 7}))
    return false;

  events.clear();
  {
    RecordingListener first{8, events};
    RecordingListener second{9, events};
    RecordingListener third{10, events};
    shmui::addDefaultThemeListener(&first);
    shmui::addDefaultThemeListener(&second);
    shmui::addDefaultThemeListener(&third);
    shmui::setDefaultTheme(light);
    shmui::removeDefaultThemeListener(&first);
    shmui::removeDefaultThemeListener(&second);
    shmui::removeDefaultThemeListener(&third);
  }
  if (!require(events == std::vector<int>{8, 9, 10}))
    return false;

  events.clear();
  {
    RecordingListener reentrant{11, events};
    RecordingListener later{12, events};
    bool didReenter = false;
    reentrant.onChange = [&] {
      if (!didReenter) {
        didReenter = true;
        shmui::setDefaultTheme(cool);
      }
    };
    shmui::addDefaultThemeListener(&reentrant);
    shmui::addDefaultThemeListener(&later);
    shmui::setDefaultTheme(light);
    shmui::removeDefaultThemeListener(&reentrant);
    shmui::removeDefaultThemeListener(&later);
  }
  return require(events == std::vector<int>{11, 12, 11, 12}) &&
         require(shmui::defaultTheme().material.chassis == cool.material.chassis);
}

bool testDefaultFollowingComponents() {
  using Flavor = shmui::ThemeFlavor;
  using Mode = shmui::AppearanceMode;
  const auto light = shmui::ShmuiTheme::forProfile(Flavor::Console, Mode::Light);
  const auto dark = shmui::ShmuiTheme::forProfile(Flavor::Console, Mode::Dark);

  shmui::setDefaultTheme(light);
  shmui::Button button;
  button.setStyle(shmui::ButtonStyle::Secondary);
  shmui::LevelMeter meter;
  const auto buttonLight = button.getEffectiveColors().background;
  const auto meterLight = meter.getStyle().backgroundColor;
  shmui::setDefaultTheme(dark);
  if (!require(button.getEffectiveColors().background != buttonLight) ||
      !require(meter.getStyle().backgroundColor != meterLight)) {
    return false;
  }

  const auto customButton = shmui::ButtonColors{
      juce::Colours::magenta, juce::Colours::yellow, juce::Colours::cyan,  juce::Colours::black,
      juce::Colours::white,   juce::Colours::red,    juce::Colours::orange};
  button.setCustomColors(customButton);
  auto explicitMeter = meter.getStyle();
  explicitMeter.backgroundColor = juce::Colours::magenta;
  meter.setStyle(explicitMeter);
  shmui::setDefaultTheme(light);
  if (!require(button.getEffectiveColors().background == customButton.background) ||
      !require(meter.getStyle().backgroundColor == juce::Colours::magenta) ||
      !require(!meter.usesDefaultThemeStyle())) {
    return false;
  }

  meter.useDefaultThemeStyle();
  return require(meter.usesDefaultThemeStyle()) &&
         require(meter.getStyle().backgroundColor == light.meter.surface);
}

} // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juceGui;
  static_assert(orpheus::kSdkVersionMajor == ORPHEUS_SDK_VERSION_MAJOR);

  constexpr std::array<float, 8> impulse{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  const auto spectrum = orpheus::analysis::magnitudeSpectrum(
      impulse.data(), impulse.size(), 48000, orpheus::analysis::WindowType::Rectangular);

  return spectrum.fftSize == impulse.size() && !spectrum.magnitudes.empty() &&
                 shmui::Version::major == 2 && testAppearanceProfiles() &&
                 testDefaultThemeListeners() && testDefaultFollowingComponents()
             ? 0
             : 1;
}
