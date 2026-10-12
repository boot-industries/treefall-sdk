// SPDX-License-Identifier: MIT
#include <orpheus/time_domain.h>

#include <limits>

constexpr auto minimum = orpheus::TimePoint::fromSeconds(-0x1p63, 1);
constexpr auto maximum_double_below_limit =
    orpheus::TimePoint::fromSeconds(0x1.fffffffffffffp62, 1);
static_assert(minimum.samples() == std::numeric_limits<int64_t>::min());
static_assert(maximum_double_below_limit.samples() == 9223372036854774784LL);
static_assert(orpheus::TimePoint::fromSeconds(-1.5, 1).samples() == -2);
static_assert(orpheus::TimePoint::fromSeconds(1.5, 1).samples() == 2);

int main() {}
