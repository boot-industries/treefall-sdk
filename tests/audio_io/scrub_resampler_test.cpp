// SPDX-License-Identifier: MIT
//
// ORP128 / FTR018 §2 — unit tests for orpheus::ScrubResampler, the variable-rate
// + reverse scrub resampler. Ported from FourTrack's contract suite
// (fourtrack tests/unit/scrub_resampler_test.cpp), the load-bearing correctness
// spec named in the FTR018 handoff. Pure DSP, no engine: push a known ramp
// forward, then render at forward, fractional, and reverse rates and assert the
// interpolated output. The reverse cases are the load-bearing ones: they prove
// reverse plays back buffered history (a plain file reader cannot stream
// backward — the reason this class exists).

#include <orpheus/dsp/scrub_resampler.h>

#include <gtest/gtest.h>

#include <vector>

using namespace orpheus;

namespace {

// Push a contiguous ramp 0,1,2,... starting at absolute index `first`.
void push_ramp(ScrubResampler& r, int64_t first, size_t count) {
  std::vector<float> ramp(count);
  for (size_t i = 0; i < count; ++i) {
    ramp[i] = static_cast<float>(static_cast<int64_t>(i) + first);
  }
  r.push_forward(ramp.data(), count, first);
}

TEST(ScrubResamplerTest, PassThroughAtUnityReturnsInput) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 512);

  std::vector<float> out(256, -1.0f);
  EXPECT_EQ(r.render(out.data(), out.size(), 1.0), out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], static_cast<float>(i), 1e-4f) << "at " << i;
  }
  // Cursor advanced by exactly one sample per output frame.
  EXPECT_NEAR(r.position(), 256.0, 1e-9);
}

TEST(ScrubResamplerTest, HalfRateInterpolatesMidpoints) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 512);

  std::vector<float> out(8, 0.0f);
  r.render(out.data(), out.size(), 0.5);
  // cursor: 0.0, 0.5, 1.0, 1.5, ... -> ramp values 0, 0.5, 1.0, 1.5, ...
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], 0.5f * static_cast<float>(i), 1e-4f) << "at " << i;
  }
  EXPECT_NEAR(r.position(), 4.0, 1e-9);
}

TEST(ScrubResamplerTest, DoubleRateSkipsEveryOther) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 512);

  std::vector<float> out(8, 0.0f);
  r.render(out.data(), out.size(), 2.0);
  // cursor: 0, 2, 4, 6, ... -> ramp values 0, 2, 4, 6, ...
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], 2.0f * static_cast<float>(i), 1e-4f) << "at " << i;
  }
  EXPECT_NEAR(r.position(), 16.0, 1e-9);
}

TEST(ScrubResamplerTest, ReversePlaysBufferedHistoryBackward) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 512);

  // Park the cursor mid-ring by playing forward first.
  std::vector<float> fwd(100, 0.0f);
  r.render(fwd.data(), fwd.size(), 1.0);
  ASSERT_NEAR(r.position(), 100.0, 1e-9);

  // Now reverse at -1.0x: output should descend from 100 downward.
  std::vector<float> rev(10, 0.0f);
  r.render(rev.data(), rev.size(), -1.0);
  for (size_t i = 0; i < rev.size(); ++i) {
    EXPECT_NEAR(rev[i], static_cast<float>(100 - static_cast<int>(i)), 1e-4f) << "at " << i;
  }
  EXPECT_NEAR(r.position(), 90.0, 1e-9);
}

TEST(ScrubResamplerTest, ZeroRateHolds) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 512);

  // Advance to a fractional position, then hold.
  std::vector<float> seek(5, 0.0f);
  r.render(seek.data(), seek.size(), 0.5); // cursor -> 2.5
  const double held = r.position();

  std::vector<float> out(16, -99.0f);
  r.render(out.data(), out.size(), 0.0);
  const float expected = out[0];
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], expected, 1e-6f) << "at " << i; // frozen value
  }
  // The held sample is the interpolation at 2.5 == 2.5 on the ramp.
  EXPECT_NEAR(expected, 2.5f, 1e-4f);
  EXPECT_NEAR(r.position(), held, 1e-9); // cursor did not move
}

TEST(ScrubResamplerTest, ReversePastBufferStartClampsToSilence) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 64); // only indices 0..63 buffered

  // Start near the beginning, reverse well past 0.
  std::vector<float> out(40, 7.0f);
  r.render(out.data(), out.size(), -1.0); // cursor 0 -> -40 across the buffer
  // out[0] at cursor 0 == sample 0 == 0.0; once cursor goes below the oldest
  // buffered sample (index 0), output must be silence — never out of bounds.
  EXPECT_NEAR(out[0], 0.0f, 1e-4f);
  for (size_t i = 1; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], 0.0f, 1e-4f) << "at " << i; // below index 0 -> silence
  }
  EXPECT_LT(r.position(), 0.0); // cursor is allowed to go negative internally
}

TEST(ScrubResamplerTest, ForwardPastNewestClampsToSilence) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 16); // indices 0..15

  std::vector<float> out(32, 3.0f);
  r.render(out.data(), out.size(), 1.0);
  for (size_t i = 0; i < 16; ++i) {
    EXPECT_NEAR(out[i], static_cast<float>(i), 1e-4f) << "at " << i;
  }
  for (size_t i = 16; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], 0.0f, 1e-4f) << "past newest at " << i;
  }
}

TEST(ScrubResamplerTest, RingWrapKeepsAbsoluteIndexingCorrect) {
  // Push more than capacity so the ring wraps; the newest samples must still read
  // back at their correct absolute positions and the oldest must have fallen off.
  ScrubResampler r;
  r.prepare(64);
  r.reset(0);
  push_ramp(r, 0, 200); // capacity 64 -> only indices 136..199 survive

  // newest_sample_ should be 199; oldest held is 136.
  EXPECT_EQ(r.newest_sample(), 199);

  // The ramp value equals the absolute index, so a unity render from cursor 0
  // (reset put it at 0; push advanced newest but NOT the cursor) reads from 0 —
  // but indices < 136 have fallen off => silence, and 136..199 read as value.
  std::vector<float> out(200, -1.0f);
  r.render(out.data(), out.size(), 1.0);
  for (int i = 0; i < 136; ++i) {
    EXPECT_NEAR(out[static_cast<size_t>(i)], 0.0f, 1e-4f) << "fell off at " << i;
  }
  for (int i = 136; i < 200; ++i) {
    EXPECT_NEAR(out[static_cast<size_t>(i)], static_cast<float>(i), 1e-4f) << "held at " << i;
  }
}

TEST(ScrubResamplerTest, ForwardRefillHintIsZeroWhenReversingOrHolding) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  EXPECT_EQ(r.forward_refill_hint(-1.0, 512), 0u);
  EXPECT_EQ(r.forward_refill_hint(0.0, 512), 0u);
  // Forward needs at least the consumed span plus interpolation margin.
  EXPECT_GE(r.forward_refill_hint(1.0, 512), 512u);
  EXPECT_GE(r.forward_refill_hint(2.0, 512), 1024u - 1u); // ~2x, capped at capacity
  EXPECT_LE(r.forward_refill_hint(2.0, 512), 1024u);
}

TEST(ScrubResamplerTest, ResetReparksCursorAndClearsHistory) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);
  push_ramp(r, 0, 128);

  r.reset(1000);
  EXPECT_NEAR(r.position(), 1000.0, 1e-9);
  // History cleared: nothing buffered, so render yields silence until refilled.
  std::vector<float> out(8, 5.0f);
  r.render(out.data(), out.size(), 1.0);
  for (float v : out) {
    EXPECT_NEAR(v, 0.0f, 1e-6f);
  }
  // After a fresh push at the new anchor, reads resume.
  r.reset(1000); // re-park (push never moves the cursor)
  push_ramp(r, 1000, 64);
  std::vector<float> out2(8, 0.0f);
  r.render(out2.data(), out2.size(), 1.0);
  for (size_t i = 0; i < out2.size(); ++i) {
    EXPECT_NEAR(out2[i], static_cast<float>(1000 + static_cast<int>(i)), 1e-4f) << "at " << i;
  }
}

// -----------------------------------------------------------------------------
// processVariable() — the per-call convenience shape named by the FTR018 §2
// handoff. It layers on the same ring, so it inherits the continuity guarantee;
// these cases prove the input->output form and cross-call phase persistence.
// -----------------------------------------------------------------------------

TEST(ScrubResamplerTest, ProcessVariablePassThroughAtUnity) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);

  // Feed a ramp block, render it back at 1.0x.
  std::vector<float> in(256);
  for (size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(i);
  }
  std::vector<float> out(256, -1.0f);
  EXPECT_EQ(r.processVariable(in.data(), in.size(), out.data(), out.size(), 1.0), out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    EXPECT_NEAR(out[i], static_cast<float>(i), 1e-4f) << "at " << i;
  }
}

TEST(ScrubResamplerTest, ProcessVariableKeepsFractionalPhaseAcrossCalls) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);

  // First feed the whole ramp so both calls read from the same history.
  std::vector<float> in(64);
  for (size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(i);
  }

  std::vector<float> a(4, 0.0f);
  r.processVariable(in.data(), in.size(), a.data(), a.size(), 0.5);
  // cursor: 0.0, 0.5, 1.0, 1.5 -> values 0, 0.5, 1.0, 1.5; cursor ends at 2.0.
  EXPECT_NEAR(r.position(), 2.0, 1e-9);

  // Second call feeds nothing new (null input); phase continues from 2.0.
  std::vector<float> b(4, 0.0f);
  r.processVariable(nullptr, 0, b.data(), b.size(), 0.5);
  for (size_t i = 0; i < b.size(); ++i) {
    EXPECT_NEAR(b[i], 2.0f + 0.5f * static_cast<float>(i), 1e-4f) << "at " << i;
  }
  EXPECT_NEAR(r.position(), 4.0, 1e-9);
}

TEST(ScrubResamplerTest, ProcessVariableReverseIsSignedRate) {
  ScrubResampler r;
  r.prepare(1024);
  r.reset(0);

  std::vector<float> in(128);
  for (size_t i = 0; i < in.size(); ++i) {
    in[i] = static_cast<float>(i);
  }
  // Advance forward, then reverse at -1.0x from mid-history with no new input.
  std::vector<float> fwd(50, 0.0f);
  r.processVariable(in.data(), in.size(), fwd.data(), fwd.size(), 1.0);
  ASSERT_NEAR(r.position(), 50.0, 1e-9);

  std::vector<float> rev(10, 0.0f);
  r.processVariable(nullptr, 0, rev.data(), rev.size(), -1.0);
  for (size_t i = 0; i < rev.size(); ++i) {
    EXPECT_NEAR(rev[i], static_cast<float>(50 - static_cast<int>(i)), 1e-4f) << "at " << i;
  }
}

} // namespace
