// SPDX-License-Identifier: MIT
#include <orpheus/abi.h>

int main() {
  uint32_t major = 0;
  uint32_t minor = 0;
  const auto *session =
      orpheus_session_abi_v1(ORPHEUS_ABI_V1_MAJOR, &major, &minor);
  return session == nullptr || major != ORPHEUS_ABI_V1_MAJOR ||
         minor != ORPHEUS_ABI_V1_MINOR;
}
