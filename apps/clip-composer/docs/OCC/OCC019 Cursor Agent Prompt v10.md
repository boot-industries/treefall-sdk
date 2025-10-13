# **Project Title: Orpheus Clip Composer**

**Cross‑Platform, Low‑Latency Professional Soundboard Application**

#### **Objective:** Develop a high‑performance, professional‑grade soundboard application for broadcasting, live performances, theatre productions, and other professional audio workflows. The application must support Windows and macOS, with an iOS companion app for remote control. It should deliver real‑time, ultra‑low‑latency audio playback, advanced DSP processing, multi‑channel routing, and intelligent clip management—all built on a scalable, crash‑proof architecture optimized for multi‑core processing.




## **Core Functional Requirements**

### **1. Real‑Time Audio Engine & Performance**

- **Ultra‑low‑latency playback:**
    - Use native audio APIs (CoreAudio on macOS/iOS; ASIO/WASAPI on Windows) with dynamic buffer and sample‑rate negotiation to achieve sub‑5ms latencies.
- **Multi‑format audio support:**
    - Primary formats: WAV, AIFF, FLAC.
    - Secondary formats: MP3, AAC, OGG, WMA (using native decoders when available).
- **Multi‑threaded audio engine:**
    - Leverage platform‑optimized threading and SIMD acceleration.
    - Implement native ring buffer mechanisms for sample‑accurate timing.
- **Time‑stretching and pitch‑shifting:**
    - Use optimized DSP libraries (e.g., Rubber Band, SoundTouch, ZTX DSP) for independent manipulation without affecting latency.
- **Native audio driver integration:**
    - **macOS:** CoreAudio with support for aggregated devices.
    - **Windows:** Primarily ASIO; fallback to WASAPI exclusive mode if needed.
    - **iOS:** CoreAudio/Audio Units with AUv3 support.
    - Refer to the attached Audio Drivers report for detailed integration strategies.




### **2. Clip Groups and Audio Output Routing**

- **Clip Group Assignment:**
    - Every clip is assigned to one of four Clip Groups (A, B, C, or D) that govern both audio routing and optional FIFO‑based playback control.
- **Configurable Routing:**
    - Default example mappings:
        - Group A → outputs 1–2
        - Group B → outputs 3–4
        - Group C → outputs 5–6
        - Group D → outputs 7–8
        - Editor/Audition Mode → outputs 9–10
    - Routing settings are dynamically configurable via a Routing bottom panel.




### **3. Clip Editing & User Interface**

- **Responsive, modern UI:**
    - Developed with JUCE (desktop) and React Native (iOS) for high‑performance, cross‑platform rendering.
- **Advanced Waveform Editor:**
    - Integrated into a bottom‑half panel (non‑pop‑up) for seamless editing.
    - Supports both mouse and touchscreen:
        - **Mouse:** Left‑click for IN point, Right‑click for OUT point, Middle‑click for playhead jump, Control+click for up to 4 cue points.
        - **Touchscreen:** Tap, drag, and multi‑touch gestures for intuitive control.
    - Includes its own audio player with transport controls, Loop toggle, DSP controls, and editable IN/OUT time entry fields.
- **Configurable Clip Grid:**
    - A user‑configurable grid (up to 10 rows x 12 columns per tab) with support for 8 full‑screen tabs and dual‑tab view.
    - Individual clip buttons can “stretch” to occupy up to 4 grid cells.
- **Unified Bottom Panel (Four Toggleable Modes):**
    - **Labelling Mode:**
        - Two tabs: Label & Appearance (edit clip label text, color, font) and Metadata (edit clip metadata with schema‑specific copy/paste).
    - **Editor Mode:**
        - Displays the waveform editor with transport controls, Loop toggle, DSP controls, and IN/OUT time entry fields.
    - **Routing Mode:**
        - Provides a routing matrix with two tabs (“Outputs” and “Inputs”) to configure connections between application endpoints and available audio hardware.
    - **Preferences Mode:**
        - Offers application‑wide settings (theme, layout, persistent playback controls, remote control configuration).
    - Only one bottom panel is visible at any time to preserve full view of the main grid.




### **4. Audio Recording & Live Capture**

- **Direct Recording into Buttons:**
    - Capture recordings directly into designated clip buttons, eliminating post-assignment.
    - Support simultaneous recording and playback (audible when no clip is active; silent capture when a clip is playing).
- **Metadata Embedding:**
    - Each recording embeds metadata (name, timestamp, duration, routing information).
- **Session‑Based Auto‑Save:**
    - Automatically save recording data as part of the session state.




### **5. Remote Control (iOS Companion App)**

- **Dual Mode Operation:**
    - Functions as a standalone soundboard or as a remote controller for the desktop application.
- **Network Communication:**
    - Supports triggering clips via WiFi, Bluetooth, or WebSockets.
    - Provides real‑time monitoring (waveform previews, countdown timers) and facilitates MIDI/OSC passthrough.
- **Optimized for iPhone/iPad:**
    - Fully compatible with iPadOS multitasking.




### **6. Logging, Session Management & Media Handling**

- **Real‑time Logging:**
    - Capture detailed logs for every clip event (start, duration, stop) including associated metadata.
- **Automated Log Export:**
    - Support export in CSV, JSON, and XML formats with optional auto‑sync to cloud storage (Google Drive, Dropbox, AWS S3).
- **Comprehensive Session Management:**
    - Enable saving, loading, and duplicating complete session states, including grid layout, clip order, UI settings, and global performance modes.
- **Intelligent File Recovery:**
    - Automatically search for missing clip files using metadata, prompting the user for confirmation if multiple candidates exist.
- **Project Media Management:**
    - Allow users to select and reassign a project media folder.
    - Support clip reconciliation across directories while preserving metadata, DSP settings, and edit points.




### **7. Audio Endpoint Configuration**

- **Output Endpoints:**
    - Defined by the outputs of Clip Groups A/B/C/D and the Audition output.
    - Support up to **five stereo output streams**, with each stream (except LTC) configurable as Mono or Stereo via system preferences.
- **Input Endpoints:**
    - Support for recording and external time‑code:
        - **Record A and Record B:** Up to **two stereo input streams**.
        - **LTC Input:** One mono input stream.
    - Record A/B streams are configurable as Mono or Stereo; if no external LTC is provided, default to the computer’s time‑of‑day.




### **8. Audio Drivers & Platform‑Specific Integration**

- **macOS:**
    - Utilize CoreAudio for low‑latency performance with dynamic buffer negotiation and error recovery.
- **Windows:**
    - Use ASIO drivers primarily (with WASAPI exclusive mode as fallback) and implement dynamic buffer/sample rate management.
- **iOS:**
    - Leverage CoreAudio/Audio Units and support AUv3 for inter‑app connectivity.
- **Cross‑Platform Abstraction:**
    - Consider a modular abstraction layer to encapsulate driver-specific code and allow dynamic driver switching based on performance metrics.
- **Reference:**
    - See the attached Audio Drivers report (“orp_AudioDrivers.md”) for detailed technical guidance.




### **9. Visual Aids & Documentation**

- **Wireframes & UI Mockups:**
    - Refer to the attached “orp_Wireframes_v2.md” file for detailed wireframe illustrations of the main screen (single and dual tab views) and the unified bottom panel in its four modes (Labelling, Editor, Routing, Preferences).
- **High‑Fidelity Prototypes and Flow Diagrams:**
    - Include detailed UI mockups (created in Figma or Sketch) and user flow diagrams to document user interactions.
- **System Architecture Diagrams & Database ERDs:**
    - Provide diagrams for system components and detailed ERDs for both clip metadata and session metadata (see “orp_ClipMetadataManifest_v0.md” and “orp_SessionMetadataManifest_v0.md”).




### **10. Technical Stack Recommendations (Optimized for 2025 Standards)**

|  |  |  |
| --- | --- | --- |
| **Audio Engine** | JUCE (C++) | PortAudio |
| **DSP Processing** | Rubber Band, ZTX, SoundTouch | Eigen (for matrix math) |
| **UI Framework** | JUCE (Desktop), React Native (iOS Companion App) | Qt, SwiftUI |
| **Networking** | WebSockets, MIDI, OSC | liblo (OSC) |
| **Database & Logs** | SQLite, JSON | LevelDB |
| **Remote Control** | WebSockets, Bluetooth MIDI | OpenStageControl |
| **Deployment** | GitHub Actions, Fastlane (iOS), Docker (for container builds) | Jenkins |




### **11. Development Priorities & Workflow**

1. **Core Audio Engine & DSP Integration:**
    - Build a multi‑threaded, low‑latency audio engine with sample‑accurate timing and robust driver integration.
2. **UI & Clip Editor Implementation:**
    - Develop a responsive waveform editor with dedicated mouse and touchscreen controls.
    - Implement a configurable clip grid with support for clip button stretching.
    - Build the unified bottom panel with four modes (Labelling, Editor, Routing, Preferences).
3. **Remote Control & Network Integration:**
    - Develop the iOS companion app and implement secure, reliable remote control via WebSockets, Bluetooth, MIDI, and OSC.
4. **Session Management & Media Handling:**
    - Implement robust session saving/loading, detailed logging, intelligent file recovery, and project media management.
5. **Audio Endpoint & Driver Configuration:**
    - Configure audio endpoints (outputs and inputs) with dynamic Mono/Stereo settings and fallback mechanisms.
6. **Performance Optimization & Testing:**
    - Perform extensive cross‑platform testing (macOS, Windows, iOS) and optimize buffer management, latency, and resource usage.
7. **CI/CD and Deployment:**
    - Automate builds and testing via GitHub Actions, Fastlane, and Docker.
8. **Security & Data Integrity:**
    - Enforce secure coding practices, encrypted communications, and rigorous input validation to protect performance-critical operations.
9. **Documentation & Visual Aids:**
    - Maintain comprehensive documentation, including high‑fidelity mockups, flow diagrams, system architecture charts, and database schema diagrams.
10. **Driver & Platform‑Specific Considerations:**
    - Integrate detailed driver management per our Audio Drivers report to ensure dependable performance across all platforms.




### **12. Expected Outcome**

A robust, crash‑proof, professional soundboard application that delivers:

- Ultra‑low‑latency audio performance and real‑time, sample‑accurate processing.
- Flexible clip management with a configurable grid (including dual‑tab support and clip button stretching).
- A unified bottom panel offering four distinct modes (Labelling, Editor, Routing, Preferences) for context‑specific controls.
- Comprehensive session management with intelligent file recovery and detailed logging.
- Dynamic audio endpoint configuration (up to five stereo outputs, two stereo inputs, one mono LTC) with Mono/Stereo configurability and fallback to system time.
- Rock‑solid audio driver integration across macOS (CoreAudio), Windows (ASIO/WASAPI), and iOS (CoreAudio/Audio Units/AUv3).
- Seamless remote control via an iOS companion app and secure network communications.
- Extensive visual documentation and detailed technical diagrams to guide development and ensure cross‑platform compatibility.




### **13. Additional Considerations**

- **Future Expansion:**
    - Consider integration of a web‑based controller (using React + WebSockets) and AI‑powered auto‑tagging.
- **Docker Deployment:**
    - Utilize Docker Desktop for containerized builds to ensure consistency across platforms and automated pipelines for streamlined updates.
- **Platform-Specific Deployment:**
    - Follow the attached appendices for best practices on deploying on Apple Silicon, Windows, and other target platforms.




This final comprehensive prompt (vX) incorporates all essential features, best practices, and insights (including those derived from the SpotOn manual) to provide Cursor AI’s Composer Agent with a complete, detailed blueprint for our soundboard application.