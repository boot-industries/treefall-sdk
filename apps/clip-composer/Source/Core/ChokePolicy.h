// SPDX-License-Identifier: MIT

#pragma once

#include "Session/SessionManager.h"

namespace occ {

/// OCC151 T8 / G6: "Stop Others On Play" (choke) scoping.
///
/// The canonical model: a clip's choke is scoped to its playgroup — the
/// clipGroup (0-3) it is assigned to — NOT global and NOT the visible tab. The
/// SDK owns no playgroup concept (it only offers the host-neutral stopOtherClips
/// primitive), so OCC decides membership here.
///
/// This is the single, pure decision point: given the firing clip and one other
/// clip, should the choke stop the other clip? A clip in group A must never stop
/// a clip in group B.
///
/// @param firing        The clip being fired (its stopOthersEnabled flag is
///                      assumed already checked by the caller).
/// @param firingGlobalIndex The firing clip's global slot index.
/// @param other         A candidate clip to potentially stop.
/// @param otherGlobalIndex  The candidate's global slot index.
/// @param otherIsActive True if the candidate is currently playing or stopping.
/// @return true if the choke should stop the other clip.
inline bool shouldChokeStop(const SessionManager::ClipData& firing, int firingGlobalIndex,
                            const SessionManager::ClipData& other, int otherGlobalIndex,
                            bool otherIsActive) {
  // Never choke yourself.
  if (otherGlobalIndex == firingGlobalIndex)
    return false;

  // Only stop clips that are actually making sound.
  if (!otherIsActive)
    return false;

  // Playgroup scoping: same clipGroup only. This is the whole point of T8 —
  // group A does not stop group B.
  return other.clipGroup == firing.clipGroup;
}

} // namespace occ
