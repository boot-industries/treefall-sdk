// SPDX-License-Identifier: MIT
#pragma once

// ORP162: adaptive source-clock to destination-clock PCM bridge. The bridge
// consumes one caller-selected live-audio stream and renders only that PCM; it
// owns no driver, device, route, thread, source bus, or destination lifecycle.

#include <orpheus/errors.h>
#include <orpheus/export.h>
#include <orpheus/live_audio.h>

#include <cstdint>
#include <memory>
#include <type_traits>

namespace orpheus {

struct ClockedOutputBridgeConfig {
  uint16_t channel_count{2};
  uint32_t source_sample_rate{48000};
  uint32_t destination_sample_rate{48000};
  uint32_t source_max_block_frames{512};
  uint32_t destination_max_block_frames{512};
  uint32_t fifo_capacity_frames{24000};
  uint32_t target_fill_frames{4800};
  double max_slew_ppm{500.0};
};

struct ClockedOutputBridgeStatus {
  uint32_t fifo_fill_frames{0};
  uint32_t fifo_high_water_frames{0};
  double current_ratio{1.0};
  double estimated_clock_error_ppm{0.0};
  uint64_t rendered_frames{0};
  uint64_t silence_frames{0};
  uint64_t underruns{0};
  uint64_t overflows{0};
  uint64_t source_discontinuities{0};
  uint64_t resets{0};
  uint64_t revision{0};
};

static_assert(std::is_trivially_copyable_v<ClockedOutputBridgeStatus>);

class IClockedOutputBridge {
public:
  virtual ~IClockedOutputBridge() = default;
  virtual void pumpSource() noexcept = 0;
  virtual void render(float* const* outputs, uint16_t output_channels,
                      uint32_t output_frames, uint64_t destination_sample_position,
                      uint64_t host_time_nanoseconds, bool discontinuity) noexcept = 0;
  virtual SessionGraphError reconfigureDestination(uint32_t sample_rate,
                                                    uint32_t max_block_frames) = 0;
  virtual void reset() noexcept = 0;
  virtual ClockedOutputBridgeStatus status() const noexcept = 0;
};

/// source and stream must outlive the returned bridge. Reconfiguration is
/// control-thread-only while source and destination callbacks are quiesced.
ORPHEUS_API Result<std::unique_ptr<IClockedOutputBridge>>
createClockedOutputBridge(ILiveAudioFanout& source, LiveAudioStreamId stream,
                          const ClockedOutputBridgeConfig& config);

} // namespace orpheus
