// SPDX-License-Identifier: MIT
#include <json_io.h>

int main() {
  orpheus::core::SessionGraph session;
  return orpheus::core::session_json::SerializeSession(session).empty() ? 1 : 0;
}
