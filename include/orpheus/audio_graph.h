// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G3: graph-neutral routing seam.
//
// A descriptive vocabulary — sources, processors, buses, sinks, sends, taps,
// channel layouts — layered BENEATH the existing routing matrix, which is
// deliberately NOT replaced (ORP134 §7 "Do Not Touch Yet"; the full graph
// engine is an ORP135 candidate). The deliverable is the SEAM:
//
//  * GraphDescription — a validated, serializable description of an audio
//    topology, neutral to any app's mental model.
//  * makeSoundboardGraph() — the soundboard FACADE: expresses the transport's
//    current hard-wired topology (N stereo clip sources → group buses →
//    master bus → stereo sink) as one instance of the neutral vocabulary.
//  * toRoutingConfig() — maps a soundboard-shaped description onto the
//    existing IRoutingMatrix configuration, proving the seam is real: the
//    facade's translation must equal the config the transport builds today
//    (asserted by tests/routing/audio_graph_test.cpp).
//
// Downstream payoff: FourTrack expresses track/input/monitor/export buses,
// FreqFinder attaches analysis TAPS, and Clip Composer keeps its cue/master/
// group facade — all in one vocabulary, without touching the matrix engine.
//
// Everything here is descriptive data (control-thread, allocating); nothing
// executes on the audio thread.

#include <orpheus/routing_matrix.h>

#include <cstdint>
#include <string>
#include <vector>

namespace orpheus::graph {

/// Node identifier within one GraphDescription (0 = invalid).
using NodeId = uint32_t;

/// What a node does.
enum class NodeKind : uint8_t {
  Source = 0,    ///< Produces audio (clip voice, live input, generator)
  Processor = 1, ///< Transforms audio in place (FX; ORP135 B4 territory)
  Bus = 2,       ///< Sums inputs (group, master, cue, monitor)
  Sink = 3,      ///< Consumes audio (device output, file writer, analyzer)
};

/// Channel layout carried by a node's output.
enum class ChannelLayout : uint8_t {
  Mono = 1,
  Stereo = 2,
  Quad = 4,
  FiveOne = 6,
  SevenOne = 8,
};

constexpr uint16_t channelCount(ChannelLayout layout) {
  return static_cast<uint16_t>(layout);
}

/// How a connection carries audio.
enum class ConnectionKind : uint8_t {
  Direct = 0, ///< Primary signal path (source→bus, bus→bus, bus→sink)
  Send = 1,   ///< Post-node auxiliary feed at send gain (cue/FX sends)
  Tap = 2,    ///< Unity-gain observation point (metering/analysis; FreqFinder)
};

/// One node in the description.
struct NodeDesc {
  NodeId id = 0;
  NodeKind kind = NodeKind::Bus;
  ChannelLayout layout = ChannelLayout::Stereo;
  std::string name;
};

/// One edge in the description.
struct Connection {
  NodeId from = 0;
  NodeId to = 0;
  ConnectionKind kind = ConnectionKind::Direct;
  float gainDb = 0.0f; ///< Send gain (Direct/Tap connections leave this at 0)
};

/// A validated, serializable audio topology.
struct GraphDescription {
  std::vector<NodeDesc> nodes;
  std::vector<Connection> connections;

  /// Structural validation: unique nonzero node ids; every connection joins
  /// existing nodes; sources have no inputs; sinks have no outputs; taps
  /// originate only from buses or sources.
  /// @return true when structurally sound (error, if any, in outError)
  bool validate(std::string* outError = nullptr) const;

  const NodeDesc* findNode(NodeId id) const;
};

/// The soundboard facade's shape parameters — mirrors the topology the
/// transport hard-wires today (transport_controller.cpp routing setup).
struct SoundboardTopology {
  size_t numClipSources = 32; ///< Stereo clip voices (transport MAX_ACTIVE_CLIPS)
  uint8_t numGroups = 4;      ///< Group buses (soundboard default)
  uint8_t numOutputs = 2;     ///< Device sink channels (stereo master out)
};

/// Build the soundboard topology as a neutral graph: numClipSources stereo
/// Sources → group Buses (all sources default to group 0, matching the
/// transport) → master Bus → device Sink.
GraphDescription makeSoundboardGraph(const SoundboardTopology& topology);

/// Map a soundboard-shaped graph onto the existing routing matrix
/// configuration. Channels = stereo pairs per Source (the matrix's
/// SourceChannelPolicy::StereoPairs model); groups = non-master Buses;
/// outputs = Sink channel count.
RoutingConfig toRoutingConfig(const GraphDescription& graph);

} // namespace orpheus::graph
