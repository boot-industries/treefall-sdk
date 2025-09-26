#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "session_graph.h"

namespace orpheus::core::session_json {

SessionGraph ParseSession(const std::string &json_text);
std::string SerializeSession(const SessionGraph &session);

SessionGraph LoadSessionFromFile(const std::string &path);
void SaveSessionToFile(const SessionGraph &session, const std::string &path);

std::string MakeRenderClickFilename(const std::string &session_name,
                                    double tempo_bpm, std::uint32_t bars);

}  // namespace orpheus::core::session_json
