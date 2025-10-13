# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**

#### **Objective:** Develop a high-performance, professional-grade soundboard application for use in broadcasting, live performances, theatre productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real-time, ultra-low-latency audio playback, advanced DSP processing, multi-channel routing, and intelligent clip management—all built on a modern, scalable, and crash-proof architecture optimized for multi‑core processing.




## **Core Functional Requirements**

### **1. Real-Time Audio Engine & Performance**

- **Ultra‑low‑latency playback** optimized for real‑time triggering.
- **Multi‑format audio support:** MP3, WAV, FLAC, AIFF, OGG, WMA, and AAC (m4a).
- **Multi‑threaded audio engine** with real‑time priority execution.
- **Sample‑accurate playback** featuring independent pitch and time processing:
    - **Time‑stretching** (without altering pitch).
    - **Pitch‑shifting** (independent of playback speed).
    - Implement using libraries such as Rubber Band, SoundTouch, or ZTX DSP.
- **Multi‑channel audio routing:**
    - Ability to assign audio to specific outputs (e.g., Dante, USB interfaces, virtual audio buses).




### **2. Clip Groups and Audio Output Routing**

- Every clip must be assigned to one of four **Clip Groups (A, B, C, or D)**. Each group serves a dual purpose:
    1. **Example Output Routing:**
        - **Group A:** Routes audio of all Group A clips to outputs 1–2.
        - **Group B:** Routes audio of all Group B clips to outputs 3–4.
        - **Group C:** Routes audio of all Group C clips to outputs 5–6.
        - **Group D:** Routes audio of all Group D clips to outputs 7–8.
        - **Editor/Audition Mode:** Routes audio from the editor player to outputs 9–10.
    2. **Playback Management:**
        - Each group has an optional togglable FIFO‑based “choke” feature. When enabled, triggering a new clip in a group will stop (or fade out) any previous clip within the same group, ensuring clean transitions.
    - **Note:** These routing assignments serve as examples; the system must allow dynamic reconfiguration of routing settings.




### **3. Clip Editing & User Interface**

- **Responsive, modern UI** developed using JUCE for the desktop and React Native for the iOS companion app.
- **Advanced waveform editor** with integrated mouse and touchscreen controls:
    - **Mouse Controls:**
        - **Left‑click** sets the **IN point**.
        - **Right‑click** sets the **OUT point**.
        - **Middle‑click** jumps the playhead.
        - **Control+click** sets up to **4 cue points per clip** on a FIFO basis.
    - **Touchscreen Controls:**
        - Intuitive gestures mirror mouse actions—tap-to‑set IN/OUT points, drag‑to‑scrub the playhead, and multi‑touch for cue point management.
    - The waveform editor is integrated into a panel that appears from the **bottom half of the screen** (instead of as a pop‑up), ensuring that the main tab view remains visible while editing.
    - The editor includes its own audio player with basic transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
    - User‑configurable grid layout for clip buttons, with a maximum size of **10 rows x 12 columns** per tab.
    - Supports **8 full‑screen tabs by default**.
    - Option to display two tabs simultaneously (using FIFO logic to govern which two tabs are active/highlighted).
    - **Clip Button Stretching:**
        - Individual clip buttons can be configured to “stretch” and occupy up to 4 grid cells, allowing for larger buttons when desired.
    - Primary page elements remain fixed (aside from adjustable pane widths in dual‑tab view); the editor panel itself remains fixed in size.
- **Unified Bottom Panel:**
    - Four toggleable modes are available in a single bottom panel region, with only one visible at a time:
        - **Labelling:** For editing clip labels, appearance, and metadata (with schema‑specific copy/paste).
        - **Editor:** The waveform editor with transport controls, Loop toggle, DSP controls, and IN/OUT time entry fields.
        - **Routing:** A routing panel featuring two tabs (“Outputs” and “Inputs”) with a routing matrix grid to define connections between application endpoints and available audio hardware.
        - **Preferences:** Application‑wide settings and persistent playback controls.




### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
    - Recorded sounds are captured directly into the designated button, eliminating the need for later assignment.
    - Supports simultaneous recording and playback. If no clip is playing, the recording is audible; if a clip is already playing, the recording is captured silently.
- **Metadata embedding** for each recorded clip (including name, timestamp, duration, and routing info).
- **Auto‑save and session‑based management** for recordings.




### **5. Remote Control (iOS Companion App)**

- The iOS app can operate in two modes:
    - As a standalone soundboard.
    - Or as a remote controller for the desktop application.
- Supports triggering clips via WiFi, Bluetooth, or WebSockets.
- Provides live monitoring of active clips with waveform previews and countdown timers.
- Facilitates MIDI/OSC passthrough for seamless external control.
- Fully optimized for both iPhone and iPad (including iPadOS multitasking).




### **6. Logging, Exporting, Session Management, Intelligent File Recovery & Project Media Management**

- **Real‑time Logging:**
    - Capture timestamped logs for every clip played (recording start, duration, and stop times).
    - Logs include clip metadata such as name, category, audio properties, and assigned hotkeys.
- **Automated Log Export:**
    - Export logs in CSV, JSON, or XML formats.
    - Option to auto‑sync logs to cloud storage services (Google Drive, Dropbox, AWS S3).
- **Comprehensive Session Management:**
    - Users can save, load, and duplicate complete session states.
    - Sessions include all clips, settings, cue sequences, banks, and configuration data.
- **Intelligent Missing File Search:**
    - When a session is loaded and one or more clip files are missing, the system automatically performs an intelligent search using clip metadata to locate relevant or nearby files.
    - If multiple candidate files are found, the user is prompted to confirm the correct file, ensuring seamless recovery of missing assets.
- **Project Media Folder Management & Reconciliation:**
    - **Project Media Folder Selection:**
        - Upon creating a new project, a dialogue prompts the user to select a default project media folder, which will serve as the central repository for all clip files.
        - For existing projects, users have the option to reassign the project media folder if media files have been relocated or if a different folder is preferred.
    - **Clip Reconciliation:**
        - The system supports reconciling clip files to identical assets located in different directories.
        - When a clip file is found in an alternate location, the system maintains all session‑relevant clip metadata, DSP settings, and edit points.
        - The user is prompted to confirm the reassignment of the clip file to the new location, ensuring that session integrity is preserved while providing flexibility in managing media file storage.




### **7. Audio Endpoint Configuration**

- **Output Endpoints:**
    - **Definition:**
        - The application's outputs are defined by the outputs of Clip Groups A/B/C/D, plus the Audition output (from the Editor).
    - **Capabilities:**
        - Supports a maximum of **five stereo output streams**.
        - Each stream (except for LTC) must be configurable as Mono or Stereo via system preferences.
- **Input Endpoints:**
    - **Definition:**
        - The application supports inputs for recording and external time-code:
            - **Record A and Record B:** Up to **two stereo input streams**.
            - **LTC Input:** One mono input stream.
    - **Configuration:**
        - Record A and Record B streams should be configurable as Mono or Stereo within system preferences.
    - **Time Synchronization:**
        - In the absence of an external LTC signal, the application defaults to using the computer’s time-of-day.




### **8. Tech Stack Recommendations (Optimized for 2025 Standards)**

|  |  |  |
| --- | --- | --- |
| **Audio Engine** | JUCE (C++) | PortAudio |
| **DSP Processing** | Rubber Band, ZTX, SoundTouch | Eigen (for matrix math) |
| **UI Framework** | JUCE (Desktop), React Native (iOS Companion App) | Qt, SwiftUI |
| **Networking** | WebSockets, MIDI, OSC | liblo (OSC) |
| **Database & Logs** | SQLite, JSON | LevelDB |
| **Remote Control** | WebSockets, Bluetooth MIDI | OpenStageControl |
| **Deployment** | GitHub Actions, Fastlane (iOS), Docker (for container builds) | Jenkins |




### **9. Development Priorities & Workflow**

1. **Core Audio Engine Development:**
    - Develop a low‑latency, multi‑threaded DSP engine using C++/JUCE.
    - Implement multi‑channel audio routing with output assignments tied to Clip Groups A/B/C/D.
    - Integrate the optional FIFO‑based playback (choke) functionality per group.
2. **UI & Clip Editor Development:**
    - Build a responsive waveform editor with dedicated mouse and touchscreen controls for setting IN/OUT points, playhead jumps, and cue points.
    - Integrate the editor as a bottom‑half screen extension while keeping the main tab view accessible.
    - Ensure the editor’s audio player is routable (outputs 9–10).
    - Implement a user‑configurable clip grid (up to 10x12 per tab) with support for 8 full‑screen tabs and dual‑tab view.
    - **Implement clip button stretching:** Allow individual clip buttons to occupy up to 4 grid cells for larger button displays.
    - **Implement Unified Bottom Panel:**
        - Develop four toggleable modes: Labelling, Editor, Routing, and Preferences.
        - Ensure only one bottom panel is visible at any given time.
3. **Remote Control Implementation:**
    - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
4. **Performance Optimization:**
    - Optimize real‑time memory management and multi‑core processing.
5. **Session Management, Logging & Intelligent File Recovery:**
    - Implement robust auto‑save, session duplication, and automated log export features.
    - Integrate the intelligent search mechanism to locate missing files based on clip metadata when sessions are loaded.
6. **Audio Endpoint Configuration Implementation:**
    - Configure audio endpoints as specified for outputs (Clip Groups and Audition) and inputs (Record A/B and LTC), including Mono/Stereo settings and defaulting to computer time when LTC is absent.
7. **Beta Testing & Deployment:**
    - Conduct extensive stress testing under professional workloads.
    - Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.




### **10. Expected Outcome**

A robust, crash‑proof, professional soundboard application delivering real‑time, ultra‑low‑latency audio performance, intelligent clip management, and flexible routing. Key features include:

- An integrated bottom‑half waveform editor that supports both touchscreen and mouse controls, with dedicated controls for IN/OUT points, playhead jumps, Loop toggle, and DSP controls.
- Clip Groups (A/B/C/D) that manage both audio output routing and optional FIFO‑based playback control.
- A user‑configurable grid interface for managing clips across 8 full‑screen tabs (with an optional dual‑tab view), including support for clip button stretching to occupy up to 4 grid cells.
- A unified bottom panel with four toggleable modes: Labelling, Editor, Routing, and Preferences.
- Audio endpoints defined as the outputs of Clip Groups A/B/C/D and the Audition output (supporting up to five stereo streams) as well as inputs for Record A/B (up to two stereo streams) and LTC (one mono stream). Each stream (except LTC) is configurable as Mono or Stereo, and in the absence of an external LTC signal, the computer’s time-of‑day is used.
- Seamless remote control capabilities via an iOS app.
- Comprehensive logging, session management, and an intelligent missing file search feature that recovers absent clip files using metadata.
- Robust project media management with support for media folder selection and clip reconciliation.




### **11. Additional Considerations**

- **Future Expansion:**
    - Integration of a web‑based controller (using React + WebSockets) for remote access.
    - Implementation of AI‑powered auto‑tagging to streamline clip organization.
- **Docker Deployment:**
    - Utilize Docker Desktop for containerized builds, ensuring cross‑platform consistency and simplified deployment.
    - Set up automated pipelines for efficient updates and maintenance.




This updated prompt now includes detailed audio endpoint configurations along with all previously specified functionality, providing a complete blueprint for our soundboard application. Let me know if any further adjustments are needed!