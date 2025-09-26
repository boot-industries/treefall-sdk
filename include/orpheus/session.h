#pragma once

#include <string>
#include <vector>

namespace orpheus {

struct SessionEvent {
  std::string type;
  std::string payload;
};

struct SessionState {
  std::vector<SessionEvent> events;
};

SessionState DeserializeSession(const std::string &blob);
std::string SerializeSession(const SessionState &state);

}  // namespace orpheus
