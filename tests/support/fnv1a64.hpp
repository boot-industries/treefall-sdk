// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>

namespace orpheus::tests::support {

inline constexpr std::uint64_t kFnv1a64Offset = 14695981039346656037ull;
inline constexpr std::uint64_t kFnv1a64Prime = 1099511628211ull;

constexpr std::uint64_t Fnv1a64(const std::uint8_t* data, std::size_t size,
                                std::uint64_t seed = kFnv1a64Offset) {
  std::uint64_t hash = seed;
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= static_cast<std::uint64_t>(data[i]);
    hash *= kFnv1a64Prime;
  }
  return hash;
}

template <typename Container>
constexpr std::uint64_t Fnv1a64(const Container& container, std::uint64_t seed = kFnv1a64Offset) {
  return Fnv1a64(reinterpret_cast<const std::uint8_t*>(container.data()),
                 container.size() * sizeof(typename Container::value_type), seed);
}

} // namespace orpheus::tests::support
