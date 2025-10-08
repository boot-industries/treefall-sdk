# SPDX-License-Identifier: MIT
include_guard(GLOBAL)

# When the SDK is installed, this helper is expected to live alongside the
# generated package config. The find_package smoke test mimics that structure by
# staging a copy inside its source tree. Defer to the canonical implementation
# so we keep warning policy in exactly one place.
include(${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/CompilerWarnings.cmake)
