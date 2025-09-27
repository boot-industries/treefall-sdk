// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "orpheus/export.h"

namespace orpheus::core::adm {

enum class EntityKind {
  kProgramme,
  kContent,
  kBed,
  kObject,
};

struct EntityEnvelope {
  std::string id;
  std::string name;
  EntityKind kind;
};

struct BedChannel {
  std::string id;
  std::string name;
};

struct ObjectPoint {
  double time_seconds = 0.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

enum class ThinningPolicy {
  kDisabled,
  kEnabled,
};

class ORPHEUS_API Bed {
 public:
  explicit Bed(EntityEnvelope envelope);

  const EntityEnvelope &envelope() const;
  void add_channel(BedChannel channel);
  const std::vector<BedChannel> &channels() const;

 private:
  EntityEnvelope envelope_;
  std::vector<BedChannel> channels_;
};

class ORPHEUS_API Object {
 public:
  explicit Object(EntityEnvelope envelope);

  const EntityEnvelope &envelope() const;
  void add_point(ObjectPoint point);
  std::vector<ObjectPoint> trajectory(ThinningPolicy policy) const;

 private:
  EntityEnvelope envelope_;
  std::vector<ObjectPoint> points_;
};

class ORPHEUS_API Content {
 public:
  explicit Content(EntityEnvelope envelope);

  const EntityEnvelope &envelope() const;
  void attach_bed(std::size_t bed_index);
  void attach_object(std::size_t object_index);

  const std::vector<std::size_t> &beds() const;
  const std::vector<std::size_t> &objects() const;

 private:
  EntityEnvelope envelope_;
  std::vector<std::size_t> beds_;
  std::vector<std::size_t> objects_;
};

class ORPHEUS_API Programme {
 public:
  explicit Programme(EntityEnvelope envelope);

  const EntityEnvelope &envelope() const;
  void attach_content(std::size_t content_index);
  const std::vector<std::size_t> &contents() const;

 private:
  EntityEnvelope envelope_;
  std::vector<std::size_t> contents_;
};

class ORPHEUS_API EntityGraph {
 public:
  Programme &add_programme(EntityEnvelope envelope);
  Content &add_content(EntityEnvelope envelope);
  Bed &add_bed(EntityEnvelope envelope);
  Object &add_object(EntityEnvelope envelope);

  void link_programme_to_content(const Programme &programme,
                                 const Content &content);
  void link_content_to_bed(const Content &content, const Bed &bed);
  void link_content_to_object(const Content &content, const Object &object);

  const Programme &programme_at(std::size_t index) const;
  Programme &programme_at(std::size_t index);
  const Content &content_at(std::size_t index) const;
  Content &content_at(std::size_t index);
  const Bed &bed_at(std::size_t index) const;
  Bed &bed_at(std::size_t index);
  const Object &object_at(std::size_t index) const;
  Object &object_at(std::size_t index);

  std::size_t programme_count() const;
  std::size_t content_count() const;
  std::size_t bed_count() const;
  std::size_t object_count() const;

  std::string DebugDumpJson(ThinningPolicy policy) const;

 private:
  std::size_t programme_index(const Programme &programme) const;
  std::size_t content_index(const Content &content) const;
  std::size_t bed_index(const Bed &bed) const;
  std::size_t object_index(const Object &object) const;

  std::deque<Programme> programmes_;
  std::deque<Content> contents_;
  std::deque<Bed> beds_;
  std::deque<Object> objects_;
};

ORPHEUS_API std::string_view ToString(EntityKind kind);
ORPHEUS_API std::string DebugDumpEnvelope(const EntityEnvelope &envelope);
ORPHEUS_API std::vector<ObjectPoint> ThinTrajectory(
    const std::vector<ObjectPoint> &points);

}  // namespace orpheus::core::adm
