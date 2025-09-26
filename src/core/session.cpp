#include "orpheus/session.h"

#include <sstream>

namespace orpheus {

SessionState DeserializeSession(const std::string &blob) {
  SessionState state;
  std::istringstream stream(blob);
  std::string line;
  while (std::getline(stream, line)) {
    auto delimiter = line.find(':');
    if (delimiter == std::string::npos) {
      continue;
    }
    SessionEvent event;
    event.type = line.substr(0, delimiter);
    event.payload = line.substr(delimiter + 1);
    state.events.push_back(event);
  }
  return state;
}

std::string SerializeSession(const SessionState &state) {
  std::ostringstream stream;
  bool first = true;
  for (const auto &event : state.events) {
    if (!first) {
      stream << '\n';
    }
    first = false;
    stream << event.type << ':' << event.payload;
  }
  return stream.str();
}

}  // namespace orpheus
