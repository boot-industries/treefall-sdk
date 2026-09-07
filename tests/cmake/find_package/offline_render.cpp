// SPDX-License-Identifier: MIT
// Installed public-API offline render composition and portable PCM golden.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <orpheus/audio_file_capabilities.h>
#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>
#include <orpheus/channel_format.h>
#include <orpheus/media_integrity.h>
#include <orpheus/routing_matrix.h>
#include <orpheus/transport_controller.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr size_t kSourceFrames = 8193;
constexpr size_t kOutputFrames = 8450;
constexpr uint32_t kRate = 48000;
constexpr uint16_t kChannels = 2;
constexpr char kGolden[] = "e59bff0bbf328128a96992e19b1047688227778480642251b1fa1f27f6b85eaa";
constexpr std::array<size_t, 4> kBlocks{256, 512, 1024, 2048};

[[noreturn]] void fail(const std::string& message) {
  throw std::runtime_error(message);
}
void require(bool condition, const std::string& message) {
  if (!condition)
    fail(message);
}

std::string hash(const std::filesystem::path& path) {
  const auto result = orpheus::sha256File(path.string());
  require(result.isOK(), "sha256File failed for " + path.string());
  return result.digestHex;
}

uint32_t little32(const std::vector<uint8_t>& b, size_t p) {
  require(p + 4 <= b.size(), "RIFF integer out of bounds");
  return uint32_t(b[p]) | (uint32_t(b[p + 1]) << 8) | (uint32_t(b[p + 2]) << 16) |
         (uint32_t(b[p + 3]) << 24);
}
uint16_t little16(const std::vector<uint8_t>& b, size_t p) {
  require(p + 2 <= b.size(), "RIFF short out of bounds");
  return uint16_t(b[p]) | uint16_t(b[p + 1] << 8);
}

std::string writeSource(const std::filesystem::path& path) {
  const auto caps = orpheus::getAudioFileCapabilities();
  require(caps.file_io_available, "audio file I/O capability is unavailable");
  const orpheus::AudioFileWriterConfig config{.format = orpheus::AudioFileFormat::WAV,
                                              .sample_rate = kRate,
                                              .num_channels = kChannels,
                                              .sample_format = orpheus::AudioSampleFormat::Int16};
  require(orpheus::preflightAudioFileWrite(config) == orpheus::SessionGraphError::OK,
          "WAV/Int16 preflight failed");
  auto writer = orpheus::createAudioFileWriter();
  require(writer != nullptr, "real audio writer provider is required");
  require(writer->open(path.string(), config) == orpheus::SessionGraphError::OK,
          "source writer open failed");
  std::vector<float> samples(kSourceFrames * kChannels);
  for (size_t n = 0; n < kSourceFrames; ++n) {
    const int32_t left = int32_t((17 * n) % 16384) - 8192;
    const int32_t right = int32_t((29 * n) % 16384) - 8192;
    samples[2 * n] = static_cast<float>(left) / 32768.0f;
    samples[2 * n + 1] = static_cast<float>(right) / 32768.0f;
  }
  const auto written = writer->writeSamples(samples.data(), kSourceFrames);
  require(written.isOk() && *written == kSourceFrames, "source frame write mismatch");
  require(writer->close() == orpheus::SessionGraphError::OK, "source close failed");
  require(writer->metadata().num_channels == kChannels && writer->metadata().sample_rate == kRate,
          "source metadata mismatch");
  return hash(path);
}

struct Payload {
  std::vector<uint8_t> bytes;
};
Payload readPayload(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  require(file.good(), "cannot open WAV for RIFF parsing");
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
  require(bytes.size() >= 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0 &&
              std::memcmp(bytes.data() + 8, "WAVE", 4) == 0,
          "invalid RIFF/WAVE header");
  bool fmt = false;
  Payload payload;
  size_t p = 12;
  while (p < bytes.size()) {
    require(p + 8 <= bytes.size(), "truncated RIFF chunk");
    const uint32_t size = little32(bytes, p + 4);
    const size_t body = p + 8;
    require(size <= bytes.size() - body, "RIFF chunk exceeds file");
    if (std::memcmp(bytes.data() + p, "fmt ", 4) == 0) {
      require(size >= 16, "short fmt chunk");
      require(little16(bytes, body) == 1 && little16(bytes, body + 2) == kChannels &&
                  little32(bytes, body + 4) == kRate && little16(bytes, body + 14) == 16,
              "WAV format metadata mismatch");
      fmt = true;
    } else if (std::memcmp(bytes.data() + p, "data", 4) == 0) {
      require(payload.bytes.empty(), "multiple data chunks are not expected");
      payload.bytes.assign(bytes.begin() + body, bytes.begin() + body + size);
    }
    const size_t advance = 8 + size + (size & 1u);
    require(advance <= bytes.size() - p, "RIFF chunk padding out of bounds");
    p += advance;
  }
  require(little32(bytes, 4) == bytes.size() - 8, "RIFF declared size mismatch");
  require(fmt && payload.bytes.size() == kOutputFrames * kChannels * 2,
          "WAV fmt/data payload mismatch");
  return payload;
}

std::string render(const std::filesystem::path& source, const std::filesystem::path& output,
                   size_t block, const std::filesystem::path& payloadPath) {
  auto probe = orpheus::probeAudioFile(source.string());
  require(probe.isOk() && probe.value.sample_rate == kRate &&
              probe.value.num_channels == kChannels &&
              probe.value.duration_samples == int64_t(kSourceFrames) &&
              probe.value.file_hash_sha256.empty(),
          "source probe mismatch");
  auto reader = orpheus::createAudioFileReader();
  auto writer = orpheus::createAudioFileWriter();
  require(reader && writer, "reader/writer provider is required");
  auto opened = reader->open(source.string());
  require(opened.isOk() && opened.value.sample_rate == kRate &&
              opened.value.num_channels == kChannels,
          "source reader open failed");

  const orpheus::TransportConfig config{.sampleRate = kRate,
                                        .outputChannels = kChannels,
                                        .maxBlockFrames = static_cast<uint32_t>(block),
                                        .maxActiveVoices = 1,
                                        .numGroups = 1,
                                        .maxSourceChannels = kChannels,
                                        .sourceChannelPolicy =
                                            orpheus::SourceChannelPolicy::Discrete};
  auto transport = orpheus::createTransportController(nullptr, config);
  require(transport != nullptr && transport->getRenderConfig().maxBlockFrames == block,
          "transport creation/config mismatch");
  require(transport->setGroupOutputBus(0, orpheus::OutputBusRoute{0, kChannels}) ==
                  orpheus::SessionGraphError::OK &&
              transport->getRoutingMatrix() != nullptr,
          "routing topology setup failed");
  orpheus::SessionDefaults defaults;
  defaults.fadeInSeconds = defaults.fadeOutSeconds = defaults.stopFadeOutSeconds =
      defaults.playDelaySeconds = 0.0;
  transport->setSessionDefaults(defaults);
  require(transport->registerClipAudio(1, source.string()) == orpheus::SessionGraphError::OK,
          "clip registration failed");
  require(transport->prepareClipAudio(1) == orpheus::SessionGraphError::OK,
          "clip preparation failed");
  require(transport->startClip(1) == orpheus::SessionGraphError::OK, "clip start failed");
  const orpheus::AudioFileWriterConfig writerConfig{.format = orpheus::AudioFileFormat::WAV,
                                                    .sample_rate = kRate,
                                                    .num_channels = kChannels,
                                                    .sample_format =
                                                        orpheus::AudioSampleFormat::Int16};
  require(writer->open(output.string(), writerConfig) == orpheus::SessionGraphError::OK,
          "output writer open failed");
  std::vector<float> left(block), right(block), interleaved(block * kChannels);
  float* outputs[] = {left.data(), right.data()};
  size_t rendered = 0;
  while (rendered < kOutputFrames) {
    const size_t frames = std::min(block, kOutputFrames - rendered);
    transport->processAudio(outputs, kChannels, frames);
    for (size_t n = 0; n < frames; ++n) {
      interleaved[2 * n] = left[n];
      interleaved[2 * n + 1] = right[n];
    }
    const auto result = writer->writeSamples(interleaved.data(), frames);
    require(result.isOk() && *result == frames, "output frame write mismatch");
    transport->processCallbacks();
    rendered += frames;
  }
  require(writer->close() == orpheus::SessionGraphError::OK &&
              writer->getFramesWritten() == int64_t(kOutputFrames),
          "output close/frame count mismatch");
  reader->close();
  const auto outputReader = orpheus::createAudioFileReader();
  require(outputReader != nullptr, "output reader unavailable");
  auto outputMeta = outputReader->open(output.string());
  require(outputMeta.isOk() && outputMeta.value.duration_samples == int64_t(kOutputFrames) &&
              outputMeta.value.sample_rate == kRate && outputMeta.value.num_channels == kChannels &&
              outputMeta.value.bit_depth == 16,
          "output metadata mismatch");
  std::vector<float> check(block * kChannels);
  size_t checked = 0;
  while (checked < kOutputFrames) {
    const auto count =
        outputReader->readSamples(check.data(), std::min(block, kOutputFrames - checked));
    require(count.isOk() && *count > 0, "output read failed/ended early");
    for (size_t n = 0; n < *count; ++n) {
      const size_t frame = checked + n;
      const int32_t ql = frame < kSourceFrames ? int32_t((17 * frame) % 16384) - 8192 : 0;
      const int32_t qr = frame < kSourceFrames ? int32_t((29 * frame) % 16384) - 8192 : 0;
      require(check[2 * n] == float(ql) / 32768.0f && check[2 * n + 1] == float(qr) / 32768.0f,
              "rendered channel sample mismatch");
    }
    checked += *count;
  }
  const auto eof = outputReader->readSamples(check.data(), 1);
  require(eof.isOk() && *eof == 0, "output EOF mismatch");
  outputReader->close();
  const auto parsed = readPayload(output);
  std::ofstream payload(payloadPath, std::ios::binary);
  require(payload.good(), "cannot create payload fixture");
  payload.write(reinterpret_cast<const char*>(parsed.bytes.data()), parsed.bytes.size());
  require(payload.good(), "cannot write payload fixture");
  payload.close();
  require(payload.good(), "cannot close payload fixture");
  require(hash(payloadPath) == kGolden, "portable PCM golden mismatch");
  return hash(output);
}

} // namespace

int main() {
  try {
    const auto dir = std::filesystem::current_path();
    std::filesystem::remove(dir / "offline-render-hashes.json");
    const auto source = dir / "offline-render-source.wav";
    const auto sourceHash = writeSource(source);
    (void)sourceHash;
    std::array<std::string, kBlocks.size()> first{};
    std::array<std::string, kBlocks.size()> second{};
    std::ostringstream json;
    json << "{\"schemaVersion\":1,\"sampleRate\":48000,\"channels\":2,"
         << "\"sampleEncoding\":\"pcm_s16le\",\"frames\":8450,\"payloadBytes\":33800,"
         << "\"goldenPcmSha256\":\"" << kGolden << "\",\"renders\":[";
    for (size_t i = 0; i < kBlocks.size(); ++i) {
      const auto output = dir / ("offline-render-" + std::to_string(kBlocks[i]) + ".wav");
      const auto payload = dir / ("offline-render-" + std::to_string(kBlocks[i]) + ".pcm");
      first[i] = render(source, output, kBlocks[i], payload);
      second[i] = render(source, output, kBlocks[i], payload);
      require(first[i] == second[i], "repeated complete WAV hash mismatch");
      if (i)
        require(first[i] == first[0], "block-size complete WAV hash mismatch");
      if (i)
        json << ',';
      json << "{\"blockFrames\":" << kBlocks[i] << ",\"pcmSha256\":\"" << kGolden
           << "\",\"wavSha256\":\"" << first[i] << "\"}";
    }
    json << "]}\n";
    std::ofstream evidence(dir / "offline-render-hashes.json");
    evidence << json.str();
    evidence.close();
    require(evidence.good(), "cannot emit offline render evidence");
    std::cout << json.str();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "offline render fixture failed: " << error.what() << '\n';
    return 1;
  }
}
