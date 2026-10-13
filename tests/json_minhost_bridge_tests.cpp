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
  std::string filename =
      "orpheus-minhost-json-" + std::to_string(now) + "-" + std::to_string(id) + ".json";
  return std::filesystem::temp_directory_path() / std::move(filename);
}

class TempJsonFile {
public:
  explicit TempJsonFile(std::string_view contents) : path_(MakeUniqueSpecPath()) {
    std::ofstream stream(path_);
    stream << contents;
    stream.close();
  }

  ~TempJsonFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const {
    return path_;
  }

private:
  std::filesystem::path path_;
};

} // namespace
TEST(JsonMinhostBridgeTests, RejectsFractionalAndOutOfRangeIntegerOverrides) {
  for (const std::string field : {"bars", "sample_rate", "channels"}) {
    for (const std::string value : {"1.5", "-1", "4294967296", "1e300"}) {
      TempJsonFile file("{\"" + field + "\":" + value + "}");
      minhost::ClickSpecOverrides overrides;
      minhost::ErrorInfo error;
      EXPECT_FALSE(minhost::ParseClickSpecOverrides(file.path(), overrides, error));
      EXPECT_EQ(error.code, "spec.parse");
      ASSERT_EQ(error.details.size(), 1u);
      EXPECT_EQ(error.details.front(),
                field + (value == "1.5" ? " must be integral" : " out of range"));
    }
  }
}

TEST(JsonMinhostBridgeTests, RangeBarConversionRejectsUnsignedOverflow) {
  minhost::SessionContext context;
  context.range_start_beats = 0.0;
  context.range_end_beats = 4.0 * 0x1p32;
  minhost::TimelineRange range;
  EXPECT_THROW(minhost::ComputeBarsFromRange(context, range), std::invalid_argument);
  context.range_end_beats = 4.0 * (0x1p32 - 1.0);
  EXPECT_EQ(minhost::ComputeBarsFromRange(context, range), UINT32_MAX);
}
