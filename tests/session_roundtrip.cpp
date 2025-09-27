// SPDX-License-Identifier: MIT
#include "orpheus/abi.h"

#include <gtest/gtest.h>

namespace orpheus::tests {

namespace {

class SessionHandle {
 public:
  SessionHandle() {
    uint32_t got_major = 0;
    uint32_t got_minor = 0;
    const auto *abi =
        orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR, &got_major, &got_minor);
    EXPECT_NE(abi, nullptr);
    if (abi == nullptr) {
      return;
    }
    EXPECT_EQ(got_major, ORPHEUS_ABI_V1_MAJOR);
    EXPECT_EQ(got_minor, ORPHEUS_ABI_V1_MINOR);
    EXPECT_EQ(abi->create(&handle_), ORPHEUS_STATUS_OK);
    abi_ = abi;
  }

  ~SessionHandle() {
    if (abi_ != nullptr && handle_ != nullptr) {
      abi_->destroy(handle_);
    }
  }

  SessionHandle(const SessionHandle &) = delete;
  SessionHandle &operator=(const SessionHandle &) = delete;

  [[nodiscard]] orpheus_session_handle get() const { return handle_; }
  [[nodiscard]] const orpheus_session_api_v1 *abi() const { return abi_; }

 private:
  const orpheus_session_api_v1 *abi_{};
  orpheus_session_handle handle_{};
};

}  // namespace

TEST(SessionApiTest, CanCreateSessionAndManipulateTempo) {
  SessionHandle session;
  ASSERT_NE(session.get(), nullptr);

  const auto *abi = session.abi();
  ASSERT_NE(abi, nullptr);

  ASSERT_EQ(abi->set_tempo(session.get(), 140.0), ORPHEUS_STATUS_OK);

  orpheus_transport_state state{};
  ASSERT_EQ(abi->get_transport_state(session.get(), &state),
            ORPHEUS_STATUS_OK);
  EXPECT_DOUBLE_EQ(state.tempo_bpm, 140.0);
}

TEST(SessionApiTest, ClipgridOperationsSucceed) {
  SessionHandle session;
  ASSERT_NE(session.get(), nullptr);

  const auto *session_abi = session.abi();
  ASSERT_NE(session_abi, nullptr);

  orpheus_track_handle track{};
  const orpheus_track_desc track_desc{"drums"};
  ASSERT_EQ(session_abi->add_track(session.get(), &track_desc, &track),
            ORPHEUS_STATUS_OK);
  ASSERT_NE(track, nullptr);

  uint32_t clip_major = 0;
  uint32_t clip_minor = 0;
  const auto *clipgrid =
      orpheus_clipgrid_abi_v1(ORPHEUS_ABI_V1_MAJOR, &clip_major, &clip_minor);
  ASSERT_NE(clipgrid, nullptr);
  EXPECT_EQ(clip_major, ORPHEUS_ABI_V1_MAJOR);
  EXPECT_EQ(clip_minor, ORPHEUS_ABI_V1_MINOR);

  const orpheus_clip_desc clip_desc{"intro", 0.0, 4.0};
  orpheus_clip_handle clip{};
  ASSERT_EQ(clipgrid->add_clip(session.get(), track, &clip_desc, &clip),
            ORPHEUS_STATUS_OK);
  ASSERT_NE(clip, nullptr);

  EXPECT_EQ(clipgrid->set_clip_start(session.get(), clip, 1.0),
            ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->set_clip_length(session.get(), clip, 2.0),
            ORPHEUS_STATUS_OK);
  EXPECT_EQ(clipgrid->commit(session.get()), ORPHEUS_STATUS_OK);

  EXPECT_EQ(clipgrid->remove_clip(session.get(), clip), ORPHEUS_STATUS_OK);
}

}  // namespace orpheus::tests
