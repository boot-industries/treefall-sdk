#include "orpheus/session.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

TEST(SessionRoundTripTest, SerializeDeserializePreservesEvents) {
  SessionState state;
  state.events.push_back({"transport", "play"});
  state.events.push_back({"marker", "intro"});

  const auto serialized = SerializeSession(state);
  const auto decoded = DeserializeSession(serialized);

  ASSERT_EQ(decoded.events.size(), state.events.size());
  for (std::size_t i = 0; i < state.events.size(); ++i) {
    EXPECT_EQ(decoded.events[i].type, state.events[i].type);
    EXPECT_EQ(decoded.events[i].payload, state.events[i].payload);
  }
}

TEST(SessionRoundTripTest, IgnoresMalformedLines) {
  const auto decoded = DeserializeSession("invalid\ntransport:stop");
  ASSERT_EQ(decoded.events.size(), 1u);
  EXPECT_EQ(decoded.events[0].type, "transport");
  EXPECT_EQ(decoded.events[0].payload, "stop");
}

}  // namespace orpheus::tests
