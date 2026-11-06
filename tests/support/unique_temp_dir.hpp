// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace orpheus::tests::support {

/// A path under `temp_directory_path()` that no other process, checkout, or
/// earlier call in this process can produce: `<stem>-<pid>-<counter>-<ticks><extension>`.
/// Nothing is created on disk. Concurrent ctest runs (including two registrations
/// of the same binary) therefore never share or delete each other's scratch files.
inline std::filesystem::path UniqueTempPath(std::string_view stem,
                                            std::string_view extension = {}) {
  static std::atomic<std::uint64_t> counter{0};
#if defined(_WIN32)
  const auto pid = static_cast<long long>(_getpid());
#else
  const auto pid = static_cast<long long>(getpid());
#endif
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string name(stem);
  name += '-';
  name += std::to_string(pid);
  name += '-';
  name += std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
  name += '-';
  name += std::to_string(ticks);
  name += extension;
  return std::filesystem::temp_directory_path() / name;
}

/// Creates a fresh `UniqueTempPath(stem)` directory and removes it (recursively)
/// on destruction.
class UniqueTempDir {
public:
  explicit UniqueTempDir(std::string_view stem) : path_(UniqueTempPath(stem)) {
    std::filesystem::create_directories(path_);
  }
  ~UniqueTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  UniqueTempDir(const UniqueTempDir&) = delete;
  UniqueTempDir& operator=(const UniqueTempDir&) = delete;

  const std::filesystem::path& path() const {
    return path_;
  }
  std::filesystem::path operator/(const std::filesystem::path& child) const {
    return path_ / child;
  }

private:
  std::filesystem::path path_;
};

} // namespace orpheus::tests::support
