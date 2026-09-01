// SPDX-License-Identifier: MIT
#include <orpheus/audio_driver_manager.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <vector>

static_assert(std::is_trivially_copyable_v<orpheus::TransportCallbackTelemetry>);
static_assert(std::is_standard_layout_v<orpheus::TransportCallbackTelemetry>);
static_assert(std::is_trivially_copyable_v<orpheus::ActiveVoiceSnapshot>);
static_assert(std::is_standard_layout_v<orpheus::ActiveVoiceSnapshot>);
static_assert(orpheus::kActiveVoiceSnapshotCapacity == 32);
static_assert(std::is_trivially_copyable_v<orpheus::RoutingGroupControlState>);
static_assert(std::is_standard_layout_v<orpheus::RoutingGroupControlState>);
static_assert(std::is_trivially_copyable_v<orpheus::RoutingControlSnapshot>);
static_assert(std::is_standard_layout_v<orpheus::RoutingControlSnapshot>);
static_assert(orpheus::kRoutingControlMaxGroups == 32);
static_assert(std::is_trivially_copyable_v<orpheus::AudioMeter>);
static_assert(std::is_standard_layout_v<orpheus::AudioMeter>);
static_assert(std::is_trivially_copyable_v<orpheus::GroupOutputMeterFrame>);
static_assert(std::is_standard_layout_v<orpheus::GroupOutputMeterFrame>);
static_assert(std::is_trivially_copyable_v<orpheus::GroupOutputMeterSnapshot>);
static_assert(std::is_standard_layout_v<orpheus::GroupOutputMeterSnapshot>);
static_assert(std::is_trivially_copyable_v<orpheus::RoutingMeterTelemetry>);
static_assert(std::is_standard_layout_v<orpheus::RoutingMeterTelemetry>);
static_assert(std::is_trivially_copyable_v<orpheus::RealtimeTelemetrySnapshot>);
static_assert(std::is_standard_layout_v<orpheus::RealtimeTelemetrySnapshot>);

namespace {

std::filesystem::path writeFixtureWav() {
  constexpr uint32_t sampleRate = 48000;
  constexpr uint16_t channels = 2;
  constexpr uint16_t bitsPerSample = 16;
  constexpr uint16_t audioFormat = 1;
  constexpr uint32_t frames = 128;
  constexpr uint32_t dataSize = frames * channels * sizeof(int16_t);
  constexpr uint32_t riffSize = 36 + dataSize;
  constexpr uint32_t fmtSize = 16;
  constexpr uint32_t byteRate = sampleRate * channels * sizeof(int16_t);
  constexpr uint16_t blockAlign = channels * sizeof(int16_t);
  const auto path = std::filesystem::temp_directory_path() / "orpheus-find-package-transport.wav";

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&riffSize), sizeof(riffSize));
  file.write("WAVEfmt ", 8);
  file.write(reinterpret_cast<const char*>(&fmtSize), sizeof(fmtSize));
  file.write(reinterpret_cast<const char*>(&audioFormat), sizeof(audioFormat));
  file.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
  file.write(reinterpret_cast<const char*>(&sampleRate), sizeof(sampleRate));
  file.write(reinterpret_cast<const char*>(&byteRate), sizeof(byteRate));
  file.write(reinterpret_cast<const char*>(&blockAlign), sizeof(blockAlign));
  file.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(bitsPerSample));
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
  std::array<int16_t, frames * channels> silence{};
  file.write(reinterpret_cast<const char*>(silence.data()),
             static_cast<std::streamsize>(silence.size() * sizeof(int16_t)));
  return path;
}

} // namespace

int main() {
  auto routing = orpheus::createRoutingMatrix();
  orpheus::RoutingConfig config;
  config.num_channels = 1;
  config.num_groups = 1;
  config.num_outputs = 2;
  config.enable_metering = true;
  config.enable_clipping_protection = false;
  if (routing->initialize(config) != orpheus::SessionGraphError::OK) {
    return 1;
  }

  constexpr uint32_t frames = 4096;
  std::vector<float> input(frames, 0.5f);
  const float* inputs[1] = {input.data()};
  std::vector<float> left(frames, -999.0f);
  std::vector<float> right(frames, -999.0f);
  float* outputs[2] = {left.data(), right.data()};
  if (routing->processRouting(inputs, outputs, frames) != orpheus::SessionGraphError::OK ||
      left[2048] < 0.3f || right.back() < 0.3f) {
    return 2;
  }
  orpheus::GroupOutputMeterSnapshot groupMeters;
  routing->copyGroupOutputMeterSnapshot(groupMeters);
  if (groupMeters.schema_version != orpheus::kGroupOutputMeterSnapshotSchemaVersion ||
      groupMeters.availability != orpheus::MeterAvailability::Measured ||
      groupMeters.coherent == 0 || groupMeters.group_count != 1 ||
      groupMeters.groups[0].logical_lane_count != 2 ||
      groupMeters.groups[0].raw_block_frames != orpheus::kRoutingSliceFrames ||
      groupMeters.groups[0].lane_meters[0].peak_db <=
          orpheus::kAudioMeterSilenceDb) {
    return 11;
  }

  const auto initialRouting = routing->getRoutingControlSnapshot();
  if (initialRouting.schema_version != orpheus::kRoutingControlSnapshotSchemaVersion ||
      initialRouting.group_count != 1) {
    return 7;
  }
  auto desiredRouting = initialRouting;
  desiredRouting.groups[0].gain_db = -6.0f;
  desiredRouting.groups[0].configured_mute = true;
  if (routing->applyGroupControlSnapshot(desiredRouting) != orpheus::SessionGraphError::OK) {
    return 8;
  }
  const auto appliedRouting = routing->getRoutingControlSnapshot();
  if (appliedRouting.revision != initialRouting.revision + 1 ||
      appliedRouting.groups[0].gain_db != -6.0f || !appliedRouting.groups[0].configured_mute ||
      !appliedRouting.groups[0].effective_mute) {
    return 9;
  }
  auto invalidRouting = appliedRouting;
  invalidRouting.groups[0].output_width = 3;
  if (routing->applyGroupControlSnapshot(invalidRouting) !=
          orpheus::SessionGraphError::InvalidParameter ||
      routing->getRoutingControlSnapshot().revision != appliedRouting.revision) {
    return 10;
  }

  orpheus::TransportConfig transportConfig;
  transportConfig.sampleRate = 48000;
  transportConfig.outputChannels = 2;
  transportConfig.maxBlockFrames = 64;
  transportConfig.maxActiveVoices = 1;
  auto transport = orpheus::createTransportController(nullptr, transportConfig);
  if (transport == nullptr) {
    return 3;
  }
  if (transport->startClipWithGroupChoke(0) != orpheus::SessionGraphError::InvalidHandle ||
      transport->startClipWithGroupChoke(42) != orpheus::SessionGraphError::ClipNotRegistered) {
    return 4;
  }

  const auto wav = writeFixtureWav();
  if (transport->registerClipAudio(77, wav.string()) != orpheus::SessionGraphError::OK ||
      transport->startClip(77) != orpheus::SessionGraphError::OK) {
    return 5;
  }
  std::array<float, 64> transportLeft{};
  std::array<float, 64> transportRight{};
  float* transportOutputs[2] = {transportLeft.data(), transportRight.data()};
  transport->processAudio(transportOutputs, 2, transportLeft.size());

  const orpheus::TransportCallbackTelemetry callbackTelemetry =
      transport->getCallbackDeliveryTelemetry();
  const orpheus::ActiveVoiceSnapshot activeVoices = transport->getActiveVoiceSnapshot();
  std::error_code ignored;
  std::filesystem::remove(wav, ignored);
  if (callbackTelemetry.schemaVersion != orpheus::kTransportCallbackTelemetrySchemaVersion ||
      callbackTelemetry.lastAttemptedSequence != 1 || callbackTelemetry.lastPostedSequence != 1 ||
      callbackTelemetry.cumulativeDroppedCount != 0 ||
      callbackTelemetry.activeVoiceSnapshotSequence != 1 ||
      activeVoices.schemaVersion != orpheus::kActiveVoiceSnapshotSchemaVersion ||
      activeVoices.publicationSequence != 1 || activeVoices.entryCount != 1 ||
      activeVoices.totalActiveVoiceCount != 1 || activeVoices.entries[0].handle != 77 ||
      activeVoices.entries[0].activeVoiceCount != 1 ||
      activeVoices.entries[0].state != orpheus::PlaybackState::Playing) {
    return 6;
  }

  auto manager = orpheus::createAudioDriverManager();
  return manager != nullptr && routing->maxBlockFrames() >= 1 ? 0 : 1;
}
