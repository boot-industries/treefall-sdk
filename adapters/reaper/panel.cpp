#include "panel.h"

#include "orpheus/abi.h"

#include <sstream>

namespace orpheus::reaper {

std::string BuildPanelText() {
  std::ostringstream stream;
  stream << "Orpheus Adapter\n";
  stream << "ABI Version: " << ToString(kCurrentAbi) << "\n";
  stream << "Status: Ready";
  return stream.str();
}

const char *PanelTitle() {
  return "Orpheus";
}

}  // namespace orpheus::reaper
