# Clip Composer Launch & Kill Procedures

## Launch Procedure

### Option 1: Direct Executable (Recommended for Testing)
```bash
cd /Users/chrislyons/dev/orpheus-sdk
build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app/Contents/MacOS/OrpheusClipComposer
```

### Option 2: macOS Open Command
```bash
cd /Users/chrislyons/dev/orpheus-sdk
open build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app
```

### Option 3: One-Liner from Anywhere
```bash
/Users/chrislyons/dev/orpheus-sdk/build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app/Contents/MacOS/OrpheusClipComposer
```

## Build Procedure (if needed)

```bash
cd /Users/chrislyons/dev/orpheus-sdk
cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --target orpheus_clip_composer_app
```

## Kill Procedures

### Option 1: Kill by Process Name
```bash
pkill -f OrpheusClipComposer
```

### Option 2: Kill by PID (if you know it)
```bash
# Find the PID
ps aux | grep OrpheusClipComposer | grep -v grep

# Kill specific PID
kill <PID>

# Force kill if needed
kill -9 <PID>
```

### Option 3: Kill All JUCE Apps (Nuclear Option)
```bash
killall OrpheusClipComposer
```

## Quick Reference

**Launch:**
```bash
cd ~/dev/orpheus-sdk && build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app/Contents/MacOS/OrpheusClipComposer
```

**Kill:**
```bash
pkill -f OrpheusClipComposer
```

## Troubleshooting

### If you see JUCE assertion failures
These may appear in the console but the app might still run. Check if the GUI window appeared.

### If the app doesn't appear
- Check Activity Monitor for OrpheusClipComposer process
- Try killing and relaunching
- Rebuild if necessary using the build procedure above

### If rebuild is needed
```bash
cd ~/dev/orpheus-sdk
rm -rf build
cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=ON
cmake --build build --target orpheus_clip_composer_app
```

---

**Last Updated:** 2025-10-13
