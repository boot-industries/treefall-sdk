# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**

#### **Objective:** Develop a **high-performance, professional-grade soundboard application** for use in **broadcasting, live performances, theater productions, and professional audio workflows**. This software must support **Windows and macOS** with an **iOS companion app** for remote control.

The application will feature **real-time, ultra-low-latency audio playback**, **advanced DSP processing**, **MIDI/OSC integration**, **multi-channel audio routing**, **session logging**, and **intelligent playback behavior**. It must be **highly stable and optimized for multi-core processing**, with a **modern, sleek, and responsive UI**.




## **Core Functional Requirements**

### **1. Real-Time Audio Engine & Performance**

- **Ultra-low-latency playback** (optimized for real-time triggering).
- **Multi-format audio support**: MP3, WAV, FLAC, AIFF, OGG, WMA.
- **Multi-threaded audio engine** with **real-time priority execution**.
- **Sample-accurate playback** with **independent pitch and time processing**:
    - **Time-stretching** without altering pitch.
    - **Pitch-shifting** independent of speed.
    - Implement via **Rubber Band Library, SoundTouch, or ZTX DSP**.
- **Multi-channel audio routing**:
    - Assign sounds to specific **outputs (Dante, USB, virtual audio buses)**.
- **Choke Groups & Intelligent Playback**:
    - Assign sounds to **grouped channels (A/B/C/D)** to prevent overlapping playback.
- **Configurable fade-in/out and crossfades**.
- **Pre-loading and caching** of frequently used sounds to optimize response time.




### **2. Clip Editing & User Interface**

- **Modern, responsive UI** built with **JUCE (for desktop)** and **React Native (for iOS companion app)**.
- **Advanced waveform visualization**:
    - **Precision waveform zooming**.
    - **Snap-to-grid** and **drag-to-trim** functionality.
    - **Real-time audio scrubbing** and **cue point editing**.
- **Color-coded clip organization**:
    - Assign custom colors for fast visual identification.
- **Countdown timer for active clips** (total duration + time remaining).
- **Multi-bank system**:
    - Users can organize clips into **1-8 banks**, each supporting **custom grid layouts**.
- **Hotkeys and shortcut customization**:
    - Keyboard shortcuts for launching clips, adjusting volume, pausing playback.
- **MIDI & OSC integration**:
    - **Velocity-sensitive MIDI triggering** for dynamic playback.
    - **OSC support** for advanced remote triggering via external controllers.




### **3. Audio Recording & Live Capture**

- **Real-time audio recording**:
    - Users can **record sounds and immediately assign them to buttons**.
    - **Simultaneous playback & recording**.
    - Logic: If **no clip is playing**, the recorded sound is **audible**; if a **clip is playing**, the recording is **silent but captured**.
- **Metadata embedding** in recorded audio clips (name, timestamp, duration, routing info).
- **Auto-save and session-based recording management**.




### **4. Remote Control (iOS Companion App)**

- **Standalone OR remote control mode**:
    - Can either **run its own soundboard** or **remotely control the desktop app**.
- **Trigger clips remotely** via **WiFi, Bluetooth, or WebSockets**.
- **Live monitoring** of active clips (waveform previews, countdown timers).
- **MIDI/OSC passthrough** for external control from iOS to desktop app.
- **Support for iPhone & iPad (iPadOS multitasking enabled)**.




### **5. Logging, Exporting & Session Management**

- **Real-time playback logging**:
    - Capture **timestamped logs of every clip played** (start time, duration, stop time).
    - Include **clip metadata (name, category, audio properties, assigned hotkey)**.
- **Automated log export**:
    - Export logs in **CSV, JSON, or XML format**.
    - **Auto-sync logs to cloud storage (Google Drive, Dropbox, AWS S3)**.
- **Session management**:
    - Users can **save, load, and duplicate** complete session states.
    - Sessions include **all clips, settings, cue sequences, banks, and configurations**.




## **Tech Stack Recommendations (Optimized for 2025 Standards)**

|  |  |  |
| --- | --- | --- |
| **Audio Engine** | **JUCE (C++)** | PortAudio |
| **DSP Processing** | **Rubber Band, ZTX, SoundTouch** | Eigen (for matrix math) |
| **UI Framework** | **JUCE (Desktop), React Native (iOS Companion App)** | Qt, SwiftUI |
| **Networking** | **WebSockets, MIDI, OSC** | liblo (OSC) |
| **Database & Logs** | **SQLite, JSON** | LevelDB |
| **Remote Control** | **WebSockets, Bluetooth MIDI** | OpenStageControl |
| **Deployment** | **GitHub Actions, Fastlane (iOS), Docker (for containerized builds)** | Jenkins |




## **Development Priorities & Workflow**

1. **Core Audio Engine Development**
    - Build **low-latency, multi-threaded DSP engine** (C++/JUCE).
    - Implement **multi-channel routing and choke groups**.
2. **UI & Clip Editor Development**
    - Develop **responsive waveform rendering** with real-time scrubbing.
    - Implement **drag-to-trim and cue point management**.
3. **Remote Control Implementation**
    - Develop **iOS companion app** with WebSockets & MIDI/OSC integration.
4. **Performance Optimization**
    - Optimize **real-time memory management** and **multi-core processing**.
5. **Session Management & Logging**
    - Implement **auto-save, session duplication, and logging exports**.
6. **Beta Testing & Deployment**
    - **Stress test under professional workloads**.
    - **Prepare automated builds with CI/CD pipeline** (GitHub Actions, Fastlane).




## **Expected Outcome**

A **robust, crash-proof, professional soundboard application** that delivers **real-time, ultra-low-latency audio performance**, **intelligent playback features**, and **seamless remote control capabilities**. Built on a **modern, scalable architecture**, it will be **optimized for high-performance professional environments**, ensuring reliability in **live events, broadcasting, and theater productions**.




## **Additional Considerations**

- **Future Expansion**:
    - **Web-based controller** (React + WebSockets) for remote access.
    - **AI-powered auto-tagging** of sound clips for fast organization.
- **Docker Deployment**:
    - Using **Docker Desktop** for **containerized builds and cross-platform consistency**.
    - Deployment pipelines to ensure **efficient updates and maintenance**.




This version of the **Cursor AI prompt** refines the **previous drafts** into a **clear, optimized, and structured document**, emphasizing **performance, modern UI/UX, and cross-platform compatibility**. It also integrates **best practices for professional audio application development in 2025**, ensuring that the resulting application is **sleek, stable, and production-ready**.