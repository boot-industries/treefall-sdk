// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <chrono>
#include <filesystem>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string StatusToString(orpheus_status status) {
  switch (status) {
    case ORPHEUS_STATUS_OK:
      return "ok";
    case ORPHEUS_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case ORPHEUS_STATUS_NOT_FOUND:
      return "not found";
    case ORPHEUS_STATUS_OUT_OF_MEMORY:
      return "out of memory";
    case ORPHEUS_STATUS_INTERNAL_ERROR:
      return "internal error";
    case ORPHEUS_STATUS_NOT_IMPLEMENTED:
      return "not implemented";
    case ORPHEUS_STATUS_IO_ERROR:
      return "io error";
  }
  return "unknown";
}

int main() {
  const auto *render = orpheus_render_abi_v1();
  if (render == nullptr) {
    std::cerr << "render ABI unavailable" << std::endl;
    return 1;
  }

  const std::vector<std::uint32_t> sample_rates = {44100, 48000, 96000};
  const double tempo = 120.0;
  const std::uint32_t bars = 4;

  std::cout << "Orpheus render_click performance" << std::endl;
  for (const auto rate : sample_rates) {
    const fs::path output =
        fs::temp_directory_path() /
        ("orpheus_perf_" + std::to_string(rate) + ".wav");

    orpheus_render_click_spec spec{};
    spec.tempo_bpm = tempo;
    spec.bars = bars;
    spec.sample_rate = rate;
    spec.channels = 2;
    spec.gain = 0.3;
    spec.click_frequency_hz = 1000.0;
    spec.click_duration_seconds = 0.05;

    const auto start = std::chrono::steady_clock::now();
    const auto status = render->render_click(&spec, output.string().c_str());
    const auto end = std::chrono::steady_clock::now();
    if (status != ORPHEUS_STATUS_OK) {
      std::cerr << "render_click failed: " << StatusToString(status)
                << std::endl;
      return 1;
    }

    const auto elapsed =
        std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "sample_rate=" << rate << "Hz took "
              << std::fixed << std::setprecision(3) << elapsed << " ms"
              << std::endl;

    std::error_code ec;
    fs::remove(output, ec);
  }

  return 0;
}
