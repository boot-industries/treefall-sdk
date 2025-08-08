#include "track_playlist.h"
#include <sstream>
#include <algorithm>

namespace rpr {

Track::ID Track::CreateTrackPlaylist(const std::string &name) {
  playlists_.push_back({name, {}});
  if (active_index_ == -1)
    active_index_ = 0;
  return static_cast<ID>(playlists_.size() - 1);
}

bool Track::SetActiveTrackPlaylist(ID id) {
  if (id >= 0 && id < static_cast<ID>(playlists_.size())) {
    active_index_ = id;
    return true;
  }
  return false;
}

void Track::EnumTrackPlaylists(const std::function<void(const Playlist &, bool)> &fn) const {
  for (ID i = 0; i < static_cast<ID>(playlists_.size()); ++i)
    fn(playlists_[i], i == active_index_);
}

std::string Track::Serialize() const {
  std::ostringstream oss;
  oss << "PLAYLISTS " << playlists_.size() << " " << active_index_ << "\n";
  for (const auto &pl : playlists_) {
    oss << pl.name;
    for (const auto &lane : pl.lanes)
      oss << "|" << lane;
    oss << "\n";
  }
  return oss.str();
}

Track Track::Deserialize(const std::string &chunk) {
  Track t;
  std::istringstream iss(chunk);
  std::string header;
  size_t count = 0;
  iss >> header >> count >> t.active_index_;
  std::string line;
  std::getline(iss, line); // consume endline
  for (size_t i = 0; i < count && std::getline(iss, line); ++i) {
    std::istringstream pl(line);
    std::string name;
    std::getline(pl, name, '|');
    Playlist p{name};
    std::string lane;
    while (std::getline(pl, lane, '|'))
      p.lanes.push_back(lane);
    t.playlists_.push_back(std::move(p));
  }
  return t;
}

Track Track::DuplicatePlaylistToNewTrack(ID id) const {
  Track t;
  if (const Playlist *p = GetPlaylist(id)) {
    t.playlists_.push_back(*p);
    t.active_index_ = 0;
  }
  return t;
}

Track Track::ConsolidatePlaylistsToNewTrack() const {
  Track t;
  Playlist merged{"Consolidated"};
  for (const auto &pl : playlists_)
    merged.lanes.insert(merged.lanes.end(), pl.lanes.begin(), pl.lanes.end());
  t.playlists_.push_back(std::move(merged));
  t.active_index_ = 0;
  return t;
}

const Playlist *Track::GetPlaylist(ID id) const {
  if (id >= 0 && id < static_cast<ID>(playlists_.size()))
    return &playlists_[id];
  return nullptr;
}

} // namespace rpr

