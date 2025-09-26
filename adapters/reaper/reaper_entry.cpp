#include "panel.h"

#include "orpheus/abi.h"
#include "orpheus/export.h"

#include <mutex>
#include <string>

namespace {

std::once_flag gInitFlag;
std::string gPanelCache;

void InitializePanel() {
  std::call_once(gInitFlag, [] {
    gPanelCache = orpheus::reaper::BuildPanelText();
  });
}

}  // namespace

extern "C" ORPHEUS_API const char *ReaperExtensionName() {
  return orpheus::reaper::PanelTitle();
}

extern "C" ORPHEUS_API const char *ReaperExtensionVersion() {
  static std::string version = "ABI " + orpheus::ToString(orpheus::kCurrentAbi);
  return version.c_str();
}

extern "C" ORPHEUS_API const char *ReaperExtensionPanelText() {
  InitializePanel();
  return gPanelCache.c_str();
}
