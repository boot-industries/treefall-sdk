// SPDX-License-Identifier: MIT
#pragma once

#include <clocale>
#include <locale>
#include <string>

namespace orpheus::test {

/// A numpunct facet with a comma decimal point and '.'-grouped thousands.
/// It is built from the classic locale, so it exists on every platform,
/// unlike named locales such as de_DE.
class CommaGroupingNumpunct final : public std::numpunct<char> {
protected:
  char do_decimal_point() const override {
    return ',';
  }
  char do_thousands_sep() const override {
    return '.';
  }
  std::string do_grouping() const override {
    return "\3";
  }
};

/// Restores the global C++ locale and the C locale on scope exit, so a test
/// that installs a host-style locale cannot leak it into later tests.
class GlobalLocaleGuard {
public:
  GlobalLocaleGuard() : cpp_locale_(std::locale()), c_locale_(currentCLocale()) {}
  ~GlobalLocaleGuard() {
    // std::locale::global also resets the C locale when the saved locale is
    // named, so restore the C locale last.
    std::locale::global(cpp_locale_);
    std::setlocale(LC_ALL, c_locale_.c_str());
  }
  GlobalLocaleGuard(const GlobalLocaleGuard&) = delete;
  GlobalLocaleGuard& operator=(const GlobalLocaleGuard&) = delete;

  /// Installs a global C++ locale that formats 1234.5 as "1.234,5".
  static void installCommaGroupingCppLocale() {
    std::locale::global(std::locale(std::locale::classic(), new CommaGroupingNumpunct));
  }

  /// Sets the C locale to the first available comma-decimal locale. Returns
  /// false when the host has none installed.
  static bool installCommaDecimalCLocale() {
    for (const char* name : {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "German_Germany.1252"}) {
      if (std::setlocale(LC_ALL, name) != nullptr) {
        return true;
      }
    }
    return false;
  }

private:
  static std::string currentCLocale() {
    const char* name = std::setlocale(LC_ALL, nullptr);
    return name != nullptr ? std::string(name) : std::string("C");
  }

  std::locale cpp_locale_;
  std::string c_locale_;
};

} // namespace orpheus::test
