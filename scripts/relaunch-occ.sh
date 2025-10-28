#!/bin/bash
# Relaunch Orpheus Clip Composer with clean logs

# Kill existing instance
killall OrpheusClipComposer 2>/dev/null

# Clear diagnostic logs
rm /tmp/audio_callback.txt 2>/dev/null
rm /tmp/occ_output.log 2>/dev/null

# Launch new instance in background
/Users/chrislyons/dev/orpheus-sdk/build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app/Contents/MacOS/OrpheusClipComposer > /tmp/occ_output.log 2>&1 &

echo "Orpheus Clip Composer relaunched (PID: $!)"
echo "Logs: /tmp/occ_output.log, /tmp/audio_callback.txt"
