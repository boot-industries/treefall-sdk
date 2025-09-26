#pragma once

#include <string>

namespace orpheus {

struct AbiVersion {
  int major{};
  int minor{};
};

constexpr AbiVersion kCurrentAbi{1, 0};

std::string ToString(const AbiVersion &version);
AbiVersion NegotiateAbi(const AbiVersion &requested);

}  // namespace orpheus
