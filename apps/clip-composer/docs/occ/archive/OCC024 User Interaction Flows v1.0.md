# OCC024 User Interaction Flows v1.0

**Document Version:** 1.0
**Date:** October 12, 2025
**Status:** Draft
**Supersedes:** None

---

## Executive Summary

This document defines the complete user interaction flows for Orpheus Clip Composer, detailing step-by-step workflows for the four primary user personas identified in OCC021:

1. **Broadcast Engineer** - Fast clip triggering, track preview, logging
2. **Theater Sound Designer** - Precise editing, master/slave linking, cue sequences
3. **Installation Technician** - Long-term stability, remote monitoring, external triggers
4. **Live Performance Musician** - Real-time tempo adjustment, flexible routing, MIDI control

**Design Philosophy:** Intuitive by default, powerful when needed. Established mouse logic, keyboard shortcuts for all critical operations, minimal clicks to achieve common tasks.

---

## Flow 1: Loading a Clip into a Button

### User Persona: Broadcast Engineer

### Goal: Quickly load a new jingle into an empty button slot

### Entry Point

User has an open session with available empty button slots on the current tab.

### Steps

**1. Select Target Button**

```
User: Right-click empty button in grid
System: Opens context menu
  • Load Audio File...
  • Record to This Button
  • Paste Clip (if clipboard has clip)
  • Button Properties
```

**2. Choose "Load Audio File..."**

```
System: Opens file picker dialog
  • Default location: Last used folder or project media folder
  • File filters: All Supported (WAV, AIFF, FLAC, MP3, AAC, OGG)
  • Preview panel on right side (SpotOn-inspired)
```

**3. Preview Track (Optional - SpotOn Feature)**

```
User: Clicks on file in file picker
System: Loads waveform preview in right panel
  • Shows waveform visualization
  • Displays duration, sample rate, format
  • Mini transport controls (play, stop)
  • Preview routes to Audition output (outputs 9-10)

User: Clicks play button
System: Plays audio through Audition output
  • User can audition before committing

User: Satisfied → clicks "Open" button
```

**4. Audio File Loaded**

```
System:
  • Copies file to project media folder (if enabled)
  • Creates clip metadata (OCC022 schema)
  • Assigns UUID
  • Sets default trim points (0, duration_samples)
  • Sets default clip group (A, or last used)
  • Renders waveform cache
  • Updates grid button visual

Grid Button Now Shows:
  • Waveform thumbnail
  • Clip name (derived from filename, editable)
  • Duration display
  • Clip group indicator (A/B/C/D colored badge)
```

**5. Quick Adjustments (Common)**

```
User: Wants to trim silence from start/end
User: Clicks button → Bottom panel switches to "Editor" mode
System: Waveform editor displays full clip waveform
User: Left-click to set IN point, Right-click to set OUT point
User: Clicks "AutoTrim" button (SPT023 feature)
System: Automatically detects silence based on -20dBFS threshold
  • Updates trim_in_samples and trim_out_samples
  • Visual markers update on waveform
```

### Success Criteria

- Clip loaded and ready to trigger in <30 seconds
- Track preview prevents loading wrong file
- AutoTrim removes manual trimming for 90% of cases

### Error Paths

**File Not Supported:**

```
System: Displays error dialog
  "Unsupported audio format: .xyz
  Supported formats: WAV, AIFF, FLAC, MP3, AAC, OGG"
User: Clicks OK → file picker remains open to select different file
```

**File Missing After Load:**

```
System: Displays warning
  "Audio file not found at expected location.
  Last known path: /path/to/file.wav

  [Locate File...] [Remove Clip] [Keep Reference]"

User: Clicks "Locate File..."
System: Searches project folder for files matching hash
  • If found: Lists candidates for user selection
  • If not found: Opens file picker to manually locate
```

---

## Flow 2: Editing Trim Points with Sample Accuracy

### User Persona: Theater Sound Designer

### Goal: Set precise IN and OUT points for a cue that must hit exact timing

### Entry Point

Clip already loaded into button. User needs to trim to exact cue point.

### Steps

**1. Open Waveform Editor**

```
User: Clicks clip button
System: Bottom panel switches to "Editor" mode
  • Full waveform displayed
  • Current trim IN (magenta line) and OUT (cyan line) visible
  • Playhead (yellow line) at position 0
  • Transport controls: Play, Pause, Stop, Loop toggle
```

**2. Navigate to Approximate Region**

```
User: Scroll wheel to zoom in on waveform
User: Click-drag to pan waveform left/right
OR
User: Middle-click on waveform to jump playhead to that position
System: Playhead jumps to clicked sample position
```

**3. Set IN Point (Left-Click)**

```
User: Left-clicks at desired IN point
System:
  • Sets trim_in_samples to clicked position
  • Magenta vertical line appears at position
  • IN time entry field updates (displays seconds)
  • Grays out waveform region before IN point
```

**4. Set OUT Point (Right-Click)**

```
User: Right-clicks at desired OUT point
System:
  • Sets trim_out_samples to clicked position
  • Cyan vertical line appears at position
  • OUT time entry field updates
  • Grays out waveform region after OUT point
```

**5. Fine-Tune with Keyboard Nudge (Sample-Accurate)**

```
User: Selects IN time entry field
User: Uses arrow keys to nudge by 1 sample
  • Left Arrow: trim_in_samples -= 1
  • Right Arrow: trim_in_samples += 1
  • Shift+Arrow: Nudge by 10 samples
  • Cmd/Ctrl+Arrow: Nudge by 100 samples

System: Updates magenta line position in real-time
```

**6. Rehearse Section (SpotOn Feature)**

```
User: Wants to hear transition into cue
User: Clicks "Play to In" button
System: Starts playback 2 seconds before trim_in_samples
  • Plays through IN point
  • Continues until user stops or reaches OUT point
  • User can hear if IN point is correct
```

**7. Add Cue Point (Optional)**

```
User: Control+click on waveform (middle of clip)
System: Creates cue point marker
  • Opens dialog: "Cue point name?"
User: Types "Chorus Start"
System: Adds cue point to edit_points.cue_points[] array
  • Visual marker appears on waveform
  • Useful for rehearsal mode or script automation
```

**8. Apply Fades**

```
User: Drags fade handle at bottom of waveform
System: Interactive fade bar appears
  • Drag left edge: Adjust fade-in duration
  • Drag right edge: Adjust fade-out duration
  • Fade curve selector: Linear, Exponential, Logarithmic, S-Curve
  • Fade visualized as gradient overlay on waveform
```

**9. Save and Close Editor**

```
User: Clicks main grid area (outside editor panel)
System: Bottom panel closes (or stays open, user preference)
  • All trim edits are auto-saved to session
  • Button waveform thumbnail updates to show trimmed region
```

### Success Criteria

- Sample-accurate trim point editing (<1ms precision)
- Visual feedback for all edits
- Rehearsal mode prevents guesswork
- Keyboard shortcuts enable rapid iteration

### Alternative Path: Numeric Time Entry

```
User: Right-clicks on IN time entry field
System: Opens numeric entry dialog
  • Displays current value in samples AND seconds
  • User can type exact time: "00:05.240" (5.24 seconds)
  • Or exact sample number: "251520" (at 48kHz = 5.24s)
System: Updates trim_in_samples
  • Validates: trim_in < trim_out
  • If invalid, shows error: "IN point must be before OUT point"
```

---

## Flow 3: Recording Directly into a Button

### User Persona: Broadcast Engineer

### Goal: Record a caller segment during live show and trigger it back immediately

### Entry Point

User is on-air, wants to capture incoming audio into a button for instant playback.

### Steps

**1. Prepare for Recording**

```
User: Right-click target empty button
System: Context menu appears
  • Record to This Button
  • Load Audio File...
  • (other options)

User: Selects "Record to This Button"
System: Opens recording configuration overlay
  • Input Source: [Record A ▼] [Record B] (dropdown)
  • Clip Name: [Caller - 555-1234] (auto-generated, editable)
  • Clip Group: [A ▼] (dropdown: A/B/C/D)
  • [Start Recording] [Cancel]
```

**2. Select Input Source**

```
User: Selects "Record A" from dropdown
System: Shows live input meter for Record A
  • Real-time level display
  • User can verify signal is present before recording
```

**3. Start Recording**

```
User: Clicks "Start Recording" button
System:
  • Button changes to recording state (red pulsing border)
  • Elapsed time counter starts: "00:00.000"
  • Waveform builds in real-time (optional visual feedback)
  • Audio written to temp file: /tmp/recording_UUID.wav
  • If another clip is playing, recording happens silently
  • If no clip playing, user hears input through monitoring
```

**4. Stop Recording**

```
User: Right-clicks button again OR presses assigned hotkey (e.g., Shift+R)
System: Stops recording
  • Temp file finalized
  • File moved to project media folder
  • Clip metadata created:
    - recorded_at: timestamp
    - recorded_by: current user
    - input_source: "Record_A"
    - ltc_timecode: (if LTC input active, else system time)
  • Button updates to show recorded clip
  • Waveform rendered and cached
```

**5. Immediate Playback (Optional)**

```
User: Clicks button immediately after recording
System: Triggers playback
  • Routes to assigned Clip Group (A/B/C/D)
  • Plays recorded audio
  • Broadcast use case: Caller segment plays on-air
```

**6. Quick Edit (Optional)**

```
User: Wants to trim awkward silence at start
User: Clicks button → Editor panel opens
User: Left-click to set IN point (skip silence)
User: Optionally applies AutoTrim
System: Updates trim points
User: Clicks button again → Plays trimmed version
```

### Success Criteria

- Recording starts in <2 seconds from user intent
- Simultaneous playback and recording supported
- Recorded clip immediately available for triggering
- Metadata captured automatically (timestamp, LTC)

### Error Paths

**No Input Signal:**

```
System: Detects no input signal for 3 seconds after recording starts
System: Displays warning overlay
  "No audio input detected on Record A.
  Check your audio interface routing.

  [Continue Recording] [Stop]"
User: Can verify routing and continue or abort
```

**Disk Full:**

```
System: Detects insufficient disk space during recording
System: Emergency stop recording
  • Saves partial recording
  • Displays error: "Disk full - recording stopped at 00:32.450"
User: Can free space and re-record if needed
```

---

## Flow 4: Configuring Audio Routing

### User Persona: Live Performance Musician

### Goal: Route Clip Group A to main outputs, Group B to in-ear monitors only

### Entry Point

User has session loaded, needs to configure custom routing.

### Steps

**1. Open Routing Panel**

```
User: Clicks "Routing" in bottom panel tabs
System: Bottom panel switches to "Routing" mode
  • Two tabs visible: [Outputs ✓] [Inputs]
  • Routing matrix grid displayed
```

**2. View Default Routing (Outputs Tab)**

```
System: Displays routing matrix

  Application Endpoints → Hardware Outputs

           Out1  Out2  Out3  Out4  Out5  Out6  Out7  Out8
  Group A   ✓     ✓     -     -     -     -     -     -
  Group B   -     -     ✓     ✓     -     -     -     -
  Group C   -     -     -     -     ✓     ✓     -     -
  Group D   -     -     -     -     -     -     ✓     ✓
  Audition  -     -     -     -     -     -     -     -

  (Audition uses fixed outputs 9-10, not shown in user routing)
```

**3. Modify Group B Routing**

```
User: Wants Group B to ONLY go to outputs 5-6 (IEM send)
User: Un-checks boxes for Out3, Out4 (row: Group B)
User: Checks boxes for Out5, Out6
System: Updates RoutingMatrix in real-time
  • Audio re-routes immediately (even during playback)
```

**4. Verify Routing**

```
User: Triggers clip in Group B
System: Audio routes to Out5-Out6 only
User: Confirms correct routing via headphones/IEM
```

**5. Configure Input Routing (Inputs Tab)**

```
User: Clicks "Inputs" tab
System: Displays input routing matrix

  Hardware Inputs → Application Endpoints

           Rec A  Rec B  LTC
  In1        ✓     -     -
  In2        ✓     -     -
  In3        -     ✓     -
  In4        -     ✓     -
  In5        -     -     ✓
  In6        -     -     -
```

**6. Save Routing as Preset (Optional)**

```
User: Clicks "Save Preset" button
System: Opens dialog
  "Preset Name: [Live Performance - IEM Setup]"
User: Types name, clicks Save
System: Stores routing configuration as JSON preset
  • Preset available in dropdown for quick recall
  • Useful for different venue setups
```

### Success Criteria

- Routing changes apply immediately (even during playback)
- Visual matrix provides clear overview
- Presets enable quick setup for recurring configurations

---

## Flow 5: Master/Slave Linking for Coordinated Cues

### User Persona: Theater Sound Designer

### Goal: Link multiple clips to trigger sequentially when master is triggered

### Entry Point

User has loaded multiple clips representing a multi-part cue (e.g., thunder → rain → wind).

### Steps

**1. Select Master Clip**

```
User: Right-click on "Thunder" clip button
System: Context menu
  • Trigger Clip
  • Edit Trim Points
  • Set as Master Clip...
  • (other options)

User: Selects "Set as Master Clip..."
System: Opens master/slave linking dialog
```

**2. Define Slave Clips**

```
System: Dialog displays
  "Master Clip: Thunder

  Select Slave Clips:
  [ ] Rain (Tab 1, Row 2, Col 4)
  [ ] Wind (Tab 1, Row 2, Col 5)
  [ ] Lightning Flash (Tab 1, Row 3, Col 1)

  Sync Mode: [Sequential ▼] (options: Sequential, Simultaneous)

  [Apply] [Cancel]"

User: Checks "Rain" and "Wind"
User: Leaves Sync Mode as "Sequential"
User: Clicks Apply
```

**3. System Updates Metadata**

```
System: Updates clip metadata (OCC022 schema)

  Thunder clip:
    linking.is_master = true
    linking.slave_clip_uuids = [uuid_rain, uuid_wind]

  Rain clip:
    linking.master_clip_uuid = uuid_thunder

  Wind clip:
    linking.master_clip_uuid = uuid_thunder
```

**4. Visual Indication of Linking**

```
System: Grid buttons update
  • Thunder button shows "M" badge (master indicator)
  • Rain and Wind buttons show chain link icon (slave indicator)
  • Optional: connecting lines drawn between linked buttons
```

**5. Trigger Master Clip**

```
User: Clicks "Thunder" button
System: ClipTriggerEngine executes sequence
  1. Thunder clip starts playing
  2. Thunder clip completes (reaches trim_out_samples)
  3. System triggers Rain clip (slave 1)
  4. Rain clip completes
  5. System triggers Wind clip (slave 2)
  6. Wind clip completes
  → Sequence done
```

**6. Modify Linking (Optional)**

```
User: Decides to add Lightning Flash to sequence
User: Right-clicks Thunder button → "Edit Master/Slave Links..."
System: Re-opens linking dialog with current selections pre-checked
User: Checks "Lightning Flash"
User: Drags items in list to reorder sequence:
  1. Rain
  2. Lightning Flash
  3. Wind
User: Clicks Apply
System: Updates linking.slave_clip_uuids order
```

### Success Criteria

- Linking setup in <1 minute
- Visual feedback shows linked relationships
- Sequential playback follows exact order
- Easy to modify linking after initial setup

### Alternative: Simultaneous Sync Mode

```
User: Sets Sync Mode to "Simultaneous"
System: All linked clips trigger AT THE SAME TIME when master triggers
  • Use case: Layered sound effects (thunder + rain + wind together)
```

---

## Flow 6: Creating a New Session

### User Persona: Broadcast Engineer

### Goal: Set up a new show session for tomorrow's morning program

### Entry Point

User launches OCC, wants to create fresh session.

### Steps

**1. New Session Creation**

```
User: File → New Session (or Cmd/Ctrl+N)
System: Opens "New Session" dialog
  "Session Name: [Morning Show - Oct 13]
  Project Media Folder: [Browse...] (optional)
  Template: [Blank ▼] (options: Blank, Broadcast Starter, Theater 8-Tab, etc.)

  [Create] [Cancel]"
```

**2. Configure Session**

```
User: Types session name
User: Clicks "Browse..." to set project media folder
System: File picker for folder selection
User: Selects folder: ~/Shows/Oct13/media/
User: Leaves template as "Broadcast Starter"
  • Pre-configures 4 Clip Groups with broadcast-typical routing
  • Sets default AutoTrim threshold: -20dBFS
  • Enables logging
User: Clicks "Create"
```

**3. System Initializes Session**

```
System: Creates session structure
  • Empty clip grid (10×12, 8 tabs)
  • Default routing:
    Group A → Out 1-2
    Group B → Out 3-4
    Group C → Out 5-6
    Group D → Out 7-8
    Audition → Out 9-10
  • Tempo: 120 BPM (default, adjustable)
  • Saves session file: ~/Shows/Oct13/MorningShow_Oct13.occ
```

**4. Populate Session**

```
User: Begins loading clips (see Flow 1)
User: Organizes clips across tabs:
  • Tab 1: Show Opener & Stingers
  • Tab 2: Music Beds
  • Tab 3: Sound Effects
  • Tab 4: Commercials
  • (etc.)
```

**5. Session Auto-Save**

```
System: Every 30 seconds (configurable), auto-saves session
  • Non-blocking incremental save
  • Writes to .occ file
  • User notification (subtle): "Session auto-saved 10:45:32 AM"
```

### Success Criteria

- New session created in <1 minute
- Templates provide sensible defaults
- Auto-save prevents data loss

---

## Flow 7: Using iOS Companion App for Remote Control

### User Persona: Live Performance Musician

### Goal: Trigger clips from iPad on stage while desktop app runs at mixing position

### Entry Point

Desktop OCC running with session loaded. User has iOS companion app installed.

### Steps

**1. Enable Remote Control on Desktop**

```
User: Opens Preferences panel (bottom panel)
User: Navigates to "Remote Control" section
System: Displays settings
  "Remote Control Server: [Enabled ✓]
  Network Mode: [WiFi (LAN) ▼] (options: WiFi, Bluetooth, Both)
  Port: [8000] (WebSocket port)
  Security: [Require PIN ✓]
  PIN: [1234] (user-configurable)

  Devices Connected: 0
  "
```

**2. Launch iOS App**

```
User: Opens "Orpheus Clip Composer Remote" on iPad
System: iOS app scans local network via Bonjour/mDNS
  • Discovers desktop instance: "Chris's MacBook Pro - Morning Show Session"
```

**3. Pair Devices**

```
User: Taps on discovered device
System: Prompts for PIN
  "Enter PIN to connect to Chris's MacBook Pro"
User: Types "1234"
System: Establishes WebSocket connection
  • Desktop app shows: "Devices Connected: 1"
  • iOS app displays: "Connected ✓"
```

**4. iOS App Syncs Session State**

```
System: Desktop sends session data via WebSocket
  • Grid layout (tabs, button positions)
  • Clip names, colors, durations
  • Current playback states
iOS App: Renders matching grid layout
  • Buttons match desktop appearance
  • Real-time status updates (playing clips highlighted)
```

**5. Trigger Clip from iOS**

```
User: Taps clip button on iPad
iOS App: Sends WebSocket message
  { "action": "trigger_clip", "uuid": "550e8400-..." }
Desktop App: Receives message
  • ClipTriggerEngine::triggerClip(uuid)
  • Clip plays through desktop audio outputs
  • iOS app receives status update: clip now playing
  • Button on iPad highlights to show active state
```

**6. Real-Time Waveform Preview (Optional)**

```
iOS App: Displays live waveform position
  • Progress bar shows playback position
  • Countdown timer: "00:42 remaining"
  • User knows when clip will complete
```

**7. Disconnect**

```
User: Closes iOS app OR moves out of WiFi range
System: WebSocket connection closes gracefully
  • Desktop app: "Devices Connected: 0"
  • No impact on audio playback (server-side remains stable)
```

### Success Criteria

- Connection established in <10 seconds
- Latency from iPad tap to audio output: <100ms
- Real-time status updates
- Graceful handling of network disruptions

---

## Flow 8: Session Recovery After Missing Files

### User Persona: Installation Technician

### Goal: Restore session after media folder was reorganized

### Entry Point

User opens existing session, but audio files have been moved.

### Steps

**1. Attempt to Load Session**

```
User: File → Open Session → Selects "Museum_Exhibit.occ"
System: Begins parsing session JSON
  • Reads clip metadata for all 120 clips
  • For each clip, checks if audio file exists at file_path
```

**2. Detect Missing Files**

```
System: Finds 15 clips with missing audio files
System: Displays dialog
  "Session Loaded with Warnings

  15 audio files could not be located at their expected paths.

  [Locate Files Automatically] [Locate Manually] [Continue Anyway]"
```

**3. Automatic Search**

```
User: Clicks "Locate Files Automatically"
System: Initiates intelligent file recovery (OCC023)
  • For each missing file, searches project folder recursively
  • Matches files by SHA-256 hash (from file_hash field)
  • Finds 12 of 15 files in new locations
```

**4. Present Results**

```
System: Displays recovery results
  "Found 12 of 15 missing files.

  [✓] intro_music.wav → media/reorganized/intro_music.wav
  [✓] ambient_loop.wav → media/reorganized/ambient_loop.wav
  ...
  [✗] missing_cue.wav (not found)
  [✗] deleted_file.wav (not found)
  [✗] old_recording.wav (not found)

  [Apply Found Files] [Skip] [Locate Remaining Manually]"
```

**5. Apply Recovered Files**

```
User: Clicks "Apply Found Files"
System: Updates clip metadata
  • file_path updated to new relative paths
  • file_path_absolute updated
  • Clips with recovered files load successfully
```

**6. Manually Locate Remaining Files (Optional)**

```
User: Clicks "Locate Remaining Manually"
System: Opens file picker for first missing file
  "Locate: missing_cue.wav
  Last known path: media/cues/missing_cue.wav"
User: Navigates to correct location, selects file
System: Verifies file hash matches (if possible)
  • If hash matches: Updates metadata
  • If hash doesn't match: Warns user "This may not be the correct file"
User: Repeats for remaining 2 files
```

**7. Session Fully Restored**

```
System: All clips loaded (or marked as missing)
Grid View: Shows all buttons
  • Missing clips show "?" icon and grayed-out state
  • User can trigger recovered clips normally
```

### Success Criteria

- Automatic recovery finds 80%+ of relocated files
- Manual recovery option for remaining files
- Session usable even with some missing clips
- Clear visual indication of missing media

---

## Summary: Key Interaction Patterns

### Mouse Logic (Waveform Editor)

- **Left-Click:** Set IN point
- **Right-Click:** Set OUT point
- **Middle-Click (or Shift+Left):** Jump playhead
- **Control+Click:** Add cue point

### Keyboard Shortcuts (Essential)

- **Space:** Play/Pause
- **Cmd/Ctrl+S:** Save Session
- **Cmd/Ctrl+N:** New Session
- **Cmd/Ctrl+O:** Open Session
- **I:** Set IN point at playhead
- **O:** Set OUT point at playhead
- **Arrow Keys:** Nudge trim points
- **Cmd/Ctrl+Z:** Undo
- **Cmd/Ctrl+Shift+Z:** Redo

### Visual Feedback Principles

- **Real-time updates:** Button states update immediately (no lag)
- **Color coding:** Clip groups use consistent colors
- **Status indicators:** Playing (pulsing), Recording (red border), Missing (grayed + "?")
- **Contextual help:** Tooltips on hover for all buttons

---

## Related Documents

- **OCC021** - Product Vision (user personas)
- **OCC022** - Clip Metadata Schema (data structures)
- **OCC023** - Component Architecture (system design)
- **OCC011** - Wireframes v2 (UI layouts)
- **SPT023** - SpotOn Report (interaction inspirations)

---

**Document Status:** Ready for UX review and usability testing.
