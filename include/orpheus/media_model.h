// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G2: media/region vocabulary and app-shaped aggregates.
//
// Thin value structs over the identity (identity.h) and time (time_domain.h)
// primitives. They give the downstream apps a shared, serializable shape for
// "a piece of media placed in time" without prescribing an engine model:
//
//  * MediaRegion — a window into a media file (FreqFinder analysis regions,
//    FourTrack punch ranges, Clip Composer trims).
//  * Take — one recorded pass targeting a track (FourTrack; ORP135 B2
//    take/comping builds on this).
//  * ClipSlot / LauncherScene — launcher-grid vocabulary (Clip Composer's
//    tabs/buttons; scene_manager.h remains the session-level snapshot API).
//
// Everything here is additive; the pointer-based SessionGraph is untouched
// (ORP134 §7 "Do Not Touch Yet").

#include <orpheus/identity.h>
#include <orpheus/time_domain.h>

#include <cstdint>
#include <string>
#include <vector>

namespace orpheus {

/// A region within a media item, in the MEDIA file's own sample timeline.
struct MediaRegion {
  MediaId media;         ///< Which media item (invalid() = unresolved)
  TimeRange sourceRange; ///< Window into that media, media-rate samples

  friend bool operator==(const MediaRegion& a, const MediaRegion& b) {
    return a.media == b.media && a.sourceRange == b.sourceRange;
  }
  friend bool operator!=(const MediaRegion& a, const MediaRegion& b) {
    return !(a == b);
  }
};

/// One recorded pass. Policy (comping, crossfades, latency compensation)
/// stays host-side; this is just the stable, serializable record of what was
/// captured where.
struct Take {
  ClipId id;          ///< Stable identity of this take
  TrackId track;      ///< Track the take targets
  MediaRegion region; ///< Captured media + usable window
  TimePoint placedAt; ///< Timeline position (session-rate samples)
  uint32_t index = 0; ///< Take number within its lane (1-based; 0 = unset)
  std::string name;   ///< Host-facing label
};

/// One cell in a launcher grid (page/tab, row, column) holding a clip.
struct ClipSlot {
  ClipId clip;         ///< Occupant (invalid() = empty slot)
  uint16_t page = 0;   ///< Tab/page index
  uint16_t row = 0;    ///< Grid row
  uint16_t column = 0; ///< Grid column
};

/// A named arrangement of launcher slots (a "scene" in launcher terms).
/// Complements ISceneManager: SceneSnapshot captures live session state,
/// LauncherScene is the serializable document-model shape.
struct LauncherScene {
  std::string name;
  std::vector<ClipSlot> slots;
};

} // namespace orpheus
