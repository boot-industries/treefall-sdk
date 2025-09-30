// SPDX-License-Identifier: MIT
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <gtest/gtest.h>

#define ORPHEUS_MINHOST_NO_ENTRYPOINT
#include "../adapters/minhost/main.cpp"
#undef ORPHEUS_MINHOST_NO_ENTRYPOINT

namespace {

std::filesystem::path MakeUniqueSpecPath() {
  static std::atomic<int> counter{0};
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const int id = counter.fetch_add(1);
  std::string filename = "orpheus-minhost-json-" + std::to_string(now) + "-" +
                         std::to_string(id) + ".json";
  return std::filesystem::temp_directory_path() / std::move(filename);
}

class TempJsonFile {
 public:
  explicit TempJsonFile(std::string_view contents)
      : path_(MakeUniqueSpecPath()) {
    std::ofstream stream(path_);
    stream << contents;
    stream.close();
  }

  ~TempJsonFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path &path() const { return path_; }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST(JsonMinhostBridgeTests, ParsesTypicalSpec) {
  const std::string contents = R"JSON({
    "tempo_bpm": 123.5,
    "bars": 8,
    "sample_rate": 48000,
    "channels": 2,
    "gain": -3.0,
    "click_frequency_hz": 950.0,
    "click_duration_seconds": 0.25,
    "output_path": "click.wav"
  })JSON";

  TempJsonFile file(contents);
  minhost::ClickSpecOverrides overrides;
  minhost::ErrorInfo error;
  ASSERT_TRUE(minhost::ParseClickSpecOverrides(file.path(), overrides, error));
  EXPECT_TRUE(error.code.empty());
  EXPECT_TRUE(error.message.empty());
  EXPECT_TRUE(error.details.empty());

  ASSERT_TRUE(overrides.tempo_bpm.has_value());
  EXPECT_DOUBLE_EQ(*overrides.tempo_bpm, 123.5);
  ASSERT_TRUE(overrides.bars.has_value());
  EXPECT_EQ(*overrides.bars, 8u);
  ASSERT_TRUE(overrides.sample_rate.has_value());
  EXPECT_EQ(*overrides.sample_rate, 48000u);
  ASSERT_TRUE(overrides.channels.has_value());
  EXPECT_EQ(*overrides.channels, 2u);
  ASSERT_TRUE(overrides.gain.has_value());
  EXPECT_DOUBLE_EQ(*overrides.gain, -3.0);
  ASSERT_TRUE(overrides.click_frequency_hz.has_value());
  EXPECT_DOUBLE_EQ(*overrides.click_frequency_hz, 950.0);
  ASSERT_TRUE(overrides.click_duration_seconds.has_value());
  EXPECT_DOUBLE_EQ(*overrides.click_duration_seconds, 0.25);
  ASSERT_TRUE(overrides.output_path.has_value());
  EXPECT_EQ(*overrides.output_path, "click.wav");
}

TEST(JsonMinhostBridgeTests, ParsesMinimalSpec) {
  TempJsonFile file("{}\n");
  minhost::ClickSpecOverrides overrides;
  minhost::ErrorInfo error;
  EXPECT_TRUE(minhost::ParseClickSpecOverrides(file.path(), overrides, error));
  EXPECT_TRUE(error.message.empty());
  EXPECT_FALSE(overrides.tempo_bpm.has_value());
  EXPECT_FALSE(overrides.bars.has_value());
  EXPECT_FALSE(overrides.sample_rate.has_value());
  EXPECT_FALSE(overrides.channels.has_value());
  EXPECT_FALSE(overrides.gain.has_value());
  EXPECT_FALSE(overrides.click_frequency_hz.has_value());
  EXPECT_FALSE(overrides.click_duration_seconds.has_value());
  EXPECT_FALSE(overrides.output_path.has_value());
}

TEST(JsonMinhostBridgeTests, ReportsInvalidSpecDetails) {
  const std::string contents = R"JSON({
    "bars": -1
  })JSON";

  TempJsonFile file(contents);
  minhost::ClickSpecOverrides overrides;
  minhost::ErrorInfo error;
  EXPECT_FALSE(minhost::ParseClickSpecOverrides(file.path(), overrides, error));
  EXPECT_EQ(error.code, "spec.parse");
  EXPECT_EQ(error.message, "Failed to parse click spec");
  ASSERT_EQ(error.details.size(), 1u);
  EXPECT_EQ(error.details.front(), "bars must be non-negative");
  EXPECT_FALSE(overrides.bars.has_value());
}
