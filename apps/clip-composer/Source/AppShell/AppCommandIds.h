// SPDX-License-Identifier: MIT

#pragma once

namespace occ::AppCommandIds {

constexpr int newSession = 0x2000;
constexpr int openSession = 0x2001;
constexpr int saveSession = 0x2002;
constexpr int saveSessionAs = 0x2003;
constexpr int revertSession = 0x2004;
constexpr int quitApplication = 0x2005;
constexpr int clearRecentSessions = 0x2006;
constexpr int toggleFullscreen = 0x2007;
constexpr int showKeyboardShortcuts = 0x2008;
constexpr int showAbout = 0x2009;
constexpr int toggleRestoreLastSession = 0x200A;
constexpr int showAudioSettings = 0x200B;
constexpr int minimiseWindow = 0x200C;
constexpr int zoomWindow = 0x200D;
constexpr int bringAllToFront = 0x200E;
constexpr int openRecentSessionBase = 0x2100;
constexpr int maxRecentSessionItems = 12;

} // namespace occ::AppCommandIds
