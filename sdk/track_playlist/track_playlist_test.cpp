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

TEST(TrackPlaylistTest, SerializeAndDeserializeRoundTrip) {
  std::string chunk = "PLAYLISTS 2 1\nOne|L1|L2\nTwo|L3\n";
  Track t = Track::Deserialize(chunk);
  EXPECT_EQ(t.Serialize(), chunk);

  std::vector<bool> actives;
  t.EnumTrackPlaylists(
      [&](const Playlist &, bool active) { actives.push_back(active); });
  EXPECT_EQ(actives, std::vector<bool>({false, true}));
}

TEST(TrackPlaylistTest, DuplicatePlaylistToNewTrack) {
  std::string chunk = "PLAYLISTS 2 1\nOne|L1|L2\nTwo|L3\n";
  Track t = Track::Deserialize(chunk);
  Track dup = t.DuplicatePlaylistToNewTrack(1);
  const Playlist *p = dup.GetPlaylist(0);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->name, "Two");
  EXPECT_EQ(p->lanes, std::vector<std::string>({"L3"}));
}

TEST(TrackPlaylistTest, ConsolidatePlaylistsToNewTrack) {
  std::string chunk = "PLAYLISTS 2 1\nOne|L1|L2\nTwo|L3\n";
  Track t = Track::Deserialize(chunk);
  Track cons = t.ConsolidatePlaylistsToNewTrack();
  const Playlist *c = cons.GetPlaylist(0);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->name, "Consolidated");
  EXPECT_EQ(c->lanes, std::vector<std::string>({"L1", "L2", "L3"}));
}

TEST(TrackPlaylistTest, DeserializeBadHeader) {
  std::string chunk = "NOTPLAYLISTS 1 0\nFoo|L1\n";
  Track t = Track::Deserialize(chunk);
  EXPECT_EQ(t.Serialize(), Track().Serialize());
}

TEST(TrackPlaylistTest, DeserializeCRLF) {
  std::string chunk =
      "PLAYLISTS 2 1\r\nOne|L1|L2\r\nTwo|L3\r\n";
  Track t = Track::Deserialize(chunk);
  std::string expected = "PLAYLISTS 2 1\nOne|L1|L2\nTwo|L3\n";
  EXPECT_EQ(t.Serialize(), expected);
  const Playlist *p = t.GetPlaylist(1);
  ASSERT_NE(p, nullptr);
  ASSERT_EQ(p->lanes.size(), 1u);
  EXPECT_EQ(p->lanes[0], "L3");
}
