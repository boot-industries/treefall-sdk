// SPDX-License-Identifier: MIT
#pragma once

// ORP162: source-neutral, fixed-storage live PCM fan-out.
//
// A producer borrows planar source PCM for publish() only. Each enabled stream
// receives an independently drainable interleaved copy; callers create one
// fan-out per selected stem or heard-master bus.

#include <orpheus/errors.h>
#include <orpheus/export.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

namespace orpheus {

using LiveAudioStreamId = uint16_t;
inline constexpr LiveAudioStreamId kInvalidLiveAudioStreamId =
    std::numeric_limits<LiveAudioStreamId>::max();

/// Borrowed planar PCM valid only for the duration of publish().
struct LiveAudioBlockView {
  const float* const* channels{nullptr};
  uint16_t channel_count{0};
  uint32_t frame_count{0};
  uint32_t sample_rate{0};
  uint64_t sample_position{0};
  uint64_t host_time_nanoseconds{0};
  bool discontinuity{false};
};

/// Stored metadata accompanying a drained interleaved PCM block.
struct LiveAudioBlockInfo {
  uint16_t channel_count{0};
  uint32_t frame_count{0};
  uint32_t sample_rate{0};
  uint64_t sample_position{0};
  uint64_t host_time_nanoseconds{0};
  bool discontinuity{false};
};

struct LiveAudioFanoutConfig {
  uint16_t channel_count{2};
  uint32_t sample_rate{48000};
  uint32_t max_block_frames{512};
  uint16_t max_streams{2};
  uint16_t queue_blocks_per_stream{32};
};

struct LiveAudioStreamConfig {
  bool initially_enabled{false};
};

struct LiveAudioStreamStatus {
  bool enabled{false};
  uint64_t published_blocks{0};
  uint64_t published_frames{0};
  uint64_t accepted_blocks{0};
  uint64_t accepted_frames{0};
  uint64_t dropped_blocks{0};
  uint64_t dropped_frames{0};
  uint64_t drained_blocks{0};
  uint64_t drained_frames{0};
  uint64_t discontinuities{0};
  uint64_t invalid_blocks{0};
  uint64_t invalid_drains{0};
  uint32_t queue_depth_blocks{0};
  uint32_t queue_depth_frames{0};
  uint32_t queue_high_water_blocks{0};
  uint64_t revision{0};
};

static_assert(std::is_trivially_copyable_v<LiveAudioBlockView>);
static_assert(std::is_trivially_copyable_v<LiveAudioBlockInfo>);
static_assert(std::is_trivially_copyable_v<LiveAudioStreamStatus>);

/// Fixed-slot SPSC fan-out. Exactly one source producer and one worker consumer
/// are permitted per stream. Destruction requires both to be quiescent.
class ILiveAudioFanout {
public:
  virtual ~ILiveAudioFanout() = default;

  /// Control-thread only, before audio starts. Returns a stable stream ID.
  virtual Result<LiveAudioStreamId> addStream(const LiveAudioStreamConfig&) = 0;
  virtual SessionGraphError setStreamEnabled(LiveAudioStreamId, bool) noexcept = 0;

  /// Source callback only. Does not allocate, lock, block, or invoke consumers.
  virtual void publish(const LiveAudioBlockView&) noexcept = 0;

  /// Worker consumer only. Copies one complete queued block if destination fits.
  virtual bool drain(LiveAudioStreamId, float* interleaved_destination,
                     uint32_t destination_capacity_frames,
                     LiveAudioBlockInfo& info) noexcept = 0;

  /// Worker consumer only. Retains its newest complete queued block.
  virtual uint64_t discardToLatest(LiveAudioStreamId) noexcept = 0;

  /// Requires the stream be disabled or source and consumer be quiescent.
  virtual SessionGraphError resetStream(LiveAudioStreamId) noexcept = 0;
  virtual LiveAudioStreamStatus streamStatus(LiveAudioStreamId) const noexcept = 0;
};

ORPHEUS_API Result<std::unique_ptr<ILiveAudioFanout>>
createLiveAudioFanout(const LiveAudioFanoutConfig& config);

} // namespace orpheus
