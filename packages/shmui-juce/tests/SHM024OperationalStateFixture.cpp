#include "Utils/ShmuiTheme.h"

#include <array>
#include <cstdio>
#include <utility>

namespace {
int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "SHM024 failure: %s\n", message);
  }
}

bool sameRole(const shmui::tokens::console::OperationalRole& left,
              const shmui::tokens::console::OperationalRole& right) {
  return left.foreground == right.foreground && left.surface == right.surface &&
         left.border == right.border && left.focusVisible == right.focusVisible;
}

bool samePalette(const shmui::tokens::console::OperationalPalette& left,
                 const shmui::tokens::console::OperationalPalette& right) {
  return sameRole(left.idle, right.idle) && sameRole(left.selected, right.selected) &&
         sameRole(left.armed, right.armed) && sameRole(left.cued, right.cued) &&
         sameRole(left.live, right.live) && sameRole(left.playing, right.playing) &&
         sameRole(left.recording, right.recording) && sameRole(left.stopping, right.stopping) &&
         sameRole(left.destructive, right.destructive) && sameRole(left.warning, right.warning) &&
         sameRole(left.health, right.health) && sameRole(left.unavailable, right.unavailable) &&
         sameRole(left.disabled, right.disabled);
}

void requireFocusVisible(const shmui::tokens::console::OperationalPalette& palette,
                         const char* message) {
  const auto expected = shmui::tokens::console::amberGlow();
  require(palette.idle.focusVisible == expected, message);
  require(palette.selected.focusVisible == expected, message);
  require(palette.armed.focusVisible == expected, message);
  require(palette.cued.focusVisible == expected, message);
  require(palette.live.focusVisible == expected, message);
  require(palette.playing.focusVisible == expected, message);
  require(palette.recording.focusVisible == expected, message);
  require(palette.stopping.focusVisible == expected, message);
  require(palette.destructive.focusVisible == expected, message);
  require(palette.warning.focusVisible == expected, message);
  require(palette.health.focusVisible == expected, message);
  require(palette.unavailable.focusVisible == expected, message);
  require(palette.disabled.focusVisible == expected, message);
}
} // namespace

int main() {
  using namespace shmui::tokens::console;

  constexpr std::array expectedPrecedence{
      OperationalState::unavailable, OperationalState::disabled, OperationalState::recording,
      OperationalState::destructive, OperationalState::stopping, OperationalState::live,
      OperationalState::playing,     OperationalState::armed,    OperationalState::cued,
      OperationalState::selected,    OperationalState::press,    OperationalState::hover,
      OperationalState::idle,
  };
  require(operationalStatePrecedence == expectedPrecedence,
          "operational precedence matches the bounded grammar");

  const auto consoleLight = forProfile(Flavor::Console, AppearanceMode::Light);
  const auto consoleDark = forProfile(Flavor::Console, AppearanceMode::Dark);
  const auto warmLight = forProfile(Flavor::ConsoleWarm, AppearanceMode::Light);
  const auto warmDark = forProfile(Flavor::ConsoleWarm, AppearanceMode::Dark);
  const auto coolLight = forProfile(Flavor::ConsoleCool, AppearanceMode::Light);
  const auto coolDark = forProfile(Flavor::ConsoleCool, AppearanceMode::Dark);
  const std::array profiles{consoleLight, consoleDark, warmLight, warmDark, coolLight, coolDark};

  for (const auto& profile : profiles) {
    require(sameRole(profile.operations.unavailable, profile.operations.disabled),
            "unavailable and disabled retain equivalent visual treatment");
    require(sameRole(profile.operations.cued, profile.operations.armed),
            "cued retains the armed visual alias");
    require(sameRole(profile.operations.playing, profile.operations.live),
            "playing retains the live visual alias");
    requireFocusVisible(profile.operations, "focus remains visible on every operational role");
  }

  const std::array flavorPairs{
      std::pair{shmui::ThemeFlavor::Console, Flavor::Console},
      std::pair{shmui::ThemeFlavor::ConsoleWarm, Flavor::ConsoleWarm},
      std::pair{shmui::ThemeFlavor::ConsoleCool, Flavor::ConsoleCool},
  };
  for (const auto& [themeFlavor, tokenFlavor] : flavorPairs) {
    for (const auto mode : {shmui::AppearanceMode::Light, shmui::AppearanceMode::Dark}) {
      const auto tokenMode =
          mode == shmui::AppearanceMode::Light ? AppearanceMode::Light : AppearanceMode::Dark;
      const auto expected = forProfile(tokenFlavor, tokenMode);
      const auto actual = shmui::ShmuiTheme::forProfile(themeFlavor, mode);
      require(samePalette(actual.operations, expected.operations),
              "ShmuiTheme propagates the selected operational palette");
    }
  }

  const auto lab = shmui::ShmuiTheme::lab();
  require(samePalette(lab.operations, consoleDark.operations),
          "Lab fallback carries the Console dark operational palette");

  return failures == 0 ? 0 : 1;
}
