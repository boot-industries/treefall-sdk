// SPDX-License-Identifier: MIT
#pragma once

// ORP162: factory-prepared streaming counterpart to PolyphaseResampler.
// Coefficients use the same normalized-cutoff Blackman-windowed-sinc policy;
// process() uses caller-provided buffers and performs no allocation.

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstdint>
#include <memory>
#include <type_traits>

namespace orpheus {

struct StreamingSampleRateConfig {
  uint16_t channel_count{2};
  uint32_t input_sample_rate{48000};
  uint32_t output_sample_rate{48000};
  uint32_t max_input_frames{512};
  uint32_t max_output_frames{1024};
  uint32_t taps_per_phase{32};
};

struct StreamingSampleRateResult {
  SessionGraphError error{SessionGraphError::OK};
  uint32_t consumed_frames{0};
  uint32_t produced_frames{0};
};

static_assert(std::is_trivially_copyable_v<StreamingSampleRateResult>);

class IStreamingSampleRateConverter {
public:
  virtual ~IStreamingSampleRateConverter() = default;
  virtual void reset() noexcept = 0;
  virtual StreamingSampleRateResult process(const float* interleaved_input,
                                             uint32_t input_frames,
                                             float* interleaved_output,
                                             uint32_t output_capacity_frames) noexcept = 0;
};

ORPHEUS_API Result<std::unique_ptr<IStreamingSampleRateConverter>>
createStreamingSampleRateConverter(const StreamingSampleRateConfig& config);

} // namespace orpheus
