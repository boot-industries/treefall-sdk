// SPDX-License-Identifier: MIT
// Simplified tests that don't require audio file registration
#include <gtest/gtest.h>

#include <orpheus/transport_controller.h>

using namespace orpheus;

/// Test invalid handle error cases (no audio file needed)
TEST(ClipCuePointsSimpleTest, AddCuePointInvalidHandle) {
  // No audio file needed - testing error handling
  auto transport = createTransportController(nullptr, 48000);

  // Try to add cue point to non-existent clip
  int index = transport->addCuePoint(99999, 1000, "Invalid", 0xFFFFFFFF);
  EXPECT_EQ(index, -1); // Should fail
}

/// Test getting cue points from invalid handle
TEST(ClipCuePointsSimpleTest, GetCuePointsInvalidHandle) {
  auto transport = createTransportController(nullptr, 48000);

  // Try to get cue points from non-existent clip
  auto cuePoints = transport->getCuePoints(99999);
  EXPECT_TRUE(cuePoints.empty()); // Should return empty vector
}

// Note: Full tests that require audio file registration are in clip_cue_points_test.cpp
// Those tests will be skipped if test audio file is not available
