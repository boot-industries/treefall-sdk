/* SPDX-License-Identifier: MIT */
#include <treefall/abi.h>
#include <treefall/abi_version.h>
#include <treefall/errors.h>

#include <stdint.h>
#include <string.h>

static int logger_calls;
static int telemetry_calls;

static void logger(treefall_log_level level, const char* message, void* user_data) {
  (void)level;
  (void)message;
  if (user_data == &logger_calls) {
    ++logger_calls;
  }
}

static void telemetry(const char* event_name, const char* json_payload, void* user_data) {
  (void)event_name;
  (void)json_payload;
  if (user_data == &telemetry_calls) {
    ++telemetry_calls;
  }
}

int main(void) {
  uint32_t major = 0;
  uint32_t minor = 0;
  const treefall_session_api_v1* treefall_session =
      treefall_session_abi_v1(TREEFALL_ABI_MAJOR, &major, &minor);
  if (treefall_session == 0 || major != TREEFALL_ABI_MAJOR || minor != TREEFALL_ABI_MINOR) {
    return 1;
  }
  const orpheus_session_api_v1* legacy_session = orpheus_session_abi_v1(ORPHEUS_ABI_MAJOR, 0, 0);
  if (legacy_session == 0 || (const void*)treefall_session != (const void*)legacy_session) {
    return 2;
  }
  if (treefall_session_abi_v1(TREEFALL_ABI_MAJOR + 1u, &major, &minor) != 0 ||
      major != TREEFALL_ABI_MAJOR || minor != TREEFALL_ABI_MINOR) {
    return 3;
  }

  const treefall_clipgrid_api_v1* treefall_clipgrid =
      treefall_clipgrid_abi_v1(TREEFALL_ABI_MAJOR, 0, 0);
  const treefall_render_api_v1* treefall_render = treefall_render_abi_v1(TREEFALL_ABI_MAJOR, 0, 0);
  if (treefall_clipgrid == 0 || treefall_render == 0) {
    return 4;
  }

  treefall_session_handle session = 0;
  if (treefall_session->create(&session) != TREEFALL_STATUS_OK || session == 0) {
    return 5;
  }
  orpheus_session_handle legacy_handle = (orpheus_session_handle)session;
  if (legacy_session->set_tempo(legacy_handle, 120.0) != ORPHEUS_STATUS_OK) {
    legacy_session->destroy(legacy_handle);
    return 6;
  }
  legacy_session->destroy(legacy_handle);

  orpheus_session_handle old_session = 0;
  if (legacy_session->create(&old_session) != ORPHEUS_STATUS_OK || old_session == 0) {
    return 7;
  }
  treefall_session->destroy((treefall_session_handle)old_session);

  treefall_set_logger(logger, &logger_calls);
  orpheus_set_logger(0, 0);
  treefall_set_telemetry_callback(telemetry, &telemetry_calls);
  orpheus_set_telemetry_callback(0, 0);
  if (strcmp(treefall_status_to_string(TREEFALL_STATUS_OK),
             orpheus_status_to_string(ORPHEUS_STATUS_OK)) != 0) {
    return 8;
  }
  treefall_set_logger(0, 0);
  treefall_set_telemetry_callback(0, 0);
  return 0;
}
