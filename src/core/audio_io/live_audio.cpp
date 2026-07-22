// SPDX-License-Identifier: MIT
#include <orpheus/live_audio.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>

namespace orpheus {
namespace {

bool checkedMultiply(size_t a, size_t b, size_t& result) noexcept {
  if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
    return false;
  }
  result = a * b;
  return true;
}

struct StreamState {
  std::atomic<bool> enabled{false};
  std::atomic<uint64_t> write_index{0};
  std::atomic<uint64_t> read_index{0};
  std::atomic<bool> publish_discontinuity{false};
  std::atomic<bool> drain_discontinuity{false};
  std::atomic<uint64_t> published_blocks{0};
  std::atomic<uint64_t> published_frames{0};
  std::atomic<uint64_t> accepted_blocks{0};
  std::atomic<uint64_t> accepted_frames{0};
  std::atomic<uint64_t> dropped_blocks{0};
  std::atomic<uint64_t> dropped_frames{0};
  std::atomic<uint64_t> drained_blocks{0};
  std::atomic<uint64_t> drained_frames{0};
  std::atomic<uint64_t> discontinuities{0};
  std::atomic<uint64_t> invalid_blocks{0};
  std::atomic<uint64_t> invalid_drains{0};
  std::atomic<uint32_t> queue_frames{0};
  std::atomic<uint32_t> high_water{0};
  std::atomic<uint64_t> revision{0};
};

class LiveAudioFanout final : public ILiveAudioFanout {
public:
  explicit LiveAudioFanout(const LiveAudioFanoutConfig& config)
      : config_(config), streams_(new StreamState[config.max_streams]),
        infos_(new LiveAudioBlockInfo[static_cast<size_t>(config.max_streams) *
                                      config.queue_blocks_per_stream]),
        samples_(new float[static_cast<size_t>(config.max_streams) *
                           config.queue_blocks_per_stream * config.max_block_frames *
                           config.channel_count]),
        scratch_(new float[static_cast<size_t>(config.max_block_frames) * config.channel_count]) {}

  Result<LiveAudioStreamId> addStream(const LiveAudioStreamConfig& config) override {
    if (stream_count_ >= config_.max_streams) {
      return {kInvalidLiveAudioStreamId, SessionGraphError::InvalidParameter,
              "Live-audio stream capacity exhausted"};
    }
    const LiveAudioStreamId id = stream_count_++;
    streams_[id].enabled.store(config.initially_enabled, std::memory_order_relaxed);
    streams_[id].revision.fetch_add(1, std::memory_order_relaxed);
    return {id, SessionGraphError::OK, {}};
  }

  SessionGraphError setStreamEnabled(LiveAudioStreamId id, bool enabled) noexcept override {
    if (!validId(id)) {
      return SessionGraphError::InvalidHandle;
    }
    auto& stream = streams_[id];
    const bool previous = stream.enabled.exchange(enabled, std::memory_order_acq_rel);
    if (previous != enabled) {
      if (enabled) {
        stream.publish_discontinuity.store(true, std::memory_order_release);
      }
      stream.revision.fetch_add(1, std::memory_order_relaxed);
    }
    return SessionGraphError::OK;
  }

  void publish(const LiveAudioBlockView& block) noexcept override {
    bool shape_valid = block.channels != nullptr && block.channel_count == config_.channel_count &&
                       block.frame_count != 0 && block.frame_count <= config_.max_block_frames &&
                       block.sample_rate == config_.sample_rate;
    if (shape_valid) {
      for (uint16_t channel = 0; channel < config_.channel_count; ++channel) {
        if (block.channels[channel] == nullptr) {
          shape_valid = false;
          break;
        }
      }
    }

    bool any_enabled = false;
    for (LiveAudioStreamId id = 0; id < stream_count_; ++id) {
      auto& stream = streams_[id];
      if (!stream.enabled.load(std::memory_order_acquire)) {
        continue;
      }
      any_enabled = true;
      stream.published_blocks.fetch_add(1, std::memory_order_relaxed);
      stream.published_frames.fetch_add(block.frame_count, std::memory_order_relaxed);
      if (!shape_valid) {
        stream.invalid_blocks.fetch_add(1, std::memory_order_relaxed);
        stream.revision.fetch_add(1, std::memory_order_relaxed);
      }
    }
    if (!shape_valid || !any_enabled) {
      return;
    }

    for (uint32_t frame = 0; frame < block.frame_count; ++frame) {
      for (uint16_t channel = 0; channel < config_.channel_count; ++channel) {
        scratch_[static_cast<size_t>(frame) * config_.channel_count + channel] =
            block.channels[channel][frame];
      }
    }

    const size_t samples_per_slot =
        static_cast<size_t>(config_.max_block_frames) * config_.channel_count;
    for (LiveAudioStreamId id = 0; id < stream_count_; ++id) {
      auto& stream = streams_[id];
      if (!stream.enabled.load(std::memory_order_acquire)) {
        continue;
      }
      const uint64_t write = stream.write_index.load(std::memory_order_relaxed);
      const uint64_t read = stream.read_index.load(std::memory_order_acquire);
      if (write - read >= config_.queue_blocks_per_stream) {
        stream.dropped_blocks.fetch_add(1, std::memory_order_relaxed);
        stream.dropped_frames.fetch_add(block.frame_count, std::memory_order_relaxed);
        stream.discontinuities.fetch_add(1, std::memory_order_relaxed);
        stream.publish_discontinuity.store(true, std::memory_order_release);
        stream.revision.fetch_add(1, std::memory_order_relaxed);
        continue;
      }

      const size_t slot = static_cast<size_t>(id) * config_.queue_blocks_per_stream +
                          static_cast<size_t>(write % config_.queue_blocks_per_stream);
      std::memcpy(samples_.get() + slot * samples_per_slot, scratch_.get(),
                  static_cast<size_t>(block.frame_count) * config_.channel_count * sizeof(float));
      infos_[slot] = {block.channel_count, block.frame_count, block.sample_rate,
                      block.sample_position, block.host_time_nanoseconds,
                      block.discontinuity || stream.publish_discontinuity.exchange(
                                                 false, std::memory_order_acq_rel)};
      stream.accepted_blocks.fetch_add(1, std::memory_order_relaxed);
      stream.accepted_frames.fetch_add(block.frame_count, std::memory_order_relaxed);
      const uint32_t depth = static_cast<uint32_t>(write - read + 1);
      uint32_t high = stream.high_water.load(std::memory_order_relaxed);
      while (depth > high && !stream.high_water.compare_exchange_weak(
                                 high, depth, std::memory_order_relaxed)) {
      }
      stream.queue_frames.fetch_add(block.frame_count, std::memory_order_relaxed);
      stream.write_index.store(write + 1, std::memory_order_release);
      stream.revision.fetch_add(1, std::memory_order_relaxed);
    }
  }

  bool drain(LiveAudioStreamId id, float* destination, uint32_t capacity_frames,
             LiveAudioBlockInfo& info) noexcept override {
    if (!validId(id)) {
      return false;
    }
    auto& stream = streams_[id];
    const uint64_t read = stream.read_index.load(std::memory_order_relaxed);
    const uint64_t write = stream.write_index.load(std::memory_order_acquire);
    if (read == write) {
      return false;
    }
    const size_t slot = static_cast<size_t>(id) * config_.queue_blocks_per_stream +
                        static_cast<size_t>(read % config_.queue_blocks_per_stream);
    const LiveAudioBlockInfo queued_info = infos_[slot];
    if (destination == nullptr || capacity_frames < queued_info.frame_count) {
      stream.invalid_drains.fetch_add(1, std::memory_order_relaxed);
      stream.revision.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    const size_t samples_per_slot =
        static_cast<size_t>(config_.max_block_frames) * config_.channel_count;
    std::memcpy(destination, samples_.get() + slot * samples_per_slot,
                static_cast<size_t>(queued_info.frame_count) * config_.channel_count * sizeof(float));
    info = queued_info;
    if (stream.drain_discontinuity.exchange(false, std::memory_order_acq_rel)) {
      info.discontinuity = true;
    }
    stream.queue_frames.fetch_sub(queued_info.frame_count, std::memory_order_relaxed);
    stream.drained_blocks.fetch_add(1, std::memory_order_relaxed);
    stream.drained_frames.fetch_add(queued_info.frame_count, std::memory_order_relaxed);
    stream.read_index.store(read + 1, std::memory_order_release);
    stream.revision.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  uint64_t discardToLatest(LiveAudioStreamId id) noexcept override {
    if (!validId(id)) {
      return 0;
    }
    auto& stream = streams_[id];
    const uint64_t read = stream.read_index.load(std::memory_order_relaxed);
    const uint64_t write = stream.write_index.load(std::memory_order_acquire);
    if (write - read <= 1) {
      return 0;
    }
    uint64_t skipped_frames = 0;
    uint64_t index = read;
    while (index + 1 < write) {
      const size_t slot = static_cast<size_t>(id) * config_.queue_blocks_per_stream +
                          static_cast<size_t>(index % config_.queue_blocks_per_stream);
      skipped_frames += infos_[slot].frame_count;
      ++index;
    }
    stream.queue_frames.fetch_sub(static_cast<uint32_t>(skipped_frames), std::memory_order_relaxed);
    stream.read_index.store(write - 1, std::memory_order_release);
    stream.drain_discontinuity.store(true, std::memory_order_release);
    stream.discontinuities.fetch_add(1, std::memory_order_relaxed);
    stream.revision.fetch_add(1, std::memory_order_relaxed);
    return skipped_frames;
  }

  SessionGraphError resetStream(LiveAudioStreamId id) noexcept override {
    if (!validId(id)) {
      return SessionGraphError::InvalidHandle;
    }
    auto& stream = streams_[id];
    if (stream.enabled.load(std::memory_order_acquire)) {
      return SessionGraphError::NotReady;
    }
    const uint64_t write = stream.write_index.load(std::memory_order_acquire);
    stream.read_index.store(write, std::memory_order_release);
    stream.queue_frames.store(0, std::memory_order_relaxed);
    stream.publish_discontinuity.store(true, std::memory_order_release);
    stream.drain_discontinuity.store(true, std::memory_order_release);
    stream.discontinuities.fetch_add(1, std::memory_order_relaxed);
    stream.revision.fetch_add(1, std::memory_order_relaxed);
    return SessionGraphError::OK;
  }

  LiveAudioStreamStatus streamStatus(LiveAudioStreamId id) const noexcept override {
    if (!validId(id)) {
      return {};
    }
    const auto& stream = streams_[id];
    LiveAudioStreamStatus status;
    status.enabled = stream.enabled.load(std::memory_order_acquire);
    status.published_blocks = stream.published_blocks.load(std::memory_order_relaxed);
    status.published_frames = stream.published_frames.load(std::memory_order_relaxed);
    status.accepted_blocks = stream.accepted_blocks.load(std::memory_order_relaxed);
    status.accepted_frames = stream.accepted_frames.load(std::memory_order_relaxed);
    status.dropped_blocks = stream.dropped_blocks.load(std::memory_order_relaxed);
    status.dropped_frames = stream.dropped_frames.load(std::memory_order_relaxed);
    status.drained_blocks = stream.drained_blocks.load(std::memory_order_relaxed);
    status.drained_frames = stream.drained_frames.load(std::memory_order_relaxed);
    status.discontinuities = stream.discontinuities.load(std::memory_order_relaxed);
    status.invalid_blocks = stream.invalid_blocks.load(std::memory_order_relaxed);
    status.invalid_drains = stream.invalid_drains.load(std::memory_order_relaxed);
    const uint64_t write = stream.write_index.load(std::memory_order_acquire);
    const uint64_t read = stream.read_index.load(std::memory_order_acquire);
    status.queue_depth_blocks = static_cast<uint32_t>(write - read);
    status.queue_depth_frames = stream.queue_frames.load(std::memory_order_relaxed);
    status.queue_high_water_blocks = stream.high_water.load(std::memory_order_relaxed);
    status.revision = stream.revision.load(std::memory_order_relaxed);
    return status;
  }

private:
  bool validId(LiveAudioStreamId id) const noexcept { return id < stream_count_; }

  LiveAudioFanoutConfig config_;
  std::unique_ptr<StreamState[]> streams_;
  std::unique_ptr<LiveAudioBlockInfo[]> infos_;
  std::unique_ptr<float[]> samples_;
  std::unique_ptr<float[]> scratch_;
  LiveAudioStreamId stream_count_{0};
};

} // namespace

Result<std::unique_ptr<ILiveAudioFanout>>
createLiveAudioFanout(const LiveAudioFanoutConfig& config) {
  if (config.channel_count == 0 || config.sample_rate == 0 || config.max_block_frames == 0 ||
      config.max_streams == 0 || config.max_streams >= kInvalidLiveAudioStreamId ||
      config.queue_blocks_per_stream == 0) {
    return {nullptr, SessionGraphError::InvalidParameter, "Invalid live-audio fan-out config"};
  }
  size_t slots = 0;
  size_t frames = 0;
  size_t samples = 0;
  if (!checkedMultiply(config.max_streams, config.queue_blocks_per_stream, slots) ||
      !checkedMultiply(slots, config.max_block_frames, frames) ||
      !checkedMultiply(frames, config.channel_count, samples) ||
      samples > std::numeric_limits<size_t>::max() / sizeof(float)) {
    return {nullptr, SessionGraphError::InvalidParameter, "Live-audio storage size overflow"};
  }
  try {
    return {std::make_unique<LiveAudioFanout>(config), SessionGraphError::OK, {}};
  } catch (const std::bad_alloc&) {
    return {nullptr, SessionGraphError::InternalError, "Unable to allocate live-audio storage"};
  }
}

} // namespace orpheus
