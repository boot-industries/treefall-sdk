// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

#define ORPHEUS_ABI_V1_MAJOR 1u
#define ORPHEUS_ABI_V1_MINOR 0u

#if defined(__cplusplus)
#define ORPHEUS_ABI_EXPECT(major_literal)                                            \
  static_assert((major_literal) == ORPHEUS_ABI_V1_MAJOR,                            \
                "Orpheus ABI major mismatch; rebuild against the expected version")
#else
#define ORPHEUS_ABI_EXPECT(major_literal)                                            \
  _Static_assert((major_literal) == ORPHEUS_ABI_V1_MAJOR,                           \
                 "Orpheus ABI major mismatch; rebuild against the expected version")
#endif

#define ORPHEUS_ABI_CAP_NONE 0ull

#define ORPHEUS_SESSION_CAP_V1_CORE (1ull << 0)
#define ORPHEUS_CLIPGRID_CAP_V1_CORE (1ull << 0)
#define ORPHEUS_RENDER_CAP_V1_CORE (1ull << 0)
