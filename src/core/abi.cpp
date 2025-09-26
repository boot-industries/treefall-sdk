#include "orpheus/abi.h"

#include <algorithm>
#include <sstream>

namespace orpheus {

std::string ToString(const AbiVersion &version) {
  std::ostringstream oss;
  oss << version.major << "." << version.minor;
  return oss.str();
}

AbiVersion NegotiateAbi(const AbiVersion &requested) {
  if (requested.major != kCurrentAbi.major) {
    return kCurrentAbi;
  }

  AbiVersion result = kCurrentAbi;
  result.minor = std::min(requested.minor, kCurrentAbi.minor);
  return result;
}

}  // namespace orpheus
