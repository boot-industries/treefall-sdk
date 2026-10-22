// SPDX-License-Identifier: MIT
#include "routing_meter_state.h"

#include <cmath>

namespace orpheus {

void RoutingMeterState::resetMaster() noexcept {
  m_master_peak.store(0.0f, std::memory_order_release);
  m_master_rms.store(0.0f, std::memory_order_release);
  m_master_clip_count.store(0, std::memory_order_release);
  for (auto& meter : m_master_true_peak_meters) {
    meter.reset();
  }
}

void RoutingMeterState::publishMasterPeak(float peak) noexcept {
  m_master_peak.store(peak, std::memory_order_release);
}

void RoutingMeterState::publishMasterRms(float rms) noexcept {
  m_master_rms.store(rms, std::memory_order_release);
}

void RoutingMeterState::incrementMasterClip() noexcept {
  m_master_clip_count.fetch_add(1, std::memory_order_relaxed);
}

AudioMeter RoutingMeterState::masterMeter() const noexcept {
  AudioMeter meter;

  const float peak = m_master_peak.load(std::memory_order_acquire);
  const float rms = m_master_rms.load(std::memory_order_acquire);
  const uint32_t clip_count = m_master_clip_count.load(std::memory_order_acquire);

  meter.peak_db = linearToDb(peak);
  meter.rms_db = linearToDb(rms);
  meter.clipping = (clip_count > 0);
  meter.clip_count = clip_count;

  return meter;
}

void RoutingMeterState::resetOutputs() noexcept {
  for (size_t output = 0; output < kRoutingMaxOutputs; ++output) {
    m_output_peak[output].store(0.0f, std::memory_order_release);
    m_output_rms[output].store(0.0f, std::memory_order_release);
    m_output_clip_count[output].store(0, std::memory_order_release);
    m_output_true_peak_meters[output].reset();
  }
}

void RoutingMeterState::publishOutputPeak(RoutingOutputIndex index, float peak) noexcept {
  m_output_peak[index].store(peak, std::memory_order_release);
}

void RoutingMeterState::publishOutputRms(RoutingOutputIndex index, float rms) noexcept {
  m_output_rms[index].store(rms, std::memory_order_release);
}

void RoutingMeterState::incrementOutputClip(RoutingOutputIndex index) noexcept {
  m_output_clip_count[index].fetch_add(1, std::memory_order_relaxed);
}

AudioMeter RoutingMeterState::outputMeter(RoutingOutputIndex index) const noexcept {
  AudioMeter meter;

  const float peak = m_output_peak[index].load(std::memory_order_acquire);
  const float rms = m_output_rms[index].load(std::memory_order_acquire);
  const uint32_t clip_count = m_output_clip_count[index].load(std::memory_order_acquire);

  meter.peak_db = linearToDb(peak);
  meter.rms_db = linearToDb(rms);
  meter.clipping = clip_count > 0;
  meter.clip_count = clip_count;

  return meter;
}

void RoutingMeterState::beginGroupOutputPublication() noexcept {
  m_group_output_meter_publication_sequence.fetch_add(1, std::memory_order_acq_rel);
}

void RoutingMeterState::publishGroupOutputHeader(RoutingGroupIndex group_count,
                                                 uint64_t topology_revision,
                                                 MeterAvailability availability) noexcept {
  m_group_output_meter_group_count.store(group_count, std::memory_order_relaxed);
  m_group_output_meter_topology_revision.store(topology_revision, std::memory_order_relaxed);
  m_group_output_meter_availability.store(static_cast<uint8_t>(availability),
                                          std::memory_order_relaxed);
  m_group_output_meter_coherent.store(0, std::memory_order_relaxed);
}

void RoutingMeterState::endGroupOutputPublication(bool coherent) noexcept {
  m_group_output_meter_coherent.store(coherent ? 1 : 0, std::memory_order_release);
  m_group_output_meter_publication_sequence.fetch_add(1, std::memory_order_release);
}

void RoutingMeterState::markGroupOutputUnmeasured() noexcept {
  m_group_output_meter_availability.store(static_cast<uint8_t>(MeterAvailability::Unmeasured),
                                          std::memory_order_relaxed);
}

void RoutingMeterState::resetGroupOutputPublication(RoutingGroupIndex group_count) noexcept {
  m_group_output_meter_publication_sequence.store(0, std::memory_order_release);
  m_group_output_meter_group_count.store(group_count, std::memory_order_release);
  m_group_output_meter_topology_revision.store(0, std::memory_order_release);
  m_group_output_meter_availability.store(static_cast<uint8_t>(MeterAvailability::Unmeasured),
                                          std::memory_order_release);
  m_group_output_meter_coherent.store(0, std::memory_order_release);
}

uint64_t RoutingMeterState::groupOutputPublicationSequence() const noexcept {
  return m_group_output_meter_publication_sequence.load(std::memory_order_acquire);
}

RoutingGroupIndex RoutingMeterState::groupOutputGroupCount() const noexcept {
  return m_group_output_meter_group_count.load(std::memory_order_relaxed);
}

uint64_t RoutingMeterState::groupOutputTopologyRevision() const noexcept {
  return m_group_output_meter_topology_revision.load(std::memory_order_relaxed);
}

MeterAvailability RoutingMeterState::groupOutputAvailability() const noexcept {
  return static_cast<MeterAvailability>(
      m_group_output_meter_availability.load(std::memory_order_relaxed));
}

bool RoutingMeterState::groupOutputCoherent() const noexcept {
  return m_group_output_meter_coherent.load(std::memory_order_acquire) != 0;
}

std::array<TruePeakMeter, 2>& RoutingMeterState::masterTruePeakMeters() noexcept {
  return m_master_true_peak_meters;
}

std::array<TruePeakMeter, kRoutingMaxOutputs>& RoutingMeterState::outputTruePeakMeters() noexcept {
  return m_output_true_peak_meters;
}

float RoutingMeterState::linearToDb(float linear) noexcept {
  if (!std::isfinite(linear) || linear <= 0.0f) {
    return kAudioMeterSilenceDb;
  }
  const float db = 20.0f * std::log10(linear);
  return std::isfinite(db) ? db : kAudioMeterSilenceDb;
}

} // namespace orpheus
