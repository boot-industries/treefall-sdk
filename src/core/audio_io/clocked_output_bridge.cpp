// SPDX-License-Identifier: MIT
#include <orpheus/clocked_output_bridge.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace orpheus {
namespace {

struct FifoSlot {
  std::atomic<uint64_t> sequence{0};
};

class FrameQueue {
public:
  FrameQueue(uint32_t capacity, uint16_t channels)
      : capacity_(capacity), channels_(channels), slots_(new FifoSlot[capacity]),
        samples_(new float[static_cast<size_t>(capacity) * channels]) {
    for (uint64_t index = 0; index < capacity_; ++index) {
      slots_[index].sequence.store(index, std::memory_order_relaxed);
    }
  }

  bool enqueue(const float* frame) noexcept {
    const uint64_t position = enqueue_position_.load(std::memory_order_relaxed);
    FifoSlot& slot = slots_[position % capacity_];
    if (slot.sequence.load(std::memory_order_acquire) != position) {
      return false;
    }
    std::memcpy(samples_.get() + static_cast<size_t>(position % capacity_) * channels_, frame,
                static_cast<size_t>(channels_) * sizeof(float));
    enqueue_position_.store(position + 1, std::memory_order_relaxed);
    slot.sequence.store(position + 1, std::memory_order_release);
    const uint32_t fill = fill_.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32_t high = high_water_.load(std::memory_order_relaxed);
    while (fill > high &&
           !high_water_.compare_exchange_weak(high, fill, std::memory_order_relaxed)) {
    }
    return true;
  }

  bool dequeue(float* frame) noexcept {
    uint64_t position = dequeue_position_.load(std::memory_order_relaxed);
    for (;;) {
      FifoSlot& slot = slots_[position % capacity_];
      const uint64_t sequence = slot.sequence.load(std::memory_order_acquire);
      if (sequence == position + 1) {
        if (!dequeue_position_.compare_exchange_weak(position, position + 1,
                                                      std::memory_order_relaxed)) {
          continue;
        }
        std::memcpy(frame,
                    samples_.get() + static_cast<size_t>(position % capacity_) * channels_,
                    static_cast<size_t>(channels_) * sizeof(float));
        slot.sequence.store(position + capacity_, std::memory_order_release);
        fill_.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }
      if (sequence < position + 1) {
        return false;
      }
      position = dequeue_position_.load(std::memory_order_relaxed);
    }
  }

  uint32_t fill() const noexcept { return fill_.load(std::memory_order_relaxed); }
  uint32_t highWater() const noexcept {
    return high_water_.load(std::memory_order_relaxed);
  }

private:
  uint64_t capacity_;
  uint16_t channels_;
  std::unique_ptr<FifoSlot[]> slots_;
  std::unique_ptr<float[]> samples_;
  std::atomic<uint64_t> enqueue_position_{0};
  std::atomic<uint64_t> dequeue_position_{0};
  std::atomic<uint32_t> fill_{0};
  std::atomic<uint32_t> high_water_{0};
};

class ClockedOutputBridge final : public IClockedOutputBridge {
public:
  ClockedOutputBridge(ILiveAudioFanout& source, LiveAudioStreamId stream,
                      const ClockedOutputBridgeConfig& config)
      : source_(source), stream_(stream), config_(config),
        prepared_destination_max_(config.destination_max_block_frames),
        queue_(config.fifo_capacity_frames, config.channel_count),
        source_block_(new float[static_cast<size_t>(config.source_max_block_frames) *
                                config.channel_count]),
        discard_frame_(new float[config.channel_count]), current_frame_(new float[config.channel_count]),
        next_frame_(new float[config.channel_count]) {
    current_ratio_.store(nominalRatio(), std::memory_order_relaxed);
  }

  void pumpSource() noexcept override {
    LiveAudioBlockInfo info;
    while (source_.drain(stream_, source_block_.get(), config_.source_max_block_frames, info)) {
      if (info.channel_count != config_.channel_count ||
          info.sample_rate != config_.source_sample_rate ||
          info.frame_count > config_.source_max_block_frames) {
        source_discontinuities_.fetch_add(1, std::memory_order_relaxed);
        requestSourceReset();
        continue;
      }
      if (info.discontinuity) {
        source_discontinuities_.fetch_add(1, std::memory_order_relaxed);
        requestSourceReset();
      }

      const uint32_t fill = queue_.fill();
      if (fill + info.frame_count > config_.fifo_capacity_frames) {
        overflows_.fetch_add(1, std::memory_order_relaxed);
        source_discontinuities_.fetch_add(1, std::memory_order_relaxed);
        const uint32_t retain = std::min(config_.target_fill_frames,
                                         config_.fifo_capacity_frames - info.frame_count);
        while (queue_.fill() > retain && queue_.dequeue(discard_frame_.get())) {
        }
        reset_epoch_.fetch_add(1, std::memory_order_release);
      }

      for (uint32_t frame = 0; frame < info.frame_count; ++frame) {
        const float* source_frame =
            source_block_.get() + static_cast<size_t>(frame) * config_.channel_count;
        if (!queue_.enqueue(source_frame)) {
          // A competing destination dequeue can only create space. Reaching
          // this branch means the prepared capacity contract was violated;
          // drop the remaining incoming complete-block tail and reset epoch.
          overflows_.fetch_add(1, std::memory_order_relaxed);
          source_discontinuities_.fetch_add(1, std::memory_order_relaxed);
          reset_epoch_.fetch_add(1, std::memory_order_release);
          break;
        }
      }
      revision_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void render(float* const* outputs, uint16_t output_channels, uint32_t output_frames,
              uint64_t destination_sample_position, uint64_t host_time_nanoseconds,
              bool discontinuity) noexcept override {
    (void)destination_sample_position;
    (void)host_time_nanoseconds;

    if (discontinuity) {
      clearQueue();
      clearRenderState();
      resetController();
      resets_.fetch_add(1, std::memory_order_relaxed);
      reset_epoch_.fetch_add(1, std::memory_order_release);
    }
    const uint64_t epoch = reset_epoch_.load(std::memory_order_acquire);
    if (epoch != observed_epoch_) {
      clearRenderState();
      resetController();
      observed_epoch_ = epoch;
    }

    bool valid = outputs != nullptr && output_channels == config_.channel_count &&
                 output_frames != 0 && output_frames <= config_.destination_max_block_frames;
    if (outputs != nullptr) {
      for (uint16_t channel = 0; channel < output_channels; ++channel) {
        if (outputs[channel] == nullptr) {
          valid = false;
        } else {
          std::fill_n(outputs[channel], output_frames, 0.0f);
        }
      }
    }
    if (!valid) {
      underruns_.fetch_add(1, std::memory_order_relaxed);
      silence_frames_.fetch_add(output_frames, std::memory_order_relaxed);
      rendered_frames_.fetch_add(output_frames, std::memory_order_relaxed);
      revision_.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    updateController();
    const double ratio = current_ratio_.load(std::memory_order_relaxed);
    uint32_t silent = 0;
    bool underflow = false;
    for (uint32_t output_frame = 0; output_frame < output_frames; ++output_frame) {
      if (!ensurePair()) {
        ++silent;
        underflow = true;
        continue;
      }
      const float fraction = static_cast<float>(phase_);
      for (uint16_t channel = 0; channel < config_.channel_count; ++channel) {
        // Two-tap fractional FIR. Its phase is continuously slewed by the
        // bounded fill controller; no source frame is duplicated or skipped
        // outside that accumulator.
        outputs[channel][output_frame] =
            current_frame_[channel] + (next_frame_[channel] - current_frame_[channel]) * fraction;
      }
      phase_ += ratio;
      while (phase_ >= 1.0) {
        phase_ -= 1.0;
        std::memcpy(current_frame_.get(), next_frame_.get(),
                    static_cast<size_t>(config_.channel_count) * sizeof(float));
        have_current_ = true;
        have_next_ = queue_.dequeue(next_frame_.get());
        if (!have_next_) {
          break;
        }
      }
    }

    rendered_frames_.fetch_add(output_frames, std::memory_order_relaxed);
    silence_frames_.fetch_add(silent, std::memory_order_relaxed);
    if (underflow) {
      underruns_.fetch_add(1, std::memory_order_relaxed);
    }
    revision_.fetch_add(1, std::memory_order_relaxed);
  }

  SessionGraphError reconfigureDestination(uint32_t sample_rate,
                                            uint32_t max_block_frames) override {
    if (sample_rate == 0 || max_block_frames == 0 ||
        max_block_frames > prepared_destination_max_) {
      return SessionGraphError::InvalidParameter;
    }
    config_.destination_sample_rate = sample_rate;
    config_.destination_max_block_frames = max_block_frames;
    reset();
    return SessionGraphError::OK;
  }

  void reset() noexcept override {
    clearQueue();
    clearRenderState();
    resetController();
    resets_.fetch_add(1, std::memory_order_relaxed);
    reset_epoch_.fetch_add(1, std::memory_order_release);
    revision_.fetch_add(1, std::memory_order_relaxed);
  }

  ClockedOutputBridgeStatus status() const noexcept override {
    ClockedOutputBridgeStatus result;
    result.fifo_fill_frames = queue_.fill();
    result.fifo_high_water_frames = queue_.highWater();
    result.current_ratio = current_ratio_.load(std::memory_order_relaxed);
    result.estimated_clock_error_ppm =
        estimated_clock_error_ppm_.load(std::memory_order_relaxed);
    result.rendered_frames = rendered_frames_.load(std::memory_order_relaxed);
    result.silence_frames = silence_frames_.load(std::memory_order_relaxed);
    result.underruns = underruns_.load(std::memory_order_relaxed);
    result.overflows = overflows_.load(std::memory_order_relaxed);
    result.source_discontinuities = source_discontinuities_.load(std::memory_order_relaxed);
    result.resets = resets_.load(std::memory_order_relaxed);
    result.revision = revision_.load(std::memory_order_relaxed);
    return result;
  }

private:
  double nominalRatio() const noexcept {
    return static_cast<double>(config_.source_sample_rate) /
           static_cast<double>(config_.destination_sample_rate);
  }

  void resetController() noexcept {
    correction_ppm_ = 0.0;
    estimated_clock_error_ppm_.store(0.0, std::memory_order_relaxed);
    current_ratio_.store(nominalRatio(), std::memory_order_relaxed);
  }

  void updateController() noexcept {
    const double fill_error = static_cast<double>(queue_.fill()) - config_.target_fill_frames;
    const double normalized = fill_error / static_cast<double>(config_.target_fill_frames);
    const double estimate = std::clamp(normalized * config_.max_slew_ppm,
                                       -config_.max_slew_ppm, config_.max_slew_ppm);
    correction_ppm_ += (estimate - correction_ppm_) * 0.01;
    correction_ppm_ =
        std::clamp(correction_ppm_, -config_.max_slew_ppm, config_.max_slew_ppm);
    estimated_clock_error_ppm_.store(estimate, std::memory_order_relaxed);
    current_ratio_.store(nominalRatio() * (1.0 + correction_ppm_ * 1e-6),
                         std::memory_order_relaxed);
  }

  bool ensurePair() noexcept {
    if (!have_current_) {
      have_current_ = queue_.dequeue(current_frame_.get());
    }
    if (!have_current_) {
      return false;
    }
    if (!have_next_) {
      have_next_ = queue_.dequeue(next_frame_.get());
    }
    return have_next_;
  }

  void clearRenderState() noexcept {
    have_current_ = false;
    have_next_ = false;
    phase_ = 0.0;
  }

  void clearQueue() noexcept {
    while (queue_.dequeue(discard_frame_.get())) {
    }
  }

  void requestSourceReset() noexcept {
    clearQueue();
    reset_epoch_.fetch_add(1, std::memory_order_release);
  }

  ILiveAudioFanout& source_;
  LiveAudioStreamId stream_;
  ClockedOutputBridgeConfig config_;
  uint32_t prepared_destination_max_;
  FrameQueue queue_;
  std::unique_ptr<float[]> source_block_;
  std::unique_ptr<float[]> discard_frame_;
  std::unique_ptr<float[]> current_frame_;
  std::unique_ptr<float[]> next_frame_;
  bool have_current_{false};
  bool have_next_{false};
  double phase_{0.0};
  double correction_ppm_{0.0};
  uint64_t observed_epoch_{0};
  std::atomic<uint64_t> reset_epoch_{0};
  std::atomic<double> current_ratio_{1.0};
  std::atomic<double> estimated_clock_error_ppm_{0.0};
  std::atomic<uint64_t> rendered_frames_{0};
  std::atomic<uint64_t> silence_frames_{0};
  std::atomic<uint64_t> underruns_{0};
  std::atomic<uint64_t> overflows_{0};
  std::atomic<uint64_t> source_discontinuities_{0};
  std::atomic<uint64_t> resets_{0};
  std::atomic<uint64_t> revision_{0};
};

} // namespace

Result<std::unique_ptr<IClockedOutputBridge>>
createClockedOutputBridge(ILiveAudioFanout& source, LiveAudioStreamId stream,
                          const ClockedOutputBridgeConfig& config) {
  const auto source_status = source.streamStatus(stream);
  if (source_status.revision == 0 || config.channel_count == 0 ||
      config.source_sample_rate == 0 || config.destination_sample_rate == 0 ||
      config.source_max_block_frames == 0 || config.destination_max_block_frames == 0 ||
      config.fifo_capacity_frames < 2 || config.source_max_block_frames > config.fifo_capacity_frames ||
      config.target_fill_frames == 0 || config.target_fill_frames >= config.fifo_capacity_frames ||
      !std::isfinite(config.max_slew_ppm) || config.max_slew_ppm < 0.0 ||
      config.max_slew_ppm > 100000.0 ||
      static_cast<size_t>(config.fifo_capacity_frames) * config.channel_count >
          std::numeric_limits<size_t>::max() / sizeof(float)) {
    return {nullptr, SessionGraphError::InvalidParameter, "Invalid clocked output bridge config"};
  }
  try {
    return {std::make_unique<ClockedOutputBridge>(source, stream, config),
            SessionGraphError::OK, {}};
  } catch (const std::bad_alloc&) {
    return {nullptr, SessionGraphError::InternalError,
            "Unable to allocate clocked output bridge storage"};
  }
}

} // namespace orpheus
