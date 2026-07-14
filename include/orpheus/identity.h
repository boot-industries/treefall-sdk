// SPDX-License-Identifier: MIT
#pragma once

// ORP134 G2: stable identity primitives.
//
// SessionGraph now uses these IDs for its public transaction/snapshot contract.
// Pointer-returning methods remain control-thread conveniences for the legacy C
// ABI, while persistence, undo, and cross-thread handoff use stable IDs.
//
// Properties:
//  * Opaque strong typedefs over uint64_t — SessionId and ClipId do not
//    convert to each other or to integers implicitly.
//  * 0 is the universal "invalid" value; valid IDs start at 1.
//  * Trivially copyable, hashable, ordered, constexpr-friendly — usable as
//    map keys and POD message payloads (safe on realtime paths).
//  * Deterministic allocation: IdAllocator hands out monotonically increasing
//    values from an explicit seed. No wall clock, no global RNG — the same
//    session build replays to the same IDs (SDK determinism rule).

#include <cstdint>
#include <functional>

namespace orpheus {

/// Opaque, stable, serializable identifier. Tag makes each instantiation a
/// distinct type: StrongId<ClipIdTag> never mixes with StrongId<TrackIdTag>.
template <typename Tag> class StrongId {
public:
  /// Default-constructed IDs are invalid (raw value 0).
  constexpr StrongId() = default;

  /// Rehydrate an ID from its serialized raw value.
  static constexpr StrongId fromRaw(uint64_t raw) {
    return StrongId(raw);
  }

  /// The invalid sentinel (raw value 0).
  static constexpr StrongId invalid() {
    return StrongId(0);
  }

  /// Serialized representation (stable across processes and sessions).
  constexpr uint64_t raw() const {
    return m_value;
  }

  constexpr bool isValid() const {
    return m_value != 0;
  }

  friend constexpr bool operator==(StrongId a, StrongId b) {
    return a.m_value == b.m_value;
  }
  friend constexpr bool operator!=(StrongId a, StrongId b) {
    return a.m_value != b.m_value;
  }
  friend constexpr bool operator<(StrongId a, StrongId b) {
    return a.m_value < b.m_value;
  }
  friend constexpr bool operator>(StrongId a, StrongId b) {
    return a.m_value > b.m_value;
  }
  friend constexpr bool operator<=(StrongId a, StrongId b) {
    return a.m_value <= b.m_value;
  }
  friend constexpr bool operator>=(StrongId a, StrongId b) {
    return a.m_value >= b.m_value;
  }

private:
  constexpr explicit StrongId(uint64_t value) : m_value(value) {}
  uint64_t m_value = 0;
};

// The identity vocabulary (ORP134 G2). AutomationLaneId is forward-provisioned
// for sample-accurate automation (ORP135 B1).
using SessionId = StrongId<struct SessionIdTag>;
using TrackId = StrongId<struct TrackIdTag>;
using ClipId = StrongId<struct ClipIdTag>;
using MediaId = StrongId<struct MediaIdTag>;
using AutomationLaneId = StrongId<struct AutomationLaneIdTag>;

/// Deterministic, monotonic ID source. One allocator per ID space (typically
/// per session document). Not thread-safe by design — allocate on the
/// document/control thread; IDs are immutable values once handed out.
template <typename Id> class IdAllocator {
public:
  /// @param nextRaw First raw value to hand out (>= 1). Serialize this
  ///        watermark with the document so reloads keep allocating above
  ///        every existing ID.
  constexpr explicit IdAllocator(uint64_t nextRaw = 1) : m_next(nextRaw == 0 ? 1 : nextRaw) {}

  Id allocate() {
    return Id::fromRaw(m_next++);
  }

  /// Watermark to persist: the next raw value that would be handed out.
  constexpr uint64_t nextRaw() const {
    return m_next;
  }

  /// Bump the watermark above an ID observed during deserialization.
  void reserveThrough(Id id) {
    if (id.raw() >= m_next) {
      m_next = id.raw() + 1;
    }
  }

private:
  uint64_t m_next;
};

} // namespace orpheus

// Hash support so IDs work as unordered_map keys out of the box.
template <typename Tag> struct std::hash<orpheus::StrongId<Tag>> {
  size_t operator()(orpheus::StrongId<Tag> id) const noexcept {
    return std::hash<uint64_t>{}(id.raw());
  }
};
