// SPDX-License-Identifier: MIT
#pragma once

#include <cstdlib>
#include <string_view>

namespace orpheus::test {

/// True when this translation unit is compiled with a sanitizer.
inline bool underSanitizer() {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
  return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) ||                         \
    __has_feature(memory_sanitizer) || __has_feature(undefined_behavior_sanitizer)
  return true;
#else
  return false;
#endif
#else
  return false;
#endif
}

/// Timing thresholds are only meaningful in an unsanitized build.
/// ORP_SANITIZED_BUILD=1 lets CI force the same policy for a sanitized build
/// that the preprocessor cannot detect (UBSan alone defines no macro).
inline bool timingAssertionsEnabled() {
  if (underSanitizer()) {
    return false;
  }
  const char* flag = std::getenv("ORP_SANITIZED_BUILD");
  return flag == nullptr || std::string_view(flag) != "1";
}

/// Soak-length stress passes run in full only where they are meaningful and
/// cheap: unsanitized Release builds, or anywhere ORPHEUS_STRESS_FULL=1.
/// Debug and sanitized builds run a shortened pass that still exercises every
/// code path; the assertions that depend on duration are not weakened.
inline bool fullStressEnabled() {
  if (const char* full = std::getenv("ORPHEUS_STRESS_FULL");
      full != nullptr && std::string_view(full) == "1") {
    return true;
  }
#if defined(NDEBUG)
  return timingAssertionsEnabled();
#else
  return false;
#endif
}

} // namespace orpheus::test
