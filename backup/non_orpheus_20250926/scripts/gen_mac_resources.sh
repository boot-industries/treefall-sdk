#!/usr/bin/env bash
set -euo pipefail

# Generate macOS resource files (res.rc_mac_dlg and res.rc_mac_menu)
# for each plug-in with a res.rc file. This uses the WDL/swell tools.

# Resolve repository root and WDL path
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
WDL_DIR="${WDL_PATH:-$REPO_ROOT/WDL}"

# WDL repo contains an inner WDL directory with sources
SWELL_DIR="$WDL_DIR/WDL/swell"

DLGGEN="$SWELL_DIR/swell_dlggen"
MENU="$SWELL_DIR/swell_menu"
RESGEN="$SWELL_DIR/swell_resgen.sh"

if [[ ! ( -x "$DLGGEN" && -x "$MENU" ) && ! -x "$RESGEN" ]]; then
  echo "warning: swell tools not found in $SWELL_DIR, skipping resource generation" >&2
  exit 0
fi

for rc in "$REPO_ROOT"/reaper-plugins/*/res.rc; do
  [ -e "$rc" ] || continue
  plugindir=$(dirname "$rc")
  if [[ -x "$DLGGEN" && -x "$MENU" ]]; then
    "$DLGGEN" "$rc" "$plugindir/res.rc_mac_dlg"
    "$MENU" "$rc" "$plugindir/res.rc_mac_menu"
  elif [[ -x "$RESGEN" ]]; then
    "$RESGEN" "$rc"
  fi
done
