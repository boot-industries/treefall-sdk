/*
  ==============================================================================

    ShmuiTheme.cpp
    Created: shmui JUCE Design System

    Process-wide default theme singleton and message-thread listener delivery.

  ==============================================================================
*/

#include "ShmuiTheme.h"

#include "MessageThread.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace shmui {
namespace {
struct DefaultThemeState {
  ShmuiTheme theme = ShmuiTheme::lab();
  std::vector<ThemeListener*> listeners;
  std::optional<ShmuiTheme> pendingTheme;
  bool notifying = false;
};

DefaultThemeState& defaultThemeState() {
  static DefaultThemeState state;
  return state;
}

bool isThemeMessageThread() noexcept {
  return requireMessageThread();
}

bool isRegistered(const DefaultThemeState& state, const ThemeListener* listener) {
  return std::find(state.listeners.begin(), state.listeners.end(), listener) !=
         state.listeners.end();
}
} // namespace

const ShmuiTheme& defaultTheme() {
  // Returning the current value keeps this accessor usable during bootstrap,
  // while the assertion rejects reads from an established worker/audio thread.
  jassert(isMessageThread());
  return defaultThemeState().theme;
}

void addDefaultThemeListener(ThemeListener* listener) {
  if (!isThemeMessageThread() || listener == nullptr)
    return;

  auto& state = defaultThemeState();
  if (!isRegistered(state, listener))
    state.listeners.push_back(listener);
}

void removeDefaultThemeListener(ThemeListener* listener) {
  if (!isThemeMessageThread() || listener == nullptr)
    return;

  auto& state = defaultThemeState();
  const auto position = std::find(state.listeners.begin(), state.listeners.end(), listener);
  if (position != state.listeners.end())
    state.listeners.erase(position);
}

ThemeListener::~ThemeListener() {
  removeDefaultThemeListener(this);
}

void setDefaultTheme(const ShmuiTheme& theme) {
  if (!isThemeMessageThread())
    return;

  auto& state = defaultThemeState();
  if (state.notifying) {
    state.pendingTheme = theme;
    return;
  }

  state.theme = theme;
  state.notifying = true;
  for (;;) {
    // Registration order is observable. The snapshot isolates iteration from
    // callbacks that remove, destroy, or add listeners.
    const auto snapshot = state.listeners;
    for (auto* listener : snapshot) {
      if (isRegistered(state, listener))
        listener->defaultThemeChanged(state.theme);
    }

    if (!state.pendingTheme.has_value())
      break;

    state.theme = *state.pendingTheme;
    state.pendingTheme.reset();
  }
  state.notifying = false;
}

} // namespace shmui
