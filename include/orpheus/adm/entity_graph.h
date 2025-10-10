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

class Bed {
public:
  ORPHEUS_API explicit Bed(EntityEnvelope envelope);
  ORPHEUS_API ~Bed();

  [[nodiscard]] ORPHEUS_API const EntityEnvelope& envelope() const;
  ORPHEUS_API void add_channel(BedChannel channel);
  [[nodiscard]] ORPHEUS_API const std::vector<BedChannel>& channels() const;

private:
  EntityEnvelope envelope_;
  std::vector<BedChannel> channels_;
};

class Object {
public:
  ORPHEUS_API explicit Object(EntityEnvelope envelope);
  ORPHEUS_API ~Object();

  [[nodiscard]] ORPHEUS_API const EntityEnvelope& envelope() const;
  ORPHEUS_API void add_point(ObjectPoint point);
  [[nodiscard]] ORPHEUS_API std::vector<ObjectPoint> trajectory(ThinningPolicy policy) const;

private:
  EntityEnvelope envelope_;
  std::vector<ObjectPoint> points_;
};

class Content {
public:
  ORPHEUS_API explicit Content(EntityEnvelope envelope);
  ORPHEUS_API ~Content();

  [[nodiscard]] ORPHEUS_API const EntityEnvelope& envelope() const;
  ORPHEUS_API void attach_bed(std::size_t bed_index);
  ORPHEUS_API void attach_object(std::size_t object_index);

  [[nodiscard]] ORPHEUS_API const std::vector<std::size_t>& beds() const;
  [[nodiscard]] ORPHEUS_API const std::vector<std::size_t>& objects() const;

private:
  EntityEnvelope envelope_;
  std::vector<std::size_t> beds_;
  std::vector<std::size_t> objects_;
};

class Programme {
public:
  ORPHEUS_API explicit Programme(EntityEnvelope envelope);
  ORPHEUS_API ~Programme();

  [[nodiscard]] ORPHEUS_API const EntityEnvelope& envelope() const;
  ORPHEUS_API void attach_content(std::size_t content_index);
  [[nodiscard]] ORPHEUS_API const std::vector<std::size_t>& contents() const;

private:
  EntityEnvelope envelope_;
  std::vector<std::size_t> contents_;
};

class EntityGraph {
public:
  ORPHEUS_API EntityGraph();
  ORPHEUS_API ~EntityGraph();

  [[nodiscard]] ORPHEUS_API Programme& add_programme(EntityEnvelope envelope);
  [[nodiscard]] ORPHEUS_API Content& add_content(EntityEnvelope envelope);
  [[nodiscard]] ORPHEUS_API Bed& add_bed(EntityEnvelope envelope);
  [[nodiscard]] ORPHEUS_API Object& add_object(EntityEnvelope envelope);

  ORPHEUS_API void link_programme_to_content(const Programme& programme, const Content& content);
  ORPHEUS_API void link_content_to_bed(const Content& content, const Bed& bed);
  ORPHEUS_API void link_content_to_object(const Content& content, const Object& object);

  [[nodiscard]] ORPHEUS_API const Programme& programme_at(std::size_t index) const;
  [[nodiscard]] ORPHEUS_API Programme& programme_at(std::size_t index);
  [[nodiscard]] ORPHEUS_API const Content& content_at(std::size_t index) const;
  [[nodiscard]] ORPHEUS_API Content& content_at(std::size_t index);
  [[nodiscard]] ORPHEUS_API const Bed& bed_at(std::size_t index) const;
  [[nodiscard]] ORPHEUS_API Bed& bed_at(std::size_t index);
  [[nodiscard]] ORPHEUS_API const Object& object_at(std::size_t index) const;
  [[nodiscard]] ORPHEUS_API Object& object_at(std::size_t index);

  [[nodiscard]] ORPHEUS_API std::size_t programme_count() const;
  [[nodiscard]] ORPHEUS_API std::size_t content_count() const;
  [[nodiscard]] ORPHEUS_API std::size_t bed_count() const;
  [[nodiscard]] ORPHEUS_API std::size_t object_count() const;

  [[nodiscard]] ORPHEUS_API std::string DebugDumpJson(ThinningPolicy policy) const;

private:
  [[nodiscard]] std::size_t programme_index(const Programme& programme) const;
  [[nodiscard]] std::size_t content_index(const Content& content) const;
  [[nodiscard]] std::size_t bed_index(const Bed& bed) const;
  [[nodiscard]] std::size_t object_index(const Object& object) const;

  std::deque<Programme> programmes_;
  std::deque<Content> contents_;
  std::deque<Bed> beds_;
  std::deque<Object> objects_;
};

[[nodiscard]] ORPHEUS_API std::string_view ToString(EntityKind kind);
[[nodiscard]] ORPHEUS_API std::string DebugDumpEnvelope(const EntityEnvelope& envelope);
[[nodiscard]] ORPHEUS_API std::vector<ObjectPoint>
ThinTrajectory(const std::vector<ObjectPoint>& points);

} // namespace orpheus::core::adm
