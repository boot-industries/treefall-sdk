// SPDX-License-Identifier: MIT
// ORP136 §2.3: golden render-hash determinism gate for the transport render
// path — and the parity oracle for the ORP134 G1 streaming-reader migration.
//
// The same clip set, rendered from sample 0 with identical metadata, must
// produce BIT-IDENTICAL output regardless of callback block size. The output
// is hashed (FNV-1a 64 over the raw float bytes) at four block sizes and the
// hashes must agree; a repeated run must reproduce the same hash exactly.
//
// When ORP134 G1 swaps the audio-thread file reads for prepared/streamed
// sources, these hashes are the proof of parity: the streaming path must
// reproduce the hash the reading path produced (same platform, same build).
//
// Cross-platform tolerance strategy (documented per ORP136 §2.3): the target
// is bit-identical output everywhere. Within one platform+toolchain this test
// asserts exact equality. Hashes are NOT hard-coded as cross-platform golden
// constants yet, because the EqualPower fade path calls std::sin per sample
// and libm results differ across platforms; pinning that math (e.g. a
// polynomial or table, std::bit_cast discipline) is ORP134 G4 scope. Any
// cross-platform drift observed before then is a determinism bug to FILE,
// not to mask in this test.
//
// Scenario constraints that keep the output block-size-invariant:
//  * all starts/metadata land before the first rendered buffer (command
//    materialization is buffer-quantized by design);
//  * no looping clips (loop wrap re-seeks at buffer granularity — that
//    quantization is documented transport behavior, not a hash bug);
//  * natural OUT-point completion IS covered (the stop fade anchors at the
//    exact OUT sample, ORP127 G3).

#include "../../src/core/transport/transport_controller.h"
#include "../support/fnv1a64.hpp"
#include "../support/synth.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace orpheus;
using orpheus::tests::support::Fnv1a64;
using orpheus::tests::support::GenerateSine;
using orpheus::tests::support::kFnv1a64Offset;

namespace {

constexpr uint32_t kSampleRate = 48000;
// 98304 = 2^15 * 3: divisible by every tested block size.
constexpr size_t kTotalFrames = 98304;
constexpr size_t kBlockSizes[] = {256, 512, 1024, 2048};

// Deterministic PCM16 WAV writer fed from the integer-table sine in
// synth.hpp — no libm in fixture generation, so fixture bytes are identical
// on every platform.
std::string writePcm16Wav(const std::filesystem::path& dir, const std::string& name,
                          const std::vector<float>& samples, uint16_t numChannels) {
  const int64_t numFrames = static_cast<int64_t>(samples.size() / numChannels);
  std::string filepath = (dir / name).string();
  std::ofstream file(filepath, std::ios::binary);

  const uint32_t dataSize = static_cast<uint32_t>(numFrames * numChannels * sizeof(int16_t));
  const uint32_t fileSize = 36 + dataSize;
  const uint32_t fmtSize = 16;
  const uint16_t audioFormat = 1; // PCM
  const uint16_t blockAlign = static_cast<uint16_t>(numChannels * 2);
  const uint32_t byteRate = kSampleRate * blockAlign;
  const uint16_t bitsPerSample = 16;
  const uint32_t sampleRate = kSampleRate;

  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&fileSize), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  file.write(reinterpret_cast<const char*>(&fmtSize), 4);
  file.write(reinterpret_cast<const char*>(&audioFormat), 2);
  file.write(reinterpret_cast<const char*>(&numChannels), 2);
  file.write(reinterpret_cast<const char*>(&sampleRate), 4);
  file.write(reinterpret_cast<const char*>(&byteRate), 4);
  file.write(reinterpret_cast<const char*>(&blockAlign), 2);
  file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&dataSize), 4);

  for (float s : samples) {
    float clamped = std::min(1.0f, std::max(-1.0f, s));
    int16_t v = static_cast<int16_t>(clamped * 32767.0f);
    file.write(reinterpret_cast<const char*>(&v), 2);
  }
  file.close();
  return filepath;
}

std::vector<float> interleaveStereo(const std::vector<float>& left,
                                    const std::vector<float>& right) {
  std::vector<float> out(left.size() * 2, 0.0f);
  for (size_t i = 0; i < left.size(); ++i) {
    out[i * 2] = left[i];
    out[i * 2 + 1] = right[i];
  }
  return out;
}

class TransportRenderHashTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_tempDir = std::filesystem::temp_directory_path() / "orp136_render_hash";
    std::filesystem::create_directories(m_tempDir);

    // Fixture 1: stereo dual sine, 1.5s — plays to natural EOF inside the
    // render window (covers OUT-point completion + default stop fade).
    {
      auto left = GenerateSine(72000, kSampleRate, 440, 0.5f);
      auto right = GenerateSine(72000, kSampleRate, 554, 0.5f);
      m_fixture1 = writePcm16Wav(m_tempDir, "hash_stereo.wav", interleaveStereo(left, right), 2);
    }
    // Fixture 2: stereo sine, 2.2s — trimmed + faded + gain (exercises trim
    // enforcement, EqualPower/Linear fade envelopes, clip gain).
    {
      auto left = GenerateSine(105600, kSampleRate, 220, 0.6f);
      auto right = GenerateSine(105600, kSampleRate, 330, 0.6f);
      m_fixture2 = writePcm16Wav(m_tempDir, "hash_trimmed.wav", interleaveStereo(left, right), 2);
    }
    // Fixture 3: mono sine, 1.0s — mono → phantom-center duplication path.
    {
      auto mono = GenerateSine(48000, kSampleRate, 660, 0.4f);
      m_fixture3 = writePcm16Wav(m_tempDir, "hash_mono.wav", mono, 1);
    }
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(m_tempDir, ec);
  }

  // Build the standard scenario on a fresh controller and render the whole
  // window at the given block size, returning the FNV-1a hash of every
  // output sample (L bytes then R bytes, buffer by buffer).
  uint64_t renderScenarioHash(size_t blockFrames) {
    EXPECT_EQ(kTotalFrames % blockFrames, 0u) << "block size must divide the render window";

    TransportController transport(nullptr, kSampleRate);

    EXPECT_EQ(transport.registerClipAudio(1, m_fixture1), SessionGraphError::OK);
    EXPECT_EQ(transport.registerClipAudio(2, m_fixture2), SessionGraphError::OK);
    EXPECT_EQ(transport.registerClipAudio(3, m_fixture3), SessionGraphError::OK);

    // Clip 2: trim + fades + gain. All metadata lands before the first
    // buffer, so every block size sees identical voice state.
    ClipMetadata meta;
    meta.trimInSamples = 1000;
    meta.trimOutSamples = 40000;
    meta.fadeInSeconds = 0.05;
    meta.fadeOutSeconds = 0.05;
    meta.fadeInCurve = FadeCurve::EqualPower;
    meta.fadeOutCurve = FadeCurve::Linear;
    meta.gainDb = -6.0f;
    meta.loopEnabled = false;
    EXPECT_EQ(transport.updateClipMetadata(2, meta), SessionGraphError::OK);
    EXPECT_EQ(transport.updateClipGain(3, 3.0f), SessionGraphError::OK);

    EXPECT_EQ(transport.prepareClipAudio(1), SessionGraphError::OK);
    EXPECT_EQ(transport.prepareClipAudio(2), SessionGraphError::OK);
    EXPECT_EQ(transport.prepareClipAudio(3), SessionGraphError::OK);

    EXPECT_EQ(transport.startClip(1), SessionGraphError::OK);
    EXPECT_EQ(transport.startClip(2), SessionGraphError::OK);
    EXPECT_EQ(transport.startClip(3), SessionGraphError::OK);

    std::vector<float> left(blockFrames, 0.0f);
    std::vector<float> right(blockFrames, 0.0f);
    float* buffers[2] = {left.data(), right.data()};

    // Accumulate the CONTIGUOUS per-channel sample streams before hashing.
    // (Hashing per-buffer L/R chunks would make the hash-stream order depend
    // on the block size even when the audio itself is identical.)
    std::vector<float> streamL;
    std::vector<float> streamR;
    streamL.reserve(kTotalFrames);
    streamR.reserve(kTotalFrames);

    const size_t numBuffers = kTotalFrames / blockFrames;
    for (size_t i = 0; i < numBuffers; ++i) {
      transport.processAudio(buffers, 2, blockFrames);
      streamL.insert(streamL.end(), left.begin(), left.end());
      streamR.insert(streamR.end(), right.begin(), right.end());
    }
    transport.processCallbacks();

    uint64_t hash = Fnv1a64(reinterpret_cast<const std::uint8_t*>(streamL.data()),
                            streamL.size() * sizeof(float), kFnv1a64Offset);
    hash = Fnv1a64(reinterpret_cast<const std::uint8_t*>(streamR.data()),
                   streamR.size() * sizeof(float), hash);
    return hash;
  }

  std::filesystem::path m_tempDir;
  std::string m_fixture1;
  std::string m_fixture2;
  std::string m_fixture3;
};

} // namespace

TEST_F(TransportRenderHashTest, HashIsIdenticalAcrossBlockSizes) {
  uint64_t reference = 0;
  bool haveReference = false;

  for (size_t blockFrames : kBlockSizes) {
    uint64_t hash = renderScenarioHash(blockFrames);
    std::cout << "[Render Hash] block " << blockFrames << ": 0x" << std::hex << hash << std::dec
              << "\n";
    if (!haveReference) {
      reference = hash;
      haveReference = true;
    } else {
      EXPECT_EQ(hash, reference) << "block size " << blockFrames
                                 << " produced different audio than the reference block size "
                                 << kBlockSizes[0]
                                 << " — the render path is block-size dependent (determinism bug)";
    }
  }
}

TEST_F(TransportRenderHashTest, HashIsReproducibleAcrossRuns) {
  const uint64_t first = renderScenarioHash(512);
  const uint64_t second = renderScenarioHash(512);
  EXPECT_EQ(first, second) << "same input, same block size, different output — "
                              "the render path is nondeterministic";
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
