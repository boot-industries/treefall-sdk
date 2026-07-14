// SPDX-License-Identifier: MIT
#include <orpheus/realtime_telemetry.h>
#include <orpheus/session_graph.h>
#include <orpheus/transport_controller.h>

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

  orpheus::RealtimeTelemetry telemetry(2);
  if (telemetry.beginRealtimeBlock(128, 48000) || !telemetry.beginRealtimeBlock(128, 48000)) {
    return 2;
  }

  orpheus::RealtimeTelemetrySnapshot input;
  input.position = orpheus::TimePoint::fromSamples(256);
  input.active_voice_count = 3;
  if (!telemetry.publishFromRealtime(input)) {
    return 3;
  }

  orpheus::RealtimeTelemetrySnapshot output;
  if (!telemetry.tryRead(output) || output.position.samples() != 256 ||
      output.active_voice_count != 3 || output.diagnostics.callback_count != 2 ||
      output.diagnostics.samples_processed != 256) {
    return 4;
  }

  auto transport = orpheus::createTransportController(&graph, 48000);
  if (!transport || !transport->getRealtimeTelemetry()) {
    return 5;
  }
  transport->getRealtimeTelemetry()->setDecimationBlocks(4);
  return transport->getRealtimeTelemetry()->decimationBlocks() == 4 ? 0 : 6;
}
