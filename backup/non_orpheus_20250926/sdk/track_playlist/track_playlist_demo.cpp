#include "track_playlist.h"
#include <iostream>

using namespace rpr;

int main() {
  Track t;
  auto verse = t.CreateTrackPlaylist("Verse");
  auto chorus = t.CreateTrackPlaylist("Chorus");
  t.SetActiveTrackPlaylist(chorus);

  std::cout << "Playlists:\n";
  t.EnumTrackPlaylists([](const Playlist &pl, bool active) {
    std::cout << (active ? "* " : "  ") << pl.name << "\n";
  });

  std::string chunk = t.Serialize();
  std::cout << "\nSerialized:\n" << chunk << "\n";

  Track dup = t.DuplicatePlaylistToNewTrack(verse);
  std::cout << "Duplicated Track:\n" << dup.Serialize() << "\n";

  Track cons = t.ConsolidatePlaylistsToNewTrack();
  std::cout << "Consolidated Track:\n" << cons.Serialize() << "\n";
  return 0;
}

