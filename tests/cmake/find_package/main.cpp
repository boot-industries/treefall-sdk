// SPDX-License-Identifier: MIT
#include <orpheus/abi.h>

// ORP134 G2 installed-header compile gate: the identity/time/media headers
// must be self-contained when consumed from an installed SDK.
#include <orpheus/audio_file_writer.h>
#include <orpheus/clocked_output_bridge.h>
#include <orpheus/live_audio.h>
#include <orpheus/streaming_sample_rate_converter.h>
#include <orpheus/identity.h>
#include <orpheus/media_model.h>
#include <orpheus/time_domain.h>
#include <orpheus/version.h>
#include <array>

int main() {
  static_assert(orpheus::kSdkVersionMajor == ORPHEUS_SDK_VERSION_MAJOR);
  uint32_t major = 0;
  uint32_t minor = 0;
  // ORP134 G2: exercise the installed identity/time primitives.
  const orpheus::ClipId clip = orpheus::ClipId::fromRaw(1);
  const orpheus::TimeRange range =
      orpheus::TimeRange::fromStartLength(orpheus::TimePoint::fromSamples(0), 48000);
  if (!clip.isValid() || range.length() != 48000) {
    return 1;
  }

  orpheus::LiveAudioFanoutConfig fanout_config;
  fanout_config.channel_count = 2;
  fanout_config.max_block_frames = 2;
  fanout_config.max_streams = 1;
  fanout_config.queue_blocks_per_stream = 2;
  auto fanout_result = orpheus::createLiveAudioFanout(fanout_config);
  if (!fanout_result.isOk()) {
    return 2;
  }
  auto fanout = std::move(fanout_result.value);
  auto stream_result = fanout->addStream({false});
  if (!stream_result.isOk() ||
      fanout->setStreamEnabled(stream_result.value, true) != orpheus::SessionGraphError::OK) {
    return 3;
  }
  const std::array<float, 2> left{1.0f, 2.0f};
  const std::array<float, 2> right{10.0f, 20.0f};
  const float* channels[]{left.data(), right.data()};
  fanout->publish({channels, 2, 2, 48000, 7, 11, false});
  std::array<float, 4> drained{};
  orpheus::LiveAudioBlockInfo info;
  if (!fanout->drain(stream_result.value, drained.data(), 2, info) ||
      drained != std::array<float, 4>{1.0f, 10.0f, 2.0f, 20.0f} ||
      info.sample_position != 7) {
    return 4;
  }

  orpheus::StreamingSampleRateConfig converter_config;
  converter_config.channel_count = 2;
  converter_config.max_input_frames = 2;
  converter_config.max_output_frames = 2;
  auto converter_result = orpheus::createStreamingSampleRateConverter(converter_config);
  if (!converter_result.isOk()) {
    return 5;
  }
  std::array<float, 4> converted{};
  const auto converted_result =
      converter_result.value->process(drained.data(), 2, converted.data(), 2);
  if (converted_result.error != orpheus::SessionGraphError::OK ||
      converted_result.consumed_frames != 2 || converted_result.produced_frames != 2 ||
      converted != drained) {
    return 6;
  }

  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &major, &minor);
  return session == nullptr || major != ORPHEUS_ABI_MAJOR || minor != ORPHEUS_ABI_MINOR;
}
