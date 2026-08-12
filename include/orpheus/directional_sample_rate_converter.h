// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstdint>
#include <memory>

namespace orpheus::audio_utils {

/// Failure and transfer status for the bounded directional realtime converter.
enum class DirectionalSrcStatus : uint8_t {
  Ok = 0,
  NotPrepared = 1,
  InvalidArgument = 2,
  InputOverflow = 3,
  InputUnderflow = 4,
};

/// Fixed-resource configuration for one directional sample-rate boundary.
struct DirectionalSrcConfig {
  uint32_t input_rate = 0;
  uint32_t output_rate = 0;
  uint16_t channels = 0;
  uint32_t max_input_frames = 0;
  uint32_t max_output_frames = 0;
  uint32_t fifo_capacity_frames = 0;
  uint32_t prime_input_frames = 0;
  bool allow_input_rate_correction = false;
};

/// Result of one producer or consumer transfer.
struct DirectionalSrcTransfer {
  DirectionalSrcStatus status = DirectionalSrcStatus::NotPrepared;
  uint32_t input_frames_consumed = 0;
  uint32_t output_frames_produced = 0;
};

/// Bounded, planar, directional sample-rate conversion for continuous devices.
///
/// prepare() owns all allocation and coefficient generation. After preparation,
/// one producer may call pushInput() while one consumer calls
/// requiredInputFrames() and renderOutput(); those callback-facing operations
/// perform no allocation, locking, waiting, or I/O.
class ORPHEUS_API DirectionalSampleRateConverter final {
public:
  DirectionalSampleRateConverter() noexcept;
  ~DirectionalSampleRateConverter();
  DirectionalSampleRateConverter(const DirectionalSampleRateConverter&) = delete;
  DirectionalSampleRateConverter& operator=(const DirectionalSampleRateConverter&) = delete;
  DirectionalSampleRateConverter(DirectionalSampleRateConverter&&) noexcept;
  DirectionalSampleRateConverter& operator=(DirectionalSampleRateConverter&&) noexcept;

  SessionGraphError prepare(const DirectionalSrcConfig&) noexcept;
  DirectionalSrcStatus requiredInputFrames(uint32_t output_frames,
                                           uint32_t& input_frames) const noexcept;
  DirectionalSrcTransfer pushInput(const float* const* input, uint16_t channels,
                                   uint32_t frames) noexcept;
  DirectionalSrcTransfer renderOutput(float* const* output, uint16_t channels,
                                      uint32_t frames) noexcept;
  bool setInputRateCorrectionPpm(int32_t ppm) noexcept;
  bool isPrimed() const noexcept;
  uint32_t bufferedInputFrames() const noexcept;
  uint32_t latencyOutputFrames() const noexcept;
  void reset() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace orpheus::audio_utils
