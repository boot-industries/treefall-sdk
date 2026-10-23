// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/routing_matrix.h>

#include "true_peak_meter.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace orpheus {

/// Internal matrix-level meter storage and publication (ORP273 F-16).
///
/// The class owns the master meter, the logical output meters and the
/// matrix-wide group-output publication. The render thread only stores and
/// the readers only load, so every published value is an atomic: the class is
/// lock-free, allocation-free and safe to read from a control thread while
/// the audio thread publishes.
///
/// The class is internal to the SDK. It is neither installed nor exported.
class RoutingMeterState {
public:
  RoutingMeterState() = default;

  // The state owns atomics, so it is neither copyable nor movable.
  RoutingMeterState(const RoutingMeterState&) = delete;
  RoutingMeterState& operator=(const RoutingMeterState&) = delete;
  RoutingMeterState(RoutingMeterState&&) = delete;
  RoutingMeterState& operator=(RoutingMeterState&&) = delete;

  /// Reset the master meter to silence and clear its clip counter.
  void resetMaster() noexcept;

  /// Publish the master peak level.
  void publishMasterPeak(float peak) noexcept;

  /// Publish the master RMS level.
  void publishMasterRms(float rms) noexcept;

  /// Count one clipped master block.
  void incrementMasterClip() noexcept;

  /// Read the published master meter.
  AudioMeter masterMeter() const noexcept;

  /// Reset every logical output meter to silence and clear its clip counter.
  void resetOutputs() noexcept;

  /// Publish the peak level of one logical output lane.
  void publishOutputPeak(RoutingOutputIndex index, float peak) noexcept;

  /// Publish the RMS level of one logical output lane.
  void publishOutputRms(RoutingOutputIndex index, float rms) noexcept;

  /// Count one clipped block on one logical output lane.
  void incrementOutputClip(RoutingOutputIndex index) noexcept;

  /// Read the published meter of one logical output lane.
  AudioMeter outputMeter(RoutingOutputIndex index) const noexcept;

  /// Open the matrix-wide group-output publication. The odd sequence value
  /// marks a publication in progress.
  void beginGroupOutputPublication() noexcept;

  /// Publish the group-output header inside the open publication.
  void publishGroupOutputHeader(RoutingGroupIndex group_count, uint64_t topology_revision,
                                MeterAvailability availability) noexcept;

  /// Close the matrix-wide group-output publication. The even sequence value
  /// marks a complete publication.
  void endGroupOutputPublication(bool coherent) noexcept;

  /// Mark the matrix-wide group-output publication unmeasured.
  void markGroupOutputUnmeasured() noexcept;

  /// Reset the matrix-wide group-output publication to the state a fresh
  /// initialization publishes.
  void resetGroupOutputPublication(RoutingGroupIndex group_count) noexcept;

  /// Read the publication sequence of the matrix-wide group-output meter.
  uint64_t groupOutputPublicationSequence() const noexcept;

  /// Read the published logical group count.
  RoutingGroupIndex groupOutputGroupCount() const noexcept;

  /// Read the published routing topology revision.
  uint64_t groupOutputTopologyRevision() const noexcept;

  /// Read the published matrix-wide availability.
  MeterAvailability groupOutputAvailability() const noexcept;

  /// Read the published coherence flag.
  bool groupOutputCoherent() const noexcept;

  /// Master true-peak histories. Render thread only.
  std::array<TruePeakMeter, 2>& masterTruePeakMeters() noexcept;

  /// Logical output true-peak histories. Render thread only.
  std::array<TruePeakMeter, kRoutingMaxOutputs>& outputTruePeakMeters() noexcept;

  /// Convert a linear amplitude to the published decibel value.
  static float linearToDb(float linear) noexcept;

private:
  // Master meter.
  std::atomic<float> m_master_peak{0.0f};
  std::atomic<float> m_master_rms{0.0f};
  std::atomic<uint32_t> m_master_clip_count{0};
  std::array<TruePeakMeter, 2> m_master_true_peak_meters{};

  // Logical output meters.
  std::array<std::atomic<float>, kRoutingMaxOutputs> m_output_peak{};
  std::array<std::atomic<float>, kRoutingMaxOutputs> m_output_rms{};
  std::array<std::atomic<uint32_t>, kRoutingMaxOutputs> m_output_clip_count{};
  std::array<TruePeakMeter, kRoutingMaxOutputs> m_output_true_peak_meters{};

  // Matrix-wide seqlock publication for the logical group-output extension.
  std::atomic<uint64_t> m_group_output_meter_publication_sequence{0};
  std::atomic<RoutingGroupIndex> m_group_output_meter_group_count{0};
  std::atomic<uint64_t> m_group_output_meter_topology_revision{0};
  std::atomic<uint8_t> m_group_output_meter_availability{
      static_cast<uint8_t>(MeterAvailability::Unmeasured)};
  std::atomic<uint8_t> m_group_output_meter_coherent{0};
};

} // namespace orpheus
