---
source: standard-notes
sn_filename: "OCC007 Cursor Agent Prompt v6-3b00d627.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**


#### **Objective:** Develop a high-performance, professional-grade soundboard application for use in broadcasting, live performances, theatre productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real-time, ultra-low-latency audio playback, advanced DSP processing, multi-channel routing, and intelligent clip management—all built on a modern, scalable, and crash-proof architecture optimized for multi-core processing.


---




## **Core Functional Requirements**


### **1. Real-Time Audio Engine & Performance**

- **Ultra-low-latency playback** optimized for real-time triggering.
- **Multi-format audio support:** MP3, WAV, FLAC, AIFF, OGG, WMA, and AAC (m4a).
- **Multi-threaded audio engine** with real-time priority execution.
- **Sample-accurate playback** featuring independent pitch and time processing:
- - **Time-stretching** (without altering pitch).
- **Pitch-shifting** (independent of playback speed).
- Implement using libraries such as Rubber Band, SoundTouch, or ZTX DSP.
- **Multi-channel audio routing:**
- - Ability to assign audio to specific outputs (e.g., Dante, USB interfaces, virtual audio buses).


---




### **2. Clip Groups and Audio Output Routing**

- Every clip must be assigned to one of four **Clip Groups (A, B, C, or D)**. Each group serves a dual purpose:
- 1. **Example Output Routing:**
2. - **Group A:** Routes audio of all Group A clips to outputs 1–2.
- **Group B:** Routes audio of all Group B clips to outputs 3–4.
- **Group C:** Routes audio of all Group C clips to outputs 5–6.
- **Group D:** Routes audio of all Group D clips to outputs 7–8.
- **Editor/Audition Mode:** Routes audio from the editor player to outputs 9–10.
3. **Playback Management:**
4. - Each group has an optional togglable FIFO-based “choke” feature. When enabled, triggering a new clip in a group will stop (or fade out) any previous clip within the same group, ensuring clean transitions.
- - **Note:** These routing assignments serve as examples; the system must allow dynamic reconfiguration of routing settings.


---




### **3. Clip Editing & User Interface**

- **Responsive, modern UI** developed using JUCE for the desktop and React Native for the iOS companion app.
- **Advanced waveform editor** with integrated mouse and touchscreen controls:
- - **Mouse Controls:**
- - **Left-click** sets the **IN point**.
- **Right-click** sets the **OUT point**.
- **Middle-click** jumps the playhead.
- **Control+click** sets up to **4 cue points per clip** on a FIFO basis.
- **Touchscreen Controls:**
- - Intuitive touch gestures mirror the mouse actions, allowing users to tap to set IN/OUT points, drag to scrub the playhead, and use multi-touch for cue point management.
- The waveform area is integrated into an editor panel that appears from the **bottom half of the screen** (rather than as a pop-up), ensuring the main tab view remains visible for launching or stopping clips while editing.
- The editor includes its own audio player with basic transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
- - User-configurable grid layout for clip buttons, with a maximum size of **10 rows x 12 columns** per tab.
- Supports **8 full-screen tabs by default**.
- Option to display two tabs simultaneously (with FIFO logic governing which two tabs are active/highlighted).
- **Clip Button Stretching:**
- - In addition to the grid layout, individual clip buttons can be configured to “stretch” and occupy up to 4 grid cells, allowing for larger buttons when desired.
- Primary page elements remain fixed (aside from adjustable pane widths in dual-tab view); the editor panel itself is fixed in size.


---




### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
- - Recorded sounds are captured directly into the designated button, eliminating the need for later assignment.
- Simultaneous recording and playback are supported. If no clip is playing, the recording is audible; if a clip is already playing, the recording is captured silently.
- **Metadata embedding** for each recorded clip (including name, timestamp, duration, and routing info).
- **Auto-save and session-based management** for recordings.


---




### **5. Remote Control (iOS Companion App)**

- The iOS app can operate in two modes:
- - As a standalone soundboard.
- Or as a remote controller for the desktop application.
- Supports triggering clips via WiFi, Bluetooth, or WebSockets.
- Provides live monitoring of active clips with waveform previews and countdown timers.
- Facilitates MIDI/OSC passthrough for seamless external control.
- Fully optimized for both iPhone and iPad (including iPadOS multitasking).


---




### **6. Logging, Exporting, Session Management, Intelligent File Recovery & Project Media Management**

- **Real-time Logging:**
- - Capture timestamped logs for every clip played (recording start, duration, and stop times).
- Logs include clip metadata such as name, category, audio properties, and assigned hotkeys.
- **Automated Log Export:**
- - Export logs in CSV, JSON, or XML formats.
- Option to auto-sync logs to cloud storage services (Google Drive, Dropbox, AWS S3).
- **Comprehensive Session Management:**
- - Users can save, load, and duplicate complete session states.
- Sessions include all clips, settings, cue sequences, banks, and configuration data.
- **Intelligent Missing File Search:**
- - When a session is loaded and one or more clip files are missing, the system automatically performs an intelligent search using clip metadata to locate relevant or nearby files.
- If multiple candidate files are found, the user is prompted to confirm the correct file, ensuring seamless recovery of missing assets.
- **Project Media Folder Management & Reconciliation:**
- - **Project Media Folder Selection:**
- - Upon creating a new project, a dialogue prompts the user to select a default project media folder, which will serve as the central repository for all clip files.
- For existing projects, users have the option to reassign the project media folder through a similar dialogue if media files have been relocated or if a different folder is preferred.
- **Clip Reconciliation:**
- - The system supports reconciling clip files to identical assets located in different directories.
- When a clip file is found in an alternate location, the system maintains all session-relevant clip metadata, DSP settings, and edit points.
- The user is prompted to confirm the reassignment of the clip file to the new location, ensuring that session integrity is preserved while providing flexibility in managing media file storage.


---




## **Tech Stack Recommendations (Optimized for 2025 Standards)**

**Component****Primary Choice****Alternative****Audio Engine**

JUCE (C++)

PortAudio

**DSP Processing**

Rubber Band, ZTX, SoundTouch

Eigen (for matrix math)

**UI Framework**

JUCE (Desktop), React Native (iOS Companion App)

Qt, SwiftUI

**Networking**

WebSockets, MIDI, OSC

liblo (OSC)

**Database & Logs**

SQLite, JSON

LevelDB

**Remote Control**

WebSockets, Bluetooth MIDI

OpenStageControl

**Deployment**

GitHub Actions, Fastlane (iOS), Docker (for container builds)

Jenkins


---




## **Development Priorities & Workflow**

1. **Core Audio Engine Development:**
2. - Develop a low-latency, multi-threaded DSP engine using C++/JUCE.
- Implement multi-channel audio routing with output assignments tied to Clip Groups A/B/C/D.
- Integrate the optional FIFO-based playback (choke) functionality per group.
3. **UI & Clip Editor Development:**
4. - Build a responsive waveform editor with dedicated mouse and touchscreen controls for setting IN/OUT points, playhead jumps, and cue points.
- Integrate the editor as a bottom-half screen extension while keeping the main tab view accessible.
- Ensure the editor’s audio player is routable (outputs 9–10).
- Implement a user-configurable clip grid (up to 10x12 per tab) with support for 8 full-screen tabs and dual-tab view.
- **Implement clip button stretching:** Allow individual clip buttons to occupy up to 4 grid cells for larger button displays.
5. **Remote Control Implementation:**
6. - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
7. **Performance Optimization:**
8. - Optimize real-time memory management and multi-core processing.
9. **Session Management, Logging & Intelligent File Recovery:**
10. - Implement robust auto-save, session duplication, and automated log export features.
- Integrate the intelligent search mechanism to locate missing files based on clip metadata when sessions are loaded.
11. **Beta Testing & Deployment:**
12. - Conduct extensive stress testing under professional workloads.
- Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.


---




## **Expected Outcome**

A robust, crash-proof, professional soundboard application delivering real-time, ultra-low-latency audio performance, intelligent clip management, and flexible routing. Key features include:

- An integrated bottom-half waveform editor that supports both touchscreen and mouse controls, with dedicated controls for IN/OUT points, playhead jumps, and cue points.
- Clip Groups (A/B/C/D) that manage both audio output routing and optional FIFO-based playback control.
- A user-configurable grid interface for managing clips across 8 full-screen tabs (with an optional dual-tab view), including the ability for individual clip buttons to stretch and occupy up to 4 grid cells.
- Seamless remote control capabilities via an iOS app.
- Comprehensive logging, session management, and an intelligent missing file search feature that recovers absent clip files using metadata.
- Robust project media management with support for media folder selection and clip reconciliation.


---




## **Additional Considerations**

- **Future Expansion:**
- - Integration of a web-based controller (using React + WebSockets) for remote access.
- Implementation of AI-powered auto-tagging to streamline clip organization.
- **Docker Deployment:**
- - Utilize Docker Desktop for containerized builds, ensuring cross-platform consistency and simplified deployment.
- Set up automated pipelines for efficient updates and maintenance.
