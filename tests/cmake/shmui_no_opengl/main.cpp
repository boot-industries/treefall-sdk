// SPDX-License-Identifier: MIT
#include <ShmUI.h>

#if defined(SHMUI_JUCE_ENABLE_OPENGL) && SHMUI_JUCE_ENABLE_OPENGL
#error "The non-OpenGL ShmUI consumer unexpectedly enabled OpenGL"
#endif

int main() {
  shmui::LevelMeter meter(2);
  meter.setLevel(0, 0.25f);
  meter.setLevel(1, 0.5f);
  return shmui::Version::major == 2 ? 0 : 1;
}
