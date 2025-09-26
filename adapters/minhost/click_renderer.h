#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace orpheus::minhost {

class ClickRenderer {
 public:
  void RenderClick(const std::string &path, int sampleRate, double bpm,
                   int bars) const;

 private:
  static void WriteWav(const std::string &path, int sampleRate,
                       const std::vector<int16_t> &samples);
};

}  // namespace orpheus::minhost
