// SPDX-License-Identifier: MIT
#pragma once

#include "orpheus/adapters/reaper/export.h"

#ifdef __cplusplus
extern "C" {
#endif

REAPER_ORPHEUS_API const char *ReaperExtensionName(void);
REAPER_ORPHEUS_API const char *ReaperExtensionVersion(void);
REAPER_ORPHEUS_API const char *ReaperExtensionPanelText(void);
REAPER_ORPHEUS_API int OrpheusTogglePanel(void);
REAPER_ORPHEUS_API int OrpheusImportSession(const char *path);
REAPER_ORPHEUS_API int OrpheusRenderClickToFile(const char *path);

#ifdef __cplusplus
}
#endif

