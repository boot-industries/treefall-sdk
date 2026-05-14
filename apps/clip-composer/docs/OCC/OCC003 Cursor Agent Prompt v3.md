---
source: standard-notes
sn_filename: "OCC003 Cursor Agent Prompt v3-97f80f7a.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**


#### **Objective:** Develop a **high-performance, professional-grade soundboard application** for use in **broadcasting, live performances, theater productions, and professional audio workflows**. The software must support **Windows and macOS** with an **iOS companion app** for remote control.

The application will feature **real-time, ultra-low-latency audio playback**, **advanced DSP processing**, **MIDI/OSC integration**, **multi-channel audio routing**, **session logging**, and **intelligent playback behavior**. It must be **highly stable and optimized for multi-core processing**, with a **modern, sleek, and responsive UI**.


---




## **Core Functional Requirements**


### **1. Real-Time Audio Engine & Performance**

- **Ultra-low-latency playback** (optimized for real-time triggering).
- **Multi-format audio support**: MP3, WAV, FLAC, AIFF, OGG, WMA, and AAC (m4a).
- **Multi-threaded audio engine** with **real-time priority execution**.
- **Sample-accurate playback** with **independent pitch and time processing**:
- - **Time-stretching** without altering pitch.
- **Pitch-shifting** independent of speed.
- Implement via **Rubber Band Library, SoundTouch, or ZTX DSP**.
- **Multi-channel audio routing**:
- - Assign sounds to specific outputs (e.g., Dante, USB, virtual audio buses).
- **Clip Groups & Routing**:
- - Replace traditional “choke groups” with **Clip Groups A/B/C/D**.
- Each clip is assigned to one of these groups, which serve as both **routing groups and choke groups**.
- A **toggable “choke” option** per group applies FIFO choke behavior (i.e., when a clip in the group is triggered, previous clips are stopped or faded out).
- **Default output routing per group** is predefined:
- - **Group A:** Outputs 1–2
- **Group B:** Outputs 3–4
- **Group C:** Outputs 5–6
- **Group D:** Outputs 7–8
- **Editor/Audition:** Outputs 9–10
- **Configurable fade-in/out and crossfades**.
- **Pre-loading and caching** of frequently used sounds to optimize response time.


---




### **2. Clip Editing & User Interface**

- **Modern, responsive UI** built with **JUCE (for desktop)** and **React Native (for iOS companion app)**.
- **Advanced waveform visualization with dedicated mouse logic**:
- - **Left-click** sets the **IN point**.
- **Right-click** sets the **OUT point**.
- **Middle-click** jumps the playhead.
- **Control+click** sets up to **4 cue points per clip on a FIFO basis**.
- The waveform editor includes an **audio player with basic transport controls** for previewing edits.
- **Editor Layout**:
- - The editor appears as a **bottom-half screen extension** rather than a pop-up.
- The main tabs remain visible in the upper portion, allowing users to **launch or stop clips** even when the editor is open.
- The editor’s audio player is **routable**, aligning with the overall output routing (Editor/Audition assigned to outputs 9–10).
- **Color-coded clip organization**:
- - Assign custom colors for fast visual identification.
- **User-configurable clip button grid**:
- - Grid of up to **10x12 (height x width)** per tab.
- Support for **8 full-screen tabs by default**.
- Option to split the screen to show **two tabs at once**; the tab handler uses FIFO logic to allow two tabs to be highlighted simultaneously.
- Primary page elements remain fixed (aside from adjusting pane widths in dual tab view).
- **Hotkeys and shortcut customization**:
- - Keyboard shortcuts for launching clips, adjusting volume, and pausing playback.
- **MIDI & OSC integration**:
- - **Velocity-sensitive MIDI triggering** for dynamic playback.
- **OSC support** for advanced remote triggering via external controllers.


---




### **3. Audio Recording & Live Capture**

- **Real-time audio recording** directly into designated buttons:
- - **Recorded sounds are captured directly into buttons**, eliminating the need for later assignment.
- Simultaneous recording and playback are supported with logic such that:
- - If no clip is playing, the recorded sound is audible.
- If a clip is already playing, the recording is silent but still captured.
- **Metadata embedding** in recorded audio clips (e.g., name, timestamp, duration, routing info).
- **Auto-save and session-based recording management**.


---




### **4. Remote Control (iOS Companion App)**

- **Standalone or remote control mode**:
- - The iOS app can either **operate its own soundboard** or **remotely control the desktop app**.
- **Trigger clips remotely** via WiFi, Bluetooth, or WebSockets.
- **Live monitoring** of active clips (waveform previews, countdown timers).
- **MIDI/OSC passthrough** for external control from iOS to the desktop app.
- **Support for iPhone & iPad (iPadOS multitasking enabled)**.


---




### **5. Logging, Exporting & Session Management**

- **Real-time playback logging**:
- - Capture **timestamped logs of every clip played** (start time, duration, stop time).
- Include **clip metadata** (name, category, audio properties, assigned hotkey).
- **Automated log export**:
- - Export logs in CSV, JSON, or XML formats.
- Auto-sync logs to cloud storage (Google Drive, Dropbox, AWS S3).
- **Session management**:
- - Users can **save, load, and duplicate complete session states**.
- Sessions include all clips, settings, cue sequences, banks, and configurations.


---




## **Tech Stack Recommendations (Optimized for 2025 Standards)**

**Component****Primary Choice****Alternative****Audio Engine**

**JUCE (C++)**

PortAudio

**DSP Processing**

**Rubber Band, ZTX, SoundTouch**

Eigen (for matrix math)

**UI Framework**

**JUCE (Desktop), React Native (iOS Companion App)**

Qt, SwiftUI

**Networking**

**WebSockets, MIDI, OSC**

liblo (OSC)

**Database & Logs**

**SQLite, JSON**

LevelDB

**Remote Control**

**WebSockets, Bluetooth MIDI**

OpenStageControl

**Deployment**

**GitHub Actions, Fastlane (iOS), Docker (for containerized builds)**

Jenkins


---




## **Development Priorities & Workflow**

1. **Core Audio Engine Development**
2. - Build a **low-latency, multi-threaded DSP engine** (C++/JUCE).
- Implement **multi-channel routing** with output assignments tied to **Clip Groups A/B/C/D**.
- Apply **toggable choke (FIFO) behavior** per group.
3. **UI & Clip Editor Development**
4. - Develop **responsive waveform rendering** with dedicated mouse logic for setting IN/OUT points, playhead jumps, and cue points.
- Implement the **bottom-half screen editor extension** so that main tabs remain accessible.
- Ensure the editor’s audio player is **routable** (assigned to outputs 9–10).
- Implement the **user-configurable grid layout** for clip buttons (up to 10x12 per tab) with support for 8 full-screen tabs and dual tab view.
5. **Remote Control Implementation**
6. - Develop the **iOS companion app** with WebSockets & MIDI/OSC integration.
7. **Performance Optimization**
8. - Optimize **real-time memory management** and **multi-core processing**.
9. **Session Management & Logging**
10. - Implement **auto-save, session duplication, and logging exports**.
11. **Beta Testing & Deployment**
12. - **Stress test under professional workloads**.
- Prepare **automated builds with CI/CD pipelines** (GitHub Actions, Fastlane, Docker).


---




## **Expected Outcome**

A **robust, crash-proof, professional soundboard application** that delivers **real-time, ultra-low-latency audio performance**, **intelligent playback features**, and **seamless remote control capabilities**. The application features:

- An integrated **bottom-half editor panel** with a routable audio player.
- **Clip Groups A/B/C/D** that control both choke behavior (via a togglable option) and output routing (with predefined mappings: Group A → Outputs 1–2, Group B → Outputs 3–4, Group C → Outputs 5–6, Group D → Outputs 7–8, and Editor/Audition → Outputs 9–10).
- A modern, scalable architecture optimized for high-performance professional environments including **live events, broadcasting, and theater productions**.


---




## **Additional Considerations**

- **Future Expansion**:
- - **Web-based controller** (React + WebSockets) for remote access.
- **AI-powered auto-tagging** of sound clips for rapid organization.
- **Docker Deployment**:
- - Use **Docker Desktop** for containerized builds and cross-platform consistency.
- Deploy automated pipelines to ensure efficient updates and maintenance.


---



This updated prompt now reflects that audio output routing is tied to each clip's assigned group—with groups renamed to **Clip Groups A/B/C/D**, each with a togglable choke option (FIFO behavior) and fixed output assignments, while maintaining all previous functionality and design improvements.
