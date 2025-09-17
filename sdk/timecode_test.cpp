#include "timecode.h"

#include <cmath>

#include <gtest/gtest.h>

namespace reaper {
namespace timecode {

TEST(TimecodeFrameRate, ProvidesTrueValues) {
    EXPECT_DOUBLE_EQ(FramesPerSecond(FrameRate::FPS24), 24.0);
    EXPECT_DOUBLE_EQ(FramesPerSecond(FrameRate::FPS25), 25.0);
    EXPECT_DOUBLE_EQ(FramesPerSecond(FrameRate::FPS30), 30.0);
    EXPECT_DOUBLE_EQ(FramesPerSecond(FrameRate::FPS30_DROP), kFrameRate30Drop);
}

TEST(TimecodeConversion, DropFrameToSecondsUsesTrueRate) {
    Frame drop_frame{0, 0, 0, 15, FrameRate::FPS30_DROP};
    const double expected = 15.0 / FramesPerSecond(FrameRate::FPS30_DROP);
    EXPECT_NEAR(ToSeconds(drop_frame), expected, 1e-9);
}

TEST(TimecodeConversion, IntegerFrameRatesRemainUnchanged) {
    Frame tc{0, 0, 0, 15, FrameRate::FPS30};
    EXPECT_DOUBLE_EQ(ToSeconds(tc), 0.5);
}

TEST(TimecodeConversion, DropFrameRoundTripSeconds) {
    const double seconds = 123.456;
    Frame tc = FromSeconds(seconds, FrameRate::FPS30_DROP);
    EXPECT_EQ(tc.rate, FrameRate::FPS30_DROP);
    const double frame_duration = 1.0 / FramesPerSecond(FrameRate::FPS30_DROP);
    EXPECT_LT(std::fabs(ToSeconds(tc) - seconds), frame_duration);
}

TEST(TimecodeConversion, DropFrameToPTPUsesTrueRate) {
    Frame tc{0, 0, 0, 1, FrameRate::FPS30_DROP};
    const auto ptp = ToPTP(tc);
    EXPECT_EQ(ptp.seconds, 0u);
    const uint32_t expected = static_cast<uint32_t>((1.0 / FramesPerSecond(FrameRate::FPS30_DROP)) * 1e9);
    EXPECT_EQ(ptp.nanoseconds, expected);
}

}  // namespace timecode
}  // namespace reaper
