// SPDX-License-Identifier: MIT
#include <treefall/realtime_diagnostics.h>

#include <type_traits>

int main() {
  static_assert(std::is_trivially_copyable_v<treefall::RealtimeDiagnosticsSnapshot>);
  treefall::RealtimeDiagnostics diagnostics;
  diagnostics.recordCallback(64, 48000);
  const auto snapshot = diagnostics.snapshot();
  return snapshot.callback_count == 1 && snapshot.last_buffer_frames == 64 &&
                 snapshot.last_sample_rate == 48000
             ? 0
             : 1;
}
