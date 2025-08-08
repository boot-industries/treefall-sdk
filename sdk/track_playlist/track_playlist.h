#pragma once
#include <string>
#include <vector>
#include <functional>

namespace rpr {

struct Playlist {
  std::string name;
  std::vector<std::string> lanes;
};

class Track {
public:
  using ID = int;
  Track() = default;

  // API: create a new playlist and return its ID
  ID CreateTrackPlaylist(const std::string &name);

  // API: set active playlist by ID
  bool SetActiveTrackPlaylist(ID id);

  // API: enumerate playlists, callback receives playlist and active flag
  void EnumTrackPlaylists(const std::function<void(const Playlist &, bool)> &fn) const;

  // Serialize playlists to a track state chunk for persistence
  std::string Serialize() const;
  // Restore playlists from a state chunk
  static Track Deserialize(const std::string &chunk);

  // Helper functions
  Track DuplicatePlaylistToNewTrack(ID id) const;
  Track ConsolidatePlaylistsToNewTrack() const;

  const Playlist *GetPlaylist(ID id) const;

private:
  std::vector<Playlist> playlists_;
  ID active_index_ = -1;
};

} // namespace rpr

