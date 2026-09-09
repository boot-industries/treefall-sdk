// SPDX-License-Identifier: MIT
#if defined(TREEFALL_LEGACY_HEADER_FIRST)
#include <orpheus/audio_file_capabilities.h>
#include <orpheus/transport_controller.h>
#endif
#include <orpheus/transport_controller.h>
#include <treefall/audio_file_capabilities.h>
#include <treefall/transport_controller.h>
#include <treefall/version.h>

#include <array>
#include <type_traits>

int main() {
  static_assert(std::is_same_v<treefall::TransportConfig, orpheus::TransportConfig>);
  static_assert(std::is_same_v<treefall::SessionGraphError, orpheus::SessionGraphError>);
  static_assert(TREEFALL_SDK_VERSION_MAJOR == ORPHEUS_SDK_VERSION_MAJOR);
  static_assert(std::is_standard_layout_v<treefall::TransportCommandIngressTelemetry>);
  static_assert(std::is_trivially_copyable_v<treefall::TransportCommandIngressTelemetry>);
  auto transport = treefall::createTransportController(
      nullptr,
      treefall::TransportConfig{.sampleRate = 48000, .outputChannels = 2, .maxBlockFrames = 64});
  if (!transport)
    return 1;
  // Canonical and new spellings operate on the same object and admission queue.
  orpheus::ITransportController& legacy = *transport;
  for (uint32_t i = 0; i < treefall::kTransportCommandIngressCapacity; ++i)
    if (legacy.stopClip(1) != orpheus::SessionGraphError::OK)
      return 2;
  if (transport->stopClip(1) != treefall::SessionGraphError::NotReady)
    return 3;
  const auto full = legacy.getCommandIngressTelemetry();
  if (!full.supported || full.attemptedCount != 256 || full.admittedCount != 255 ||
      full.slotUnavailableCount != 1 || full.processedCount != 0 ||
      full.publicationContentionCount != 0 || full.preparationRejectedCount != 0)
    return 4;
  std::array<float, 64> left{}, right{};
  float* output[]{left.data(), right.data()};
  transport->processAudio(output, 2, 64);
  const auto drained = legacy.getCommandIngressTelemetry();
  if (drained.processedCount != full.admittedCount)
    return 5;
  if (legacy.startClip(1, 91) != treefall::SessionGraphError::OK)
    return 6;
  transport->processAudio(output, 2, 64);
  const auto settlements = legacy.getStartSettlementSnapshot();
  if (settlements.entryCount != 1 || settlements.entries[0].requestTag != 91 ||
      settlements.entries[0].outcome != treefall::StartSettlementOutcome::Started)
    return 7;
  if (transport->panic() != orpheus::SessionGraphError::OK)
    return 8;
  legacy.processAudio(output, 2, 64);
  if (transport->getTotalActiveVoiceCount() != 0)
    return 9;
  return 0;
}
