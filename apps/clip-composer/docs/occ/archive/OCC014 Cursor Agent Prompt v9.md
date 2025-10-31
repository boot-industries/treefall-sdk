# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low‑Latency Professional Soundboard Application**

#### **Objective:** Develop a high‑performance, professional‑grade soundboard application for use in broadcasting, live performances, theatre productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real‑time, ultra‑low‑latency audio playback, advanced DSP processing, multi‑channel routing, and intelligent clip management—all built on a modern, scalable, and crash‑proof architecture optimized for multi‑core processing.

## **Core Functional Requirements**

### **1. Real‑Time Audio Engine & Performance**

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

- Every clip is assigned to one of four **Clip Groups (A, B, C, or D)**. Each group serves a dual purpose:
  1. **Example Output Routing:**
     - **Group A:** Routes audio of all Group A clips to outputs 1–2.
     - **Group B:** Routes audio of all Group B clips to outputs 3–4.
     - **Group C:** Routes audio of all Group C clips to outputs 5–6.
     - **Group D:** Routes audio of all Group D clips to outputs 7–8.
     - **Editor/Audition Mode:** Routes audio from the editor player to outputs 9–10.
  2. **Playback Management:**
     - Each group includes an optional togglable FIFO‑based “choke” feature to stop or fade out previous clips when a new clip is triggered.
  - **Note:** These routing assignments are provided as examples; routing is dynamically configurable via the Routing bottom panel.

### **3. Clip Editing & User Interface**

- **Responsive, modern UI** implemented with JUCE (desktop) and React Native (iOS companion).
- **Advanced waveform editor** featuring integrated mouse and touchscreen controls:
  - **Mouse Controls:**
    - Left‑click to set the IN point.
    - Right‑click to set the OUT point.
    - Middle‑click to jump the playhead.
    - Control+click to set up to 4 cue points per clip (FIFO).
  - **Touchscreen Controls:**
    - Tap-to‑set IN/OUT points, drag-to‑scrub the playhead, and multi‑touch for cue point management.
  - The waveform editor appears as a bottom‑half panel (not a pop‑up), ensuring the main grid view remains visible.
  - Includes an audio player with transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
  - Configurable grid layout (max 10 rows x 12 columns per tab) with support for 8 full‑screen tabs and an optional dual‑tab view.
  - Individual clip buttons may “stretch” to occupy up to 4 grid cells.
- **Unified Bottom Panel:**
  - A single bottom panel region offering four toggleable modes (only one visible at a time):
    - **Labelling:** For editing clip labels, appearance, and metadata (with schema‑specific copy/paste).
    - **Editor:** The waveform editor with transport controls, Loop toggle, DSP controls, and IN/OUT time entry fields.
    - **Routing:** A routing panel with two tabs (“Outputs” and “Inputs”) presenting a routing matrix grid for configuring connections between application endpoints and available audio hardware.
    - **Preferences:** Application‑wide settings, persistent playback controls, and remote control configurations.

### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
  - Recordings are directly captured into the designated clip button.
  - Supports simultaneous recording and playback: audible if no clip is playing; silently captured if a clip is already playing.
- **Metadata embedding** for each recorded clip (name, timestamp, duration, routing info).
- **Auto‑save and session‑based management** for recordings.

### **5. Remote Control (iOS Companion App)**

- Operates in two modes: standalone soundboard or remote controller for the desktop application.
- Supports triggering clips via WiFi, Bluetooth, or WebSockets.
- Provides live monitoring (waveform previews, countdown timers) and facilitates MIDI/OSC passthrough.
- Fully optimized for iPhone and iPad (iPadOS multitasking).

### **6. Logging, Exporting, Session Management, Intelligent File Recovery & Project Media Management**

- **Real‑time Logging:**
  - Capture detailed, timestamped logs for clip playback events.
  - Logs include clip metadata (name, category, audio properties, hotkeys).
- **Automated Log Export:**
  - Support CSV, JSON, or XML exports and optional auto‑sync to cloud storage (Google Drive, Dropbox, AWS S3).
- **Session Management:**
  - Users can save, load, and duplicate complete session states (including clips, settings, cue sequences, banks, configuration data).
- **Intelligent Missing File Search:**
  - Automatically search for missing clip files using metadata; prompt user for file confirmation if multiple candidates exist.
- **Project Media Folder Management & Reconciliation:**
  - Allow users to select or reassign a project media folder for centralizing clip files.
  - Support clip file reconciliation across directories while preserving metadata, DSP settings, and edit points.

### **7. Audio Endpoint Configuration**

- **Output Endpoints:**
  - Defined by the outputs of Clip Groups A/B/C/D plus the Audition output.
  - Supports up to **five stereo output streams**.
  - Each stream (except LTC) is configurable as Mono or Stereo in system preferences.
- **Input Endpoints:**
  - Supports inputs for recording and external time-code:
    - **Record A and Record B:** Up to **two stereo input streams**.
    - **LTC Input:** One mono input stream.
  - Record A/B streams are configurable as Mono or Stereo.
  - In the absence of an external LTC signal, default to using the computer’s time‑of‑day.

### **8. Audio Drivers & Platform‑Specific Considerations**

- **macOS:**
  - Leverage CoreAudio for low‑latency processing.
  - Implement dynamic buffer and sample rate negotiation; support aggregated devices.
  - Integrate robust error handling and automatic re‑initialization on device faults.
- **Windows:**
  - Primary choice is ASIO for professional low‑latency performance; use WASAPI exclusive mode as a fallback.
  - Implement dynamic buffering and sample rate conversion, with runtime checks to select the best available driver.
- **iOS:**
  - Use CoreAudio and Audio Units (AVAudioEngine) for real‑time processing.
  - Support AUv3 for inter‑app connectivity; configure for low‑latency buffers.
  - Default to the system clock for time synchronization when external LTC is absent.
- **Cross‑Platform Strategy:**
  - Consider a cross‑platform abstraction (e.g., PortAudio) to manage differences across CoreAudio, ASIO, WASAPI, and iOS audio frameworks.
  - Implement extensive diagnostics, error logging, and dynamic fallback mechanisms to ensure rock‑solid playback.
- **Reference:**
  - See the "Technical Report: Advanced Audio Driver Integration for a Professional Soundboard Application" for detailed guidance.

### **9. Visual Aids & Wireframe Illustrations**

- **Wireframe Illustrations:**
  - **Main Screen – Single Tab View:**
    - Displays clip group status, Global Tempo, tab navigation (up to 8 tabs), the main grid (10×12 layout with optional clip button stretching), and the unified bottom panel (toggleable among Labelling, Editor, Routing, Preferences).
  - **Main Screen – Dual Tab View:**
    - Two grid panels are shown side by side with dual-tab highlights and the unified bottom panel below.
  - **Unified Bottom Panel – Four Modes:**
    - **Labelling Mode:** Two tabs for editing clip label/appearance and metadata (with schema‑specific copy/paste).
    - **Editor Mode:** Waveform display with transport controls, Loop toggle, DSP controls, and IN/OUT time entry fields.
    - **Routing Mode:** Two tabs (“Outputs” and “Inputs”) displaying a routing matrix grid.
    - **Preferences Mode:** Application‑wide settings including theme, layout, persistent playback controls, and remote control configurations.
- **Additional Visual Aids:**
  - High‑fidelity UI mockups (Figma/Sketch), user flow diagrams, system architecture diagrams, component hierarchy charts, database schema ERDs, and a visual style guide.

(See attached wireframes in “orp_Wireframes_v1.md” for detailed layouts.)

### **10. Tech Stack Recommendations (Optimized for 2025 Standards)**

|                     |                                                               |                         |
| ------------------- | ------------------------------------------------------------- | ----------------------- |
| **Audio Engine**    | JUCE (C++)                                                    | PortAudio               |
| **DSP Processing**  | Rubber Band, ZTX, SoundTouch                                  | Eigen (for matrix math) |
| **UI Framework**    | JUCE (Desktop), React Native (iOS Companion App)              | Qt, SwiftUI             |
| **Networking**      | WebSockets, MIDI, OSC                                         | liblo (OSC)             |
| **Database & Logs** | SQLite, JSON                                                  | LevelDB                 |
| **Remote Control**  | WebSockets, Bluetooth MIDI                                    | OpenStageControl        |
| **Deployment**      | GitHub Actions, Fastlane (iOS), Docker (for container builds) | Jenkins                 |

### **11. Development Priorities & Workflow**

1. **Core Audio Engine Development:**
   - Develop a low‑latency, multi‑threaded DSP engine using C++/JUCE.
   - Implement multi‑channel audio routing with output assignments tied to Clip Groups A/B/C/D.
   - Integrate the optional FIFO‑based playback (choke) functionality per group.
2. **UI & Clip Editor Development:**
   - Build a responsive waveform editor with dedicated mouse and touchscreen controls for IN/OUT points, playhead jumps, and cue points.
   - Integrate the editor as a bottom‑half screen extension while keeping the main tab view accessible.
   - Ensure the editor’s audio player is routable (outputs 9–10).
   - Implement a user‑configurable clip grid (10x12 layout) with support for 8 full‑screen tabs and dual‑tab view.
   - **Implement clip button stretching:** Allow clip buttons to span up to 4 grid cells.
   - **Implement Unified Bottom Panel:**
     - Develop four toggleable modes (Labelling, Editor, Routing, Preferences) ensuring only one is visible at any time.
3. **Remote Control Implementation:**
   - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
4. **Performance Optimization:**
   - Optimize real‑time memory management and multi‑core processing.
5. **Session Management, Logging & Intelligent File Recovery:**
   - Implement robust auto‑save, session duplication, and automated log export features.
   - Integrate an intelligent search mechanism to locate missing files based on clip metadata.
6. **Audio Endpoint Configuration Implementation:**
   - Configure outputs (from Clip Groups and Audition) supporting up to five stereo streams; inputs for Record A/B (up to two stereo streams) and LTC (one mono stream) with Mono/Stereo configuration and fallback to computer time if LTC is absent.
7. **Audio Drivers Integration:**
   - Integrate platform‑specific audio drivers per the detailed Audio Drivers report to ensure rock‑solid playback on macOS (CoreAudio), Windows (ASIO/WASAPI), and iOS (CoreAudio/Audio Units/AUv3).
8. **Beta Testing & Deployment:**
   - Conduct extensive stress testing on various hardware configurations.
   - Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.

### **12. Expected Outcome**

A robust, crash‑proof, professional soundboard application delivering:

- Real‑time, ultra‑low‑latency audio performance.
- Intelligent clip management with flexible routing via Clip Groups (A/B/C/D) and dynamic endpoint configuration.
- A user‑configurable grid interface (up to 8 full‑screen tabs, dual‑tab view, and clip button stretching).
- A unified bottom panel that toggles among four modes: Labelling, Editor (with advanced waveform features), Routing (with a matrix for outputs/inputs), and Preferences.
- Comprehensive logging, session management, and intelligent missing file recovery.
- Rock‑solid audio playback ensured by robust, platform‑specific driver management per our Advanced Audio Drivers report.
- Seamless remote control capabilities via an iOS companion app.
- Extensive visual design documentation and wireframe illustrations to guide UI implementation.

### **13. Additional Considerations**

- **Future Expansion:**
  - Integration of a web‑based controller (using React + WebSockets) for remote access.
  - Implementation of AI‑powered auto‑tagging to streamline clip organization.
- **Docker Deployment:**
  - Utilize Docker Desktop for containerized builds to ensure cross‑platform consistency.
  - Set up automated pipelines for efficient updates and maintenance.

This final prompt version integrates our detailed Audio Drivers report and includes comprehensive wireframe illustrations (see “orp_Wireframes_v1.md”) as visual aids. It ensures Cursor AI’s Composer Agent has all the necessary technical and visual context to generate a robust, high‑performance application.
