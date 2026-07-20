// SPDX-License-Identifier: MIT
#include <orpheus/clip_dsp.h>

#include <array>
#include <cmath>

int main() {
  orpheus::ClipDspProgram program;
  program.width.enabled = true;
  program.width.amount = 0.0F;
  program.limiter.enabled = true;
  program.limiter.ceilingDb = -6.02059991F;

  orpheus::ClipDspProcessor processor;
  if (processor.prepare(program, 48000.0, 2) != orpheus::ClipDspValidationError::OK) {
    return 1;
  }

  std::array<float, 2> frame{1.0F, -0.5F};
  processor.processFrame(frame.data(), frame.size());
  return std::abs(frame[0] - 0.25F) < 1.0e-5F && std::abs(frame[1] - 0.25F) < 1.0e-5F ? 0 : 2;
}
