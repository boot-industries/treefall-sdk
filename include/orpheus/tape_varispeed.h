// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/errors.h"
#include "orpheus/export.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace orpheus {

enum class TapeVarispeedSlewClock : uint8_t { InputFrames, OutputFrames };

struct TapeVarispeedConfig {
  uint32_t sample_rate = 0;
  uint16_t channels = 0;
  size_t max_input_frames = 0;
  size_t max_output_frames = 0;
  double min_rate = 0.5;
  double max_rate = 2.0;
  double initial_rate = 1.0;
  uint32_t rate_slew_frames = 0;
  TapeVarispeedSlewClock slew_clock = TapeVarispeedSlewClock::OutputFrames;
};

struct TapeVarispeedProcessResult {
  SessionGraphError error = SessionGraphError::OK;
  size_t input_frames_consumed = 0;
  size_t output_frames_produced = 0;
};

/// Prepared, interleaved, positive-rate tape varispeed. prepare() owns every
/// allocation; process(), drain(), reset(), and accessors are callback-safe.
class ORPHEUS_API PreparedTapeVarispeed {
public:
  PreparedTapeVarispeed();
  ~PreparedTapeVarispeed();
  PreparedTapeVarispeed(PreparedTapeVarispeed&&) noexcept;
  PreparedTapeVarispeed& operator=(PreparedTapeVarispeed&&) noexcept;
  PreparedTapeVarispeed(const PreparedTapeVarispeed&) = delete;
  PreparedTapeVarispeed& operator=(const PreparedTapeVarispeed&) = delete;

  SessionGraphError prepare(const TapeVarispeedConfig& config);
  size_t required_input_frames(size_t output_frames) const noexcept;
  size_t required_output_capacity(size_t input_frames) const noexcept;
  size_t max_drain_frames() const noexcept;
  size_t latency_frames() const noexcept;
  double applied_rate() const noexcept;
  TapeVarispeedProcessResult process(const float* interleaved_input, size_t input_frames,
                                     float* interleaved_output, size_t output_capacity_frames,
                                     double target_rate) noexcept;
  TapeVarispeedProcessResult drain(float* interleaved_output,
                                   size_t output_capacity_frames) noexcept;
  SessionGraphError reset(double initial_rate) noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Offline counterpart using the identical kernel. It may allocate only during
/// configure(); callers provide every output buffer to process() and drain().
class ORPHEUS_API OfflineTapeVarispeed {
public:
  OfflineTapeVarispeed();
  ~OfflineTapeVarispeed();
  OfflineTapeVarispeed(OfflineTapeVarispeed&&) noexcept;
  OfflineTapeVarispeed& operator=(OfflineTapeVarispeed&&) noexcept;
  OfflineTapeVarispeed(const OfflineTapeVarispeed&) = delete;
  OfflineTapeVarispeed& operator=(const OfflineTapeVarispeed&) = delete;

  SessionGraphError configure(uint32_t sample_rate, uint16_t channels, double rate);
  size_t required_output_capacity(size_t input_frames) const noexcept;
  size_t max_drain_frames() const noexcept;
  TapeVarispeedProcessResult process(const float* interleaved_input, size_t input_frames,
                                     float* interleaved_output, size_t output_capacity_frames);
  TapeVarispeedProcessResult drain(float* interleaved_output, size_t output_capacity_frames);
  void reset() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace orpheus
