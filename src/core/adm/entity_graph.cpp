// SPDX-License-Identifier: MIT
#include "orpheus/adm/entity_graph.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace orpheus::core::adm {
namespace {
constexpr double kSlopeTolerance = 1e-7;

bool IsRedundantPoint(const ObjectPoint& previous, const ObjectPoint& current,
                      const ObjectPoint& next) {
  if (current.time_seconds <= previous.time_seconds || next.time_seconds <= current.time_seconds) {
    return false;
  }

  const double delta_prev = current.time_seconds - previous.time_seconds;
  const double delta_next = next.time_seconds - current.time_seconds;
  if (delta_prev <= 0.0 || delta_next <= 0.0) {
    return false;
  }

  const auto slope_matches = [&](double a, double b, double c) {
    const double slope_prev = (b - a) / delta_prev;
    const double slope_next = (c - b) / delta_next;
    return std::abs(slope_prev - slope_next) <= kSlopeTolerance;
  };

  return slope_matches(previous.x, current.x, next.x) &&
         slope_matches(previous.y, current.y, next.y) &&
         slope_matches(previous.z, current.z, next.z);
}

std::string EscapeJson(std::string_view input) {
  std::ostringstream oss;
  for (const char ch : input) {
    switch (ch) {
    case '\\':
      oss << "\\\\";
      break;
    case '\"':
      oss << "\\\"";
      break;
    case '\n':
      oss << "\\n";
      break;
    case '\r':
      oss << "\\r";
      break;
    case '\t':
      oss << "\\t";
      break;
    default:
      if (static_cast<unsigned char>(ch) < 0x20u) {
        oss << "\\u00";
        constexpr char kHexDigits[] = "0123456789ABCDEF";
        const unsigned char byte = static_cast<unsigned char>(ch);
        oss << kHexDigits[(byte >> 4) & 0x0F];
        oss << kHexDigits[byte & 0x0F];
      } else {
        oss << ch;
      }
      break;
    }
  }
  return oss.str();
}

void AppendEnvelopeJson(std::ostringstream& oss, const EntityEnvelope& envelope) {
  oss << "\"envelope\":{";
  oss << "\"id\":\"" << EscapeJson(envelope.id) << "\",";
  oss << "\"name\":\"" << EscapeJson(envelope.name) << "\",";
  oss << "\"kind\":\"" << EscapeJson(std::string(ToString(envelope.kind))) << "\"}";
}

} // namespace

Bed::Bed(EntityEnvelope envelope) : envelope_(std::move(envelope)) {}

Bed::~Bed() = default;

const EntityEnvelope& Bed::envelope() const {
  return envelope_;
}

void Bed::add_channel(BedChannel channel) {
  channels_.push_back(std::move(channel));
}

const std::vector<BedChannel>& Bed::channels() const {
  return channels_;
}

Object::Object(EntityEnvelope envelope) : envelope_(std::move(envelope)) {}

Object::~Object() = default;

const EntityEnvelope& Object::envelope() const {
  return envelope_;
}

void Object::add_point(ObjectPoint point) {
  points_.push_back(point);
}

std::vector<ObjectPoint> Object::trajectory(ThinningPolicy policy) const {
  if (policy == ThinningPolicy::kEnabled) {
    return ThinTrajectory(points_);
  }
  return points_;
}

Content::Content(EntityEnvelope envelope) : envelope_(std::move(envelope)) {}

Content::~Content() = default;

const EntityEnvelope& Content::envelope() const {
  return envelope_;
}

void Content::attach_bed(std::size_t bed_index) {
  if (std::find(beds_.begin(), beds_.end(), bed_index) == beds_.end()) {
    beds_.push_back(bed_index);
  }
}

void Content::attach_object(std::size_t object_index) {
  if (std::find(objects_.begin(), objects_.end(), object_index) == objects_.end()) {
    objects_.push_back(object_index);
  }
}

const std::vector<std::size_t>& Content::beds() const {
  return beds_;
}

const std::vector<std::size_t>& Content::objects() const {
  return objects_;
}

Programme::Programme(EntityEnvelope envelope) : envelope_(std::move(envelope)) {}

Programme::~Programme() = default;

EntityGraph::EntityGraph() = default;

EntityGraph::~EntityGraph() = default;

const EntityEnvelope& Programme::envelope() const {
  return envelope_;
}

void Programme::attach_content(std::size_t content_index) {
  if (std::find(contents_.begin(), contents_.end(), content_index) == contents_.end()) {
    contents_.push_back(content_index);
  }
}

const std::vector<std::size_t>& Programme::contents() const {
  return contents_;
}

Programme& EntityGraph::add_programme(EntityEnvelope envelope) {
  programmes_.emplace_back(std::move(envelope));
  return programmes_.back();
}

Content& EntityGraph::add_content(EntityEnvelope envelope) {
  contents_.emplace_back(std::move(envelope));
  return contents_.back();
}

Bed& EntityGraph::add_bed(EntityEnvelope envelope) {
  beds_.emplace_back(std::move(envelope));
  return beds_.back();
}

Object& EntityGraph::add_object(EntityEnvelope envelope) {
  objects_.emplace_back(std::move(envelope));
  return objects_.back();
}

void EntityGraph::link_programme_to_content(const Programme& programme, const Content& content) {
  programmes_.at(programme_index(programme)).attach_content(content_index(content));
}

void EntityGraph::link_content_to_bed(const Content& content, const Bed& bed) {
  contents_.at(content_index(content)).attach_bed(bed_index(bed));
}

void EntityGraph::link_content_to_object(const Content& content, const Object& object) {
  contents_.at(content_index(content)).attach_object(object_index(object));
}

const Programme& EntityGraph::programme_at(std::size_t index) const {
  return programmes_.at(index);
}

Programme& EntityGraph::programme_at(std::size_t index) {
  return programmes_.at(index);
}

const Content& EntityGraph::content_at(std::size_t index) const {
  return contents_.at(index);
}

Content& EntityGraph::content_at(std::size_t index) {
  return contents_.at(index);
}

const Bed& EntityGraph::bed_at(std::size_t index) const {
  return beds_.at(index);
}

Bed& EntityGraph::bed_at(std::size_t index) {
  return beds_.at(index);
}

const Object& EntityGraph::object_at(std::size_t index) const {
  return objects_.at(index);
}

Object& EntityGraph::object_at(std::size_t index) {
  return objects_.at(index);
}

std::size_t EntityGraph::programme_count() const {
  return programmes_.size();
}

std::size_t EntityGraph::content_count() const {
  return contents_.size();
}

std::size_t EntityGraph::bed_count() const {
  return beds_.size();
}

std::size_t EntityGraph::object_count() const {
  return objects_.size();
}

std::string EntityGraph::DebugDumpJson(ThinningPolicy policy) const {
  std::ostringstream oss;
  oss << "{";

  oss << "\"programmes\":[";
  for (std::size_t i = 0; i < programmes_.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const Programme& programme = programmes_[i];
    oss << "{";
    AppendEnvelopeJson(oss, programme.envelope());
    oss << ",\"contents\":[";
    for (std::size_t j = 0; j < programme.contents().size(); ++j) {
      if (j > 0) {
        oss << ",";
      }
      const std::size_t content_idx = programme.contents()[j];
      oss << "\"" << EscapeJson(contents_.at(content_idx).envelope().id) << "\"";
    }
    oss << "]}";
  }
  oss << "],";

  oss << "\"contents\":[";
  for (std::size_t i = 0; i < contents_.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const Content& content = contents_[i];
    oss << "{";
    AppendEnvelopeJson(oss, content.envelope());
    oss << ",\"beds\":[";
    for (std::size_t j = 0; j < content.beds().size(); ++j) {
      if (j > 0) {
        oss << ",";
      }
      oss << "\"" << EscapeJson(beds_.at(content.beds()[j]).envelope().id) << "\"";
    }
    oss << "],\"objects\":[";
    for (std::size_t j = 0; j < content.objects().size(); ++j) {
      if (j > 0) {
        oss << ",";
      }
      oss << "\"" << EscapeJson(objects_.at(content.objects()[j]).envelope().id) << "\"";
    }
    oss << "]}";
  }
  oss << "],";

  oss << "\"beds\":[";
  for (std::size_t i = 0; i < beds_.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const Bed& bed = beds_[i];
    oss << "{";
    AppendEnvelopeJson(oss, bed.envelope());
    oss << ",\"channels\":[";
    for (std::size_t j = 0; j < bed.channels().size(); ++j) {
      if (j > 0) {
        oss << ",";
      }
      const BedChannel& channel = bed.channels()[j];
      oss << "{\"id\":\"" << EscapeJson(channel.id) << "\",";
      oss << "\"name\":\"" << EscapeJson(channel.name) << "\"}";
    }
    oss << "]}";
  }
  oss << "],";

  oss << "\"objects\":[";
  for (std::size_t i = 0; i < objects_.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    const Object& object = objects_[i];
    oss << "{";
    AppendEnvelopeJson(oss, object.envelope());
    oss << ",\"trajectory\":[";
    const std::vector<ObjectPoint> trajectory = object.trajectory(policy);
    for (std::size_t j = 0; j < trajectory.size(); ++j) {
      if (j > 0) {
        oss << ",";
      }
      const ObjectPoint& point = trajectory[j];
      oss << "{\"time\":" << point.time_seconds << ",";
      oss << "\"x\":" << point.x << ",";
      oss << "\"y\":" << point.y << ",";
      oss << "\"z\":" << point.z << "}";
    }
    oss << "]}";
  }
  oss << "]";

  oss << "}";
  return oss.str();
}

std::size_t EntityGraph::programme_index(const Programme& programme) const {
  for (std::size_t i = 0; i < programmes_.size(); ++i) {
    if (&programmes_[i] == &programme) {
      return i;
    }
  }
  throw std::invalid_argument("Programme does not belong to graph");
}

std::size_t EntityGraph::content_index(const Content& content) const {
  for (std::size_t i = 0; i < contents_.size(); ++i) {
    if (&contents_[i] == &content) {
      return i;
    }
  }
  throw std::invalid_argument("Content does not belong to graph");
}

std::size_t EntityGraph::bed_index(const Bed& bed) const {
  for (std::size_t i = 0; i < beds_.size(); ++i) {
    if (&beds_[i] == &bed) {
      return i;
    }
  }
  throw std::invalid_argument("Bed does not belong to graph");
}

std::size_t EntityGraph::object_index(const Object& object) const {
  for (std::size_t i = 0; i < objects_.size(); ++i) {
    if (&objects_[i] == &object) {
      return i;
    }
  }
  throw std::invalid_argument("Object does not belong to graph");
}

std::string_view ToString(EntityKind kind) {
  switch (kind) {
  case EntityKind::kProgramme:
    return "programme";
  case EntityKind::kContent:
    return "content";
  case EntityKind::kBed:
    return "bed";
  case EntityKind::kObject:
    return "object";
  }
  return "unknown";
}

std::string DebugDumpEnvelope(const EntityEnvelope& envelope) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"id\":\"" << EscapeJson(envelope.id) << "\",";
  oss << "\"name\":\"" << EscapeJson(envelope.name) << "\",";
  oss << "\"kind\":\"" << EscapeJson(std::string(ToString(envelope.kind))) << "\"}";
  return oss.str();
}

std::vector<ObjectPoint> ThinTrajectory(const std::vector<ObjectPoint>& points) {
  if (points.size() <= 2) {
    return points;
  }

  std::vector<ObjectPoint> result;
  result.reserve(points.size());
  result.push_back(points.front());
  for (std::size_t i = 1; i + 1 < points.size(); ++i) {
    const ObjectPoint& prev = result.back();
    const ObjectPoint& curr = points[i];
    const ObjectPoint& next = points[i + 1];
    if (!IsRedundantPoint(prev, curr, next)) {
      result.push_back(curr);
    }
  }
  result.push_back(points.back());
  return result;
}

} // namespace orpheus::core::adm
