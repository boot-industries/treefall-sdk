// SPDX-License-Identifier: MIT
#include <orpheus/abi.h>

// ORP134 G2 installed-header compile gate: the identity/time/media headers
// must be self-contained when consumed from an installed SDK.
#include <orpheus/audio_file_writer.h>
#include <orpheus/identity.h>
#include <orpheus/media_model.h>
#include <orpheus/time_domain.h>

int main() {
  uint32_t major = 0;
  uint32_t minor = 0;
  // ORP134 G2: exercise the installed identity/time primitives.
  const orpheus::ClipId clip = orpheus::ClipId::fromRaw(1);
  const orpheus::TimeRange range =
      orpheus::TimeRange::fromStartLength(orpheus::TimePoint::fromSamples(0), 48000);
  if (!clip.isValid() || range.length() != 48000) {
    return 1;
  }

  const auto* session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, &major, &minor);
  return session == nullptr || major != ORPHEUS_ABI_MAJOR || minor != ORPHEUS_ABI_MINOR;
}
