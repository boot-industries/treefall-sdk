// SPDX-License-Identifier: MIT
#pragma once

// Locale-independent number formatting and parsing for the SDK's text formats.
// JSON and the other serialized documents always use '.' as the decimal point
// and never group digits, whatever global C or C++ locale the host installed.

#include <locale.h>
#include <stdlib.h>

#include <locale>
#include <sstream>

#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace orpheus::core::detail {

/// An output string stream pinned to the classic "C" locale, so neither the
/// global C++ locale's decimal point nor its digit grouping reaches the text.
class ClassicOStringStream : public std::ostringstream {
public:
  ClassicOStringStream() {
    imbue(std::locale::classic());
  }
};

/// strtod that always parses with the "C" numeric locale. errno and the end
/// pointer behave exactly as std::strtod does. If the C locale handle cannot be
/// created, nothing is consumed, so the caller reports an invalid number.
inline double strtodClassic(const char* text, char** end) noexcept {
#if defined(_WIN32)
  static const _locale_t classic = _create_locale(LC_NUMERIC, "C");
  if (classic == nullptr) {
    if (end != nullptr) {
      *end = const_cast<char*>(text);
    }
    return 0.0;
  }
  return _strtod_l(text, end, classic);
#else
  static const locale_t classic = newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0));
  if (classic == static_cast<locale_t>(0)) {
    if (end != nullptr) {
      *end = const_cast<char*>(text);
    }
    return 0.0;
  }
  return strtod_l(text, end, classic);
#endif
}

} // namespace orpheus::core::detail
