// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "orpheus/export.h"
#include "session_graph.h"

namespace orpheus::core::session_json {

ORPHEUS_API SessionGraph ParseSession(const std::string &json_text);
ORPHEUS_API std::string SerializeSession(const SessionGraph &session);

ORPHEUS_API SessionGraph LoadSessionFromFile(const std::string &path);
ORPHEUS_API void SaveSessionToFile(const SessionGraph &session,
                                   const std::string &path);

ORPHEUS_API std::string MakeRenderStemFilename(const std::string &session_name,
                                               const std::string &stem_name,
                                               std::uint32_t sample_rate_hz,
                                               std::uint32_t bit_depth_bits);
ORPHEUS_API std::string MakeRenderClickFilename(const std::string &session_name,
                                                const std::string &stem_name,
                                                std::uint32_t sample_rate_hz,
                                                std::uint32_t bit_depth_bits);

}  // namespace orpheus::core::session_json
