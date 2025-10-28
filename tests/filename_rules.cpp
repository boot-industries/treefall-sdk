// SPDX-License-Identifier: MIT
#include "session/json_io.h"

#include <gtest/gtest.h>

namespace orpheus::core::session_json::tests {

TEST(RenderFilenameRules, FormatsSampleRateAndBitDepth) {
  const std::string filename = MakeRenderStemFilename("Project", "Drums", 44100, 24);
  EXPECT_EQ(filename, "project_drums_44p1k_24b.wav");

  const std::string repeat = MakeRenderStemFilename("Project", "Drums", 44100, 24);
  EXPECT_EQ(repeat, filename);
}

TEST(RenderFilenameRules, SanitizesNamesAndDefaultsValues) {
  const std::string filename = MakeRenderStemFilename(" My Session! ", "Lead Vox", 0, 0);
  EXPECT_EQ(filename, "my_session_lead_vox_44p1k_16b.wav");
}

TEST(RenderFilenameRules, HandlesHighResolutionRates) {
  const std::string filename = MakeRenderStemFilename("Master", "Mix", 192000, 32);
  EXPECT_EQ(filename, "master_mix_192k_32b.wav");
}

TEST(RenderFilenameRules, ClickFilesAreNestedInOutDirectory) {
  const std::string filename = MakeRenderClickFilename("Project", "Click", 48000, 24);
  EXPECT_EQ(filename, "out/project_click_48k_24b.wav");
}

} // namespace orpheus::core::session_json::tests
