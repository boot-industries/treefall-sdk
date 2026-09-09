// SPDX-License-Identifier: MIT
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
  transport->getRealtimeTelemetry()->setDecimationBlocks(4);
  return transport->getRealtimeTelemetry()->decimationBlocks() == 4 ? 0 : 6;
}
