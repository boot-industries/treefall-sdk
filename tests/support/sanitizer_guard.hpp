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

} // namespace orpheus::test
