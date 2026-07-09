// SPDX-License-Identifier: MIT
// ORP134 G3: graph-neutral routing seam tests.
//
// The seam's proof: the soundboard facade expressed in the neutral
// vocabulary must translate to EXACTLY the routing configuration the
// transport hard-wires today (transport_controller.cpp: MAX_ACTIVE_CLIPS*2
// channels, 4 groups, stereo out) — and the vocabulary must also express
// the downstream shapes (FourTrack buses, FreqFinder taps) that motivated it.

#include <orpheus/audio_graph.h>
#include <orpheus/routing_matrix.h>

#include <gtest/gtest.h>
#include <string>

using namespace orpheus;
using namespace orpheus::graph;

TEST(AudioGraphTest, SoundboardFacadeMatchesTransportTopology) {
  // The transport's constructor values: 32 active clips x stereo pairs,
  // 4 groups, stereo output.
  SoundboardTopology topology;
  topology.numClipSources = 32;
  topology.numGroups = 4;
  topology.numOutputs = 2;

  GraphDescription graph = makeSoundboardGraph(topology);
  std::string error;
  ASSERT_TRUE(graph.validate(&error)) << error;

  // 32 sources + 4 group buses + master + sink
  EXPECT_EQ(graph.nodes.size(), 32u + 4u + 1u + 1u);
  // 32 source->group + 4 group->master + master->sink
  EXPECT_EQ(graph.connections.size(), 32u + 4u + 1u);

  RoutingConfig config = toRoutingConfig(graph);
  EXPECT_EQ(config.num_channels, 64); // MAX_ACTIVE_CLIPS * 2 (stereo pairs)
  EXPECT_EQ(config.num_groups, 4);    // soundboard default
  EXPECT_EQ(config.num_outputs, 2);   // stereo master out

  // The translated config must initialize the REAL matrix.
  auto matrix = createRoutingMatrix();
  ASSERT_NE(matrix, nullptr);
  EXPECT_EQ(matrix->initialize(config), SessionGraphError::OK);
}

TEST(AudioGraphTest, ValidateCatchesStructuralErrors) {
  GraphDescription graph;
  graph.nodes.push_back({1, NodeKind::Source, ChannelLayout::Stereo, "clip"});
  graph.nodes.push_back({2, NodeKind::Sink, ChannelLayout::Stereo, "out"});

  // Dangling connection target.
  graph.connections.push_back({1, 99, ConnectionKind::Direct, 0.0f});
  std::string error;
  EXPECT_FALSE(graph.validate(&error));
  EXPECT_NE(error.find("unknown node"), std::string::npos);

  // Sources cannot have inputs.
  graph.connections.clear();
  graph.connections.push_back({2, 1, ConnectionKind::Direct, 0.0f});
  EXPECT_FALSE(graph.validate(&error));

  // Duplicate ids.
  graph.connections.clear();
  graph.nodes.push_back({1, NodeKind::Bus, ChannelLayout::Stereo, "dup"});
  EXPECT_FALSE(graph.validate(&error));
  EXPECT_NE(error.find("duplicate"), std::string::npos);

  // Id 0 reserved.
  GraphDescription zero;
  zero.nodes.push_back({0, NodeKind::Bus, ChannelLayout::Stereo, "bad"});
  EXPECT_FALSE(zero.validate(&error));
}

TEST(AudioGraphTest, VocabularyExpressesFourTrackBuses) {
  // FourTrack shape: N mono track sources -> track bus -> master; a monitor
  // bus fed by a SEND; an export sink fed from master.
  GraphDescription graph;
  NodeId next = 1;
  const NodeId trackBus = next++;
  const NodeId monitorBus = next++;
  const NodeId master = next++;
  const NodeId device = next++;
  const NodeId exportSink = next++;
  graph.nodes.push_back({trackBus, NodeKind::Bus, ChannelLayout::Stereo, "tracks"});
  graph.nodes.push_back({monitorBus, NodeKind::Bus, ChannelLayout::Stereo, "monitor"});
  graph.nodes.push_back({master, NodeKind::Bus, ChannelLayout::Stereo, "master"});
  graph.nodes.push_back({device, NodeKind::Sink, ChannelLayout::Stereo, "device out"});
  graph.nodes.push_back({exportSink, NodeKind::Sink, ChannelLayout::Stereo, "export"});

  for (int track = 0; track < 4; ++track) {
    const NodeId source = next++;
    graph.nodes.push_back(
        {source, NodeKind::Source, ChannelLayout::Mono, "track " + std::to_string(track)});
    graph.connections.push_back({source, trackBus, ConnectionKind::Direct, 0.0f});
    graph.connections.push_back({source, monitorBus, ConnectionKind::Send, -6.0f});
  }
  graph.connections.push_back({trackBus, master, ConnectionKind::Direct, 0.0f});
  graph.connections.push_back({master, device, ConnectionKind::Direct, 0.0f});
  graph.connections.push_back({master, exportSink, ConnectionKind::Direct, 0.0f});

  std::string error;
  EXPECT_TRUE(graph.validate(&error)) << error;
}

TEST(AudioGraphTest, VocabularyExpressesFreqFinderTaps) {
  // FreqFinder shape: one source -> master -> device, with a unity TAP from
  // the master bus into an analyzer sink.
  GraphDescription graph;
  graph.nodes.push_back({1, NodeKind::Source, ChannelLayout::Stereo, "file"});
  graph.nodes.push_back({2, NodeKind::Bus, ChannelLayout::Stereo, "master"});
  graph.nodes.push_back({3, NodeKind::Sink, ChannelLayout::Stereo, "device out"});
  graph.nodes.push_back({4, NodeKind::Sink, ChannelLayout::Stereo, "analyzer"});
  graph.connections.push_back({1, 2, ConnectionKind::Direct, 0.0f});
  graph.connections.push_back({2, 3, ConnectionKind::Direct, 0.0f});
  graph.connections.push_back({2, 4, ConnectionKind::Tap, 0.0f});

  std::string error;
  EXPECT_TRUE(graph.validate(&error)) << error;

  // Taps must originate from buses/sources, never processors.
  graph.nodes.push_back({5, NodeKind::Processor, ChannelLayout::Stereo, "eq"});
  graph.connections.push_back({5, 4, ConnectionKind::Tap, 0.0f});
  EXPECT_FALSE(graph.validate(&error));
  EXPECT_NE(error.find("taps"), std::string::npos);
}
