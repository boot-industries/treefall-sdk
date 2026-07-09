// SPDX-License-Identifier: MIT
#include <orpheus/audio_graph.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace orpheus::graph {

const NodeDesc* GraphDescription::findNode(NodeId id) const {
  for (const auto& node : nodes) {
    if (node.id == id) {
      return &node;
    }
  }
  return nullptr;
}

bool GraphDescription::validate(std::string* outError) const {
  auto fail = [&](const std::string& message) {
    if (outError != nullptr) {
      *outError = message;
    }
    return false;
  };

  std::unordered_set<NodeId> ids;
  for (const auto& node : nodes) {
    if (node.id == 0) {
      return fail("node id 0 is reserved as invalid");
    }
    if (!ids.insert(node.id).second) {
      return fail("duplicate node id " + std::to_string(node.id));
    }
  }

  for (const auto& connection : connections) {
    const NodeDesc* from = findNode(connection.from);
    const NodeDesc* to = findNode(connection.to);
    if (from == nullptr || to == nullptr) {
      return fail("connection references unknown node");
    }
    if (to->kind == NodeKind::Source) {
      return fail("source node " + std::to_string(to->id) + " cannot have inputs");
    }
    if (from->kind == NodeKind::Sink) {
      return fail("sink node " + std::to_string(from->id) + " cannot have outputs");
    }
    if (connection.kind == ConnectionKind::Tap && from->kind == NodeKind::Processor) {
      return fail("taps observe sources or buses, not processors");
    }
  }

  return true;
}

GraphDescription makeSoundboardGraph(const SoundboardTopology& topology) {
  GraphDescription graph;
  NodeId nextId = 1;

  // Group buses first so sources can connect to group 0.
  std::vector<NodeId> groupIds;
  for (uint8_t group = 0; group < topology.numGroups; ++group) {
    NodeDesc bus;
    bus.id = nextId++;
    bus.kind = NodeKind::Bus;
    bus.layout = ChannelLayout::Stereo;
    bus.name = "group " + std::to_string(group);
    groupIds.push_back(bus.id);
    graph.nodes.push_back(std::move(bus));
  }

  NodeDesc master;
  master.id = nextId++;
  master.kind = NodeKind::Bus;
  master.layout = ChannelLayout::Stereo;
  master.name = "master";
  const NodeId masterId = master.id;
  graph.nodes.push_back(std::move(master));

  NodeDesc sink;
  sink.id = nextId++;
  sink.kind = NodeKind::Sink;
  sink.layout = topology.numOutputs >= 2 ? ChannelLayout::Stereo : ChannelLayout::Mono;
  sink.name = "device out";
  const NodeId sinkId = sink.id;
  graph.nodes.push_back(std::move(sink));

  // Clip sources — all default-routed to group 0, exactly like the
  // transport's constructor (setChannelGroup(ch, 0) for every clip pair).
  for (size_t clip = 0; clip < topology.numClipSources; ++clip) {
    NodeDesc source;
    source.id = nextId++;
    source.kind = NodeKind::Source;
    source.layout = ChannelLayout::Stereo;
    source.name = "clip " + std::to_string(clip);
    graph.nodes.push_back(source);
    if (!groupIds.empty()) {
      graph.connections.push_back({source.id, groupIds.front(), ConnectionKind::Direct, 0.0f});
    }
  }

  for (NodeId groupId : groupIds) {
    graph.connections.push_back({groupId, masterId, ConnectionKind::Direct, 0.0f});
  }
  graph.connections.push_back({masterId, sinkId, ConnectionKind::Direct, 0.0f});

  return graph;
}

RoutingConfig toRoutingConfig(const GraphDescription& graph) {
  RoutingConfig config;

  size_t sourceCount = 0;
  size_t groupBusCount = 0;
  uint16_t sinkChannels = 0;
  for (const auto& node : graph.nodes) {
    switch (node.kind) {
    case NodeKind::Source:
      ++sourceCount;
      break;
    case NodeKind::Bus:
      // The master bus is the matrix's implicit mix stage, not a group.
      if (node.name != "master") {
        ++groupBusCount;
      }
      break;
    case NodeKind::Sink:
      sinkChannels = std::max(sinkChannels, channelCount(node.layout));
      break;
    case NodeKind::Processor:
      break; // Processor nodes await the graph engine (ORP135 B4)
    }
  }

  // Stereo pair per source — the matrix's SourceChannelPolicy::StereoPairs
  // model the transport uses today (2 routing channels per clip voice).
  config.num_channels = static_cast<uint8_t>(std::min<size_t>(sourceCount * 2, 255));
  config.num_groups = static_cast<uint8_t>(std::min<size_t>(groupBusCount, 255));
  config.num_outputs = static_cast<uint8_t>(std::min<uint16_t>(sinkChannels, 255));
  return config;
}

} // namespace orpheus::graph
