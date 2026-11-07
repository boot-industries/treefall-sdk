// SPDX-License-Identifier: MIT
#include <array>
#include <memory>
#include <orpheus/realtime_telemetry.h>
#include <orpheus/session_graph.h>
#include <orpheus/transport_controller.h>
#include <type_traits>

static_assert(std::is_trivially_copyable_v<orpheus::GroupOutputMeterFrame>);
static_assert(std::is_standard_layout_v<orpheus::GroupOutputMeterFrame>);
static_assert(std::is_trivially_copyable_v<orpheus::GroupOutputMeterSnapshot>);
static_assert(std::is_standard_layout_v<orpheus::GroupOutputMeterSnapshot>);
static_assert(std::is_trivially_copyable_v<orpheus::RoutingMeterTelemetry>);
static_assert(std::is_standard_layout_v<orpheus::RoutingMeterTelemetry>);
using PublishFromRealtimeSignature =
    bool (orpheus::RealtimeTelemetry::*)(const orpheus::RealtimeTelemetrySnapshot&) noexcept;
static_assert(std::is_same_v<decltype(&orpheus::RealtimeTelemetry::publishFromRealtime),
                             PublishFromRealtimeSignature>);
static_assert(orpheus::kRealtimeTelemetrySchemaVersion == 4u);
static_assert(std::is_trivially_copyable_v<orpheus::RealtimeClockTelemetry>);
static_assert(std::is_standard_layout_v<orpheus::RealtimeClockTelemetry>);
static_assert(std::is_trivially_copyable_v<orpheus::TransportUnderrun>);
static_assert(static_cast<uint8_t>(orpheus::UnderrunCause::StreamingPageMiss) == 0u);

namespace {

// An installed consumer that implements only the four pure virtuals plus the appended hook.
class UnderrunRecorder final : public orpheus::ITransportCallback {
public:
  uint64_t cumulativeCount{0};

  void onClipStarted(orpheus::ClipHandle, orpheus::StartRequestTag, uint32_t,
                     orpheus::TransportPosition) override {}
  void onClipStopped(orpheus::ClipHandle, orpheus::StartRequestTag, uint32_t,
                     orpheus::TransportPosition) override {}
  void onClipLooped(orpheus::ClipHandle, orpheus::StartRequestTag, uint32_t,
                    orpheus::TransportPosition) override {}
  void onBufferUnderrun(orpheus::TransportPosition) override {}
  void onClassifiedUnderrun(const orpheus::TransportUnderrun& underrun) override {
    cumulativeCount = underrun.cumulativeCount;
  }
};

} // namespace

int main() {
  orpheus::core::SessionGraph graph;
  const orpheus::TimeRange clipRange =
      orpheus::TimeRange::fromStartLength(orpheus::TimePoint::fromSamples(24000), 48000);

  auto transaction = graph.begin_transaction();
  const orpheus::TrackId trackId = graph.create_track("Installed consumer track");
  const orpheus::ClipId clipId =
      graph.create_clip(trackId, "Installed consumer clip", clipRange, 2);
  const orpheus::core::SessionGraphChangeSet change = transaction.commit();
  const orpheus::core::SessionGraphSnapshot graphSnapshot = graph.snapshot();
  if (!trackId.isValid() || !clipId.isValid() || change.revision != 1 ||
      graphSnapshot.tracks.size() != 1 || graphSnapshot.tracks.front().clips.size() != 1 ||
      graphSnapshot.tracks.front().clips.front().id != clipId ||
      graphSnapshot.tracks.front().clips.front().range != clipRange) {
    return 1;
  }
  // An installed consumer must be rebuilt against the schema-2 layout and must
  // observe the allocator watermarks the snapshot now carries.
  if (graphSnapshot.schema_version != orpheus::core::SessionGraphSnapshot::kSchemaVersion ||
      graphSnapshot.next_track_id_raw != 2u || graphSnapshot.next_clip_id_raw != 2u) {
    return 7;
  }
  orpheus::core::SessionGraph restored;

  // The destination has already issued IDs of its own, so restoring the older
  if (restored.create_track("Pre-restore one").raw() != 1u ||
      restored.create_track("Pre-restore two").raw() != 2u) {
    return 8;
  }
  restored.restore(graphSnapshot);
  if (restored.create_track("Installed consumer restore").raw() != 3u) {
    return 8;
  }

  auto telemetryStorage = std::make_unique<orpheus::RealtimeTelemetry>(2);
  auto& telemetry = *telemetryStorage;
  if (telemetry.beginRealtimeBlock(128, 48000) || !telemetry.beginRealtimeBlock(128, 48000)) {
    return 2;
  }
  auto inputStorage = std::make_unique<orpheus::RealtimeTelemetrySnapshot>();
  auto& input = *inputStorage;
  input.position = orpheus::TimePoint::fromSamples(256);
  input.active_voice_count = 3;
  input.routing_meters.availability = orpheus::MeterAvailability::Measured;
  input.routing_meters.schema_version = 99;
  input.routing_meters.group_output_meters.schema_version = 99;
  if (!telemetry.publishFromRealtime(input)) {
    return 3;
  }

  auto outputStorage = std::make_unique<orpheus::RealtimeTelemetrySnapshot>();
  auto& output = *outputStorage;
  if (!telemetry.tryRead(output) || output.position.samples() != 256 ||
      output.active_voice_count != 3 || output.diagnostics.callback_count != 2 ||
      output.diagnostics.samples_processed != 256 ||
      output.schema_version != orpheus::kRealtimeTelemetrySchemaVersion ||
      output.routing_meters.schema_version != orpheus::kRoutingMeterTelemetrySchemaVersion ||
      output.routing_meters.group_output_meters.schema_version !=
          orpheus::kGroupOutputMeterSnapshotSchemaVersion ||
      output.routing_meters.availability != orpheus::MeterAvailability::Measured) {
    return 4;
  }

  auto transport = orpheus::createTransportController(
      &graph, orpheus::TransportConfig{.sampleRate = static_cast<uint32_t>(48000)});
  if (!transport || !transport->getRealtimeTelemetry()) {
    return 5;
  }
  orpheus::RealtimeTelemetry& transportTelemetry = *transport->getRealtimeTelemetry();
  transportTelemetry.setDecimationBlocks(1);
  std::array<float, 64> left{};
  std::array<float, 64> right{};
  float* outputs[2] = {left.data(), right.data()};
  const orpheus::TransportTimeReference reference{960, 5'000'000, true};
  transport->processAudioWithTimeReference(outputs, 2, left.size(), reference);
  auto clockStorage = std::make_unique<orpheus::RealtimeTelemetrySnapshot>();
  auto& clockOutput = *clockStorage;
  if (!transportTelemetry.tryRead(clockOutput) || clockOutput.schema_version != 4u ||
      !clockOutput.clock.device_sample_position_valid ||
      clockOutput.clock.device_sample_position != 960 || !clockOutput.clock.host_time_valid ||
      clockOutput.clock.host_time_nanoseconds != 5'000'000 ||
      clockOutput.clock.discontinuity_count != 1 || !clockOutput.clock.discontinuity_in_window ||
      clockOutput.clock.block_frames != left.size()) {
    return 10;
  }
  transport->processAudio(outputs, 2, left.size());
  if (!transportTelemetry.tryRead(clockOutput) || clockOutput.clock.device_sample_position_valid ||
      clockOutput.clock.host_time_valid || clockOutput.clock.discontinuity_count != 1) {
    return 9;
  }
  transportTelemetry.setDecimationBlocks(4);
  if (transportTelemetry.decimationBlocks() != 4) {
    return 6;
  }
  UnderrunRecorder recorder;
  orpheus::ITransportCallback& callback = recorder;
  callback.onClassifiedUnderrun(orpheus::TransportUnderrun{.cumulativeCount = 7});
  return recorder.cumulativeCount == 7 ? 0 : 11;
}
