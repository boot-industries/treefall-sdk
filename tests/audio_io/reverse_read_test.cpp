// SPDX-License-Identifier: MIT
//
// ORP128 / FTR018 §1 — tests for the backward/windowed read
// (IAudioFileReader::readSamplesEndingAt). Two layers:
//
//   1. The portable BASE DEFAULT via a minimal in-memory fake reader, so the
//      default seek+read path is covered without any file dependency.
//   2. The libsndfile OVERRIDE via a temporary WAV written with sf_write, so the
//      concrete fast path is covered end to end.
//
// The window [end_sample - num_frames + 1, end_sample] is filled in forward
// (ascending-index) order; the caller walks it out in reverse. Underflow below 0
// clamps to the in-range trailing frames; the prior read position is restored.

#include <orpheus/audio_file_reader.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace orpheus;

namespace {

// -----------------------------------------------------------------------------
// A minimal mono in-memory reader that implements ONLY the pure-virtuals, so it
// inherits the base default readSamplesEndingAt. Samples are a ramp: value == idx.
// -----------------------------------------------------------------------------
class RampReader : public IAudioFileReader {
public:
  explicit RampReader(int64_t frames) : m_frames(frames) {}

  Result<AudioFileMetadata> open(const std::string&) override {
    m_open = true;
    Result<AudioFileMetadata> r;
    r.value.sample_rate = 48000;
    r.value.num_channels = 1;
    r.value.duration_samples = m_frames;
    return r;
  }

  Result<size_t> readSamples(float* buffer, size_t num_samples) override {
    Result<size_t> r;
    if (!m_open) {
      r.error = SessionGraphError::NotReady;
      return r;
    }
    size_t n = 0;
    for (; n < num_samples && m_pos < m_frames; ++n, ++m_pos) {
      buffer[n] = static_cast<float>(m_pos);
    }
    r.value = n;
    return r;
  }

  SessionGraphError seek(int64_t sample_position) override {
    if (!m_open) {
      return SessionGraphError::NotReady;
    }
    if (sample_position < 0) {
      sample_position = 0;
    }
    if (sample_position > m_frames) {
      sample_position = m_frames;
    }
    m_pos = sample_position;
    return SessionGraphError::OK;
  }

  void close() override {
    m_open = false;
    m_pos = 0;
  }
  int64_t getCurrentPosition() const override {
    return m_pos;
  }
  bool isOpen() const override {
    return m_open;
  }

private:
  int64_t m_frames;
  int64_t m_pos = 0;
  bool m_open = false;
};

} // namespace

// --- Base default (fake reader) ---------------------------------------------

TEST(ReverseReadBaseTest, WindowEndingAtReadsForwardOrder) {
  RampReader r(1000);
  r.open("");

  std::vector<float> buf(8, -1.0f);
  auto res = r.readSamplesEndingAt(107, buf.data(), 8); // frames 100..107
  ASSERT_TRUE(res.isOk());
  EXPECT_EQ(res.value, 8u);
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_NEAR(buf[i], static_cast<float>(100 + i), 1e-4f) << "at " << i;
  }
}

TEST(ReverseReadBaseTest, RestoresPriorForwardPosition) {
  RampReader r(1000);
  r.open("");

  // Establish a forward position, then a windowed read must not disturb it.
  std::vector<float> fwd(10);
  r.readSamples(fwd.data(), 10); // pos -> 10
  ASSERT_EQ(r.getCurrentPosition(), 10);

  std::vector<float> buf(4);
  r.readSamplesEndingAt(500, buf.data(), 4);
  EXPECT_EQ(r.getCurrentPosition(), 10); // restored

  // Forward streaming resumes where it left off.
  std::vector<float> more(4);
  r.readSamples(more.data(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(more[i], static_cast<float>(10 + i), 1e-4f);
  }
}

TEST(ReverseReadBaseTest, WindowUnderflowClampsToInRangeTail) {
  RampReader r(1000);
  r.open("");

  // end_sample 3, window 8 -> requested start -4, clamps to 0; in-range = 0..3.
  std::vector<float> buf(8, -1.0f);
  auto res = r.readSamplesEndingAt(3, buf.data(), 8);
  ASSERT_TRUE(res.isOk());
  EXPECT_EQ(res.value, 4u); // only frames 0..3 exist
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(buf[i], static_cast<float>(i), 1e-4f) << "at " << i;
  }
}

TEST(ReverseReadBaseTest, NegativeEndSampleReadsNothing) {
  RampReader r(1000);
  r.open("");
  std::vector<float> buf(4, 9.0f);
  auto res = r.readSamplesEndingAt(-1, buf.data(), 4);
  ASSERT_TRUE(res.isOk());
  EXPECT_EQ(res.value, 0u);
}

TEST(ReverseReadBaseTest, NotOpenReturnsNotReady) {
  RampReader r(1000);
  std::vector<float> buf(4);
  auto res = r.readSamplesEndingAt(10, buf.data(), 4);
  EXPECT_FALSE(res.isOk());
  EXPECT_EQ(res.error, SessionGraphError::NotReady);
}

TEST(ReverseReadBaseTest, ReversePlaybackDescendsWhenWalkedBackward) {
  // The intended use: read a forward window, then the caller emits it in reverse.
  RampReader r(1000);
  r.open("");

  std::vector<float> win(16);
  ASSERT_TRUE(r.readSamplesEndingAt(215, win.data(), 16).isOk()); // 200..215
  // Emitting win in reverse yields a descending ramp 215, 214, ... 200.
  for (size_t i = 0; i < 16; ++i) {
    float reversed = win[win.size() - 1 - i];
    EXPECT_NEAR(reversed, static_cast<float>(215 - static_cast<int>(i)), 1e-4f) << "at " << i;
  }
}

// --- libsndfile override (temp WAV) -----------------------------------------
// Guarded so the file only compiles/runs its integration half when the concrete
// reader is available. createAudioFileReader() returns nullptr in the stub build.

#if defined(ORPHEUS_HAVE_SNDFILE)
#include <sndfile.h>

namespace {
// Write a mono 48k WAV ramp (value == frame index) and return its path.
std::string writeRampWav(int64_t frames) {
  std::string path = std::string(::testing::TempDir()) + "orpheus_reverse_read_ramp.wav";
  SF_INFO info;
  info.samplerate = 48000;
  info.channels = 1;
  info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
  info.frames = 0;
  info.sections = 0;
  info.seekable = 0;
  SNDFILE* f = sf_open(path.c_str(), SFM_WRITE, &info);
  if (!f) {
    return "";
  }
  std::vector<float> ramp(static_cast<size_t>(frames));
  for (int64_t i = 0; i < frames; ++i) {
    ramp[static_cast<size_t>(i)] = static_cast<float>(i);
  }
  sf_writef_float(f, ramp.data(), frames);
  sf_close(f);
  return path;
}
} // namespace

TEST(ReverseReadSndfileTest, OverrideMatchesForwardOrderAndRestoresPosition) {
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);

  const std::string path = writeRampWav(1000);
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(reader->open(path).isOk());

  // Forward-stream a bit to set a position.
  std::vector<float> fwd(10);
  reader->readSamples(fwd.data(), 10);
  ASSERT_EQ(reader->getCurrentPosition(), 10);

  std::vector<float> buf(8, -1.0f);
  auto res = reader->readSamplesEndingAt(107, buf.data(), 8); // 100..107
  ASSERT_TRUE(res.isOk());
  EXPECT_EQ(res.value, 8u);
  for (size_t i = 0; i < 8; ++i) {
    EXPECT_NEAR(buf[i], static_cast<float>(100 + i), 1e-3f) << "at " << i;
  }
  EXPECT_EQ(reader->getCurrentPosition(), 10); // restored

  reader->close();
  std::remove(path.c_str());
}

TEST(ReverseReadSndfileTest, OverrideUnderflowClampsToTail) {
  auto reader = createAudioFileReader();
  ASSERT_NE(reader, nullptr);

  const std::string path = writeRampWav(1000);
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(reader->open(path).isOk());

  std::vector<float> buf(8, -1.0f);
  auto res = reader->readSamplesEndingAt(3, buf.data(), 8); // clamps to 0..3
  ASSERT_TRUE(res.isOk());
  EXPECT_EQ(res.value, 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NEAR(buf[i], static_cast<float>(i), 1e-3f) << "at " << i;
  }

  reader->close();
  std::remove(path.c_str());
}
#endif // ORPHEUS_HAVE_SNDFILE
