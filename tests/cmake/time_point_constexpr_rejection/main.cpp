// SPDX-License-Identifier: MIT
#include <orpheus/time_domain.h>

#include <cstdint>
#include <type_traits>

// A dependent constant-expression requirement avoids relying on compiler notes
// for the failure oracle. Both GCC and Clang/MSVC put the assertion message on
// the actual error diagnostic when the invalid expression is rejected.
template <typename T>
concept AcceptsInvalidConstantTimePoint =
    requires { typename std::integral_constant<int64_t, T::fromSeconds(0x1p63, 1).samples()>; };

static_assert(AcceptsInvalidConstantTimePoint<orpheus::TimePoint>,
              "InvalidTimePointConstantExpression");

int main() {}
