#include "track_playlist.h"
#include <gtest/gtest.h>

using namespace rpr;

TEST(TrackPlaylistTest, CreateAndSetActive) {
  Track t;
  auto first = t.CreateTrackPlaylist("First");
  auto second = t.CreateTrackPlaylist("Second");
  EXPECT_EQ(first, 0);
  EXPECT_EQ(second, 1);

  std::vector<std::string> names;
  std::vector<bool> actives;
  t.EnumTrackPlaylists([&](const Playlist &pl, bool active) {
    names.push_back(pl.name);
    actives.push_back(active);
  });
  EXPECT_EQ(names, std::vector<std::string>({"First", "Second"}));
  EXPECT_EQ(actives, std::vector<bool>({true, false}));

  EXPECT_TRUE(t.SetActiveTrackPlaylist(second));
  actives.clear();
  t.EnumTrackPlaylists(
      [&](const Playlist &, bool active) { actives.push_back(active); });
  EXPECT_EQ(actives, std::vector<bool>({false, true}));
}

TEST(TrackPlaylistTest, SerializeDeserializeDuplicateConsolidate) {
  std::string chunk = "PLAYLISTS 2 1\nOne|L1|L2\nTwo|L3\n";
  Track t = Track::Deserialize(chunk);
  EXPECT_EQ(t.Serialize(), chunk);

  std::vector<bool> actives;
  t.EnumTrackPlaylists(
      [&](const Playlist &, bool active) { actives.push_back(active); });
  EXPECT_EQ(actives, std::vector<bool>({false, true}));

  Track dup = t.DuplicatePlaylistToNewTrack(1);
  const Playlist *p = dup.GetPlaylist(0);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->name, "Two");
  EXPECT_EQ(p->lanes, std::vector<std::string>({"L3"}));

  Track cons = t.ConsolidatePlaylistsToNewTrack();
  const Playlist *c = cons.GetPlaylist(0);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->name, "Consolidated");
  EXPECT_EQ(c->lanes, std::vector<std::string>({"L1", "L2", "L3"}));
}
