---
source: standard-notes
sn_filename: "OCC004 Cursor Agent Prompt v4-c55f4b58.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**


#### **Objective:** Develop a high-performance, professional-grade soundboard application for use in broadcasting, live performances, theater productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real-time, ultra-low-latency audio playback, advanced DSP processing, multi-channel routing, and intelligent clip management—all built on a modern, scalable, and crash-proof architecture optimized for multi-core processing.


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


### **2. Clip Groups and Audio Output Routing**

- Every clip must be assigned to one of four **Clip Groups (A, B, C, or D)**. Each group serves a dual purpose:
- 1. **Output Routing:**
2. - **Group A:** Routes audio to outputs 1–2
- **Group B:** Routes audio to outputs 3–4
- **Group C:** Routes audio to outputs 5–6
- **Group D:** Routes audio to outputs 7–8
- **Editor/Audition Mode:** Routes audio to outputs 9–10
3. **Playback Management:**
4. - Each group has an optional togglable FIFO-based "choke" feature. When enabled, triggering a new clip in a group will stop (or fade out) any previous clip within the same group, ensuring clean transitions.


### **3. Clip Editing & User Interface**

- **Responsive, modern UI** developed using JUCE for the desktop and React Native for the iOS companion app.
- **Advanced waveform editor** with specific mouse-based controls:
- - **Left-click** sets the **IN point**.
- **Right-click** sets the **OUT point**.
- **Middle-click** jumps the playhead.
- **Control+click** sets up to **4 cue points per clip** on a FIFO basis.
- The waveform area is integrated into an editor panel that appears from the **bottom half of the screen** (rather than a pop-up). This allows the main tab view to remain visible for launching or stopping clips while editing.
- The editor includes its own audio player with basic transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
- - User-configurable grid layout for clip buttons, with a maximum size of **10 rows x 12 columns** per tab.
- Supports **8 full-screen tabs by default**.
- Option to display two tabs simultaneously (with FIFO logic governing which two tabs are active/highlighted).
- Primary page elements remain fixed (aside from adjustable pane widths in dual-tab view); the editor panel itself is fixed in size.


### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
- - Recorded sounds are captured directly into the designated button, eliminating the need for later assignment.
- Simultaneous recording and playback are supported. If no clip is playing, the recording is audible; if a clip is already playing, the recording is captured silently.
- **Metadata embedding** for each recorded clip (including name, timestamp, duration, and routing info).
- **Auto-save and session-based management** for recordings.


### **5. Remote Control (iOS Companion App)**

- The iOS app can operate in two modes:
- - As a standalone soundboard.
- Or as a remote controller for the desktop application.
- Supports triggering clips via WiFi, Bluetooth, or WebSockets.
- Provides live monitoring of active clips with waveform previews and countdown timers.
- Facilitates MIDI/OSC passthrough for seamless external control.
- Fully optimized for both iPhone and iPad (including iPadOS multitasking).


### **6. Logging, Exporting & Session Management**

- **Real-time logging:**
- - Capture timestamped logs for every clip played (recording start, duration, and stop times).
- Log includes clip metadata such as name, category, audio properties, and assigned hotkeys.
- **Automated log export:**
- - Export logs in CSV, JSON, or XML formats.
- Option to auto-sync logs to cloud storage services (Google Drive, Dropbox, AWS S3).
- **Comprehensive session management:**
- - Users can save, load, and duplicate complete session states.
- Sessions include all clips, settings, cue sequences, banks, and configuration data.


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
4. - Build a responsive waveform editor with dedicated mouse logic (IN/OUT points, playhead jumps, cue points).
- Integrate the editor as a bottom-half screen extension while keeping the main tab view accessible.
- Ensure the editor’s audio player is routable (outputs 9–10).
- Implement a user-configurable clip grid (up to 10x12 per tab) with support for 8 full-screen tabs and dual-tab view.
5. **Remote Control Implementation:**
6. - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
7. **Performance Optimization:**
8. - Optimize real-time memory management and multi-core processing.
9. **Session Management & Logging:**
10. - Implement robust auto-save, session duplication, and automated log export features.
11. **Beta Testing & Deployment:**
12. - Conduct extensive stress testing under professional workloads.
- Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.


---




## **Expected Outcome**

A robust, crash-proof, professional soundboard application delivering real-time, ultra-low-latency audio performance, intelligent clip management, and flexible routing. Key features include:

- An integrated bottom-half waveform editor with dedicated mouse controls and a routable audio player.
- Clip Groups (A/B/C/D) that manage both audio output routing and optional FIFO-based playback control (with predefined routing: Group A → Outputs 1–2, Group B → Outputs 3–4, Group C → Outputs 5–6, Group D → Outputs 7–8, and Editor/Audition → Outputs 9–10).
- A user-configurable grid interface for managing clips across 8 full-screen tabs (with an optional dual-tab view).
- Seamless remote control capabilities via an iOS app.
- Comprehensive logging, session management, and export features.


---




## **Additional Considerations**

- **Future Expansion:**
- - Integration of a web-based controller (using React + WebSockets) for remote access.
- Implementation of AI-powered auto-tagging to streamline clip organization.
- **Docker Deployment:**
- - Utilize Docker Desktop for containerized builds, ensuring cross-platform consistency and simplified deployment.
- Set up automated pipelines for efficient updates and maintenance.


---



This comprehensive prompt should now provide Cursor AI’s Composer Agent with clear, unambiguous instructions to design and implement a modern, scalable, and professional soundboard application based on our collective vision.

Let me know if any further refinements are needed!


##### You said:

Good. Now I'd like you to advise on a set of Cursor "rules" that would provide the best results based on the scope of our project here. Referring to the best practices for Cursor AI "rules," please provide a set of rules here.


###### ChatGPT said:

Reasoned about Cursor AI "rules" for soundboard project for 10 seconds

Below is a recommended set of Cursor “rules” tailored to our project’s scope. These rules follow best practices and are designed to ensure that the Composer Agent delivers clear, maintainable, and high-performance results:

1. **Strict Adherence to Scope:**
2. - Always base implementations on the comprehensive prompt.
- Do not deviate from the defined objectives (real-time performance, cross-platform compatibility, advanced clip management, etc.).
3. **Clear Modular Architecture:**
4. - Divide functionality into distinct modules (audio engine, clip grouping, UI/editor, recording, remote control, logging/session management).
- Each module should have clear responsibilities and interfaces to allow future updates or replacements.
5. **Real-Time Performance & Low Latency:**
6. - Prioritize multi-threaded, sample-accurate processing for ultra-low-latency audio playback.
- Use optimized DSP libraries (Rubber Band, SoundTouch, or ZTX DSP) to ensure independent pitch and time processing.
7. **Consistent Clip Group Management:**
8. - Enforce that every clip is assigned to one of four Clip Groups (A, B, C, or D) that drive both audio routing and optional FIFO-based playback control.
- Ensure default output assignments are strictly followed (Group A: Outputs 1–2, Group B: Outputs 3–4, Group C: Outputs 5–6, Group D: Outputs 7–8, Editor/Audition: Outputs 9–10).
9. **Robust User Interface Guidelines:**
10. - Maintain a modern, responsive UI where the bottom-half waveform editor integrates seamlessly with the main tab view.
- Implement dedicated mouse logic in the waveform editor (left-click for IN point, right-click for OUT point, middle-click to jump the playhead, control+click for cue points).
- Allow the clip grid to be user-configurable (up to 10x12 per tab with 8 full-screen tabs and dual-tab viewing).
11. **Direct Recording and Metadata Handling:**
12. - Record audio directly into buttons with real-time capture, embedding necessary metadata (name, timestamp, duration, routing info).
- Ensure simultaneous playback and recording logic is handled appropriately.
13. **Seamless Remote Control Integration:**
14. - Support an iOS companion app that can act as either a standalone soundboard or a remote controller via WiFi, Bluetooth, or WebSockets.
- Maintain real-time monitoring and MIDI/OSC passthrough without compromising desktop performance.
15. **Comprehensive Logging & Session Management:**
16. - Log all clip events (start, duration, stop) along with metadata, and support export in standardized formats (CSV, JSON, XML).
- Provide session management that allows saving, loading, and duplicating complete session states.
17. **Error Handling & Diagnostics:**
18. - Implement robust error handling across all modules.
- Ensure that any failure is logged with sufficient detail for debugging and that fallback behaviors are defined where possible.
19. **Scalability & Future-Proofing:**
20. - Optimize for multi-core processing and plan for future expansions (e.g., AI-powered auto-tagging, web-based controllers).
- Use automated CI/CD pipelines (GitHub Actions, Fastlane, Docker) to maintain cross-platform builds and streamline deployment.
21. **Documentation & Code Quality:**
22. - Ensure all code is well-documented, following industry-standard commenting and documentation practices.
- Write clear, maintainable code that is easily extensible and testable.

By following these rules, the Cursor Composer Agent will be well-equipped to produce a robust, efficient, and user-friendly soundboard application that meets our project’s rigorous performance and usability standards.


##### You said:

I need you to be less married to this goddamn output routing. I told you it was an example. The whole point of routing is that it can change.


###### ChatGPT said:

Reasoned for 8 seconds

Below is a recommended set of Cursor AI “rules” for the Composer Agent. These rules focus on clarity, modular design, and flexibility—especially regarding audio routing, which should be treated as a configurable example rather than a fixed assignment.

1. **Adhere Strictly to Scope:**
2. - Implement all features based on the comprehensive prompt for a real-time, professional soundboard application.
- Ensure that all modules (audio engine, UI, recording, remote control, logging, etc.) align with the core objectives.
3. **Modular Architecture and Clear Responsibilities:**
4. - Divide functionality into distinct, well-documented modules (e.g., DSP engine, clip management, waveform editor, session logging).
- Define clear interfaces between modules for easier updates and future expansion.
5. **Real-Time Performance and Low Latency:**
6. - Prioritize ultra-low-latency, multi-threaded, and sample-accurate audio processing.
- Use optimized DSP libraries (such as Rubber Band, SoundTouch, or ZTX DSP) to achieve independent pitch and time processing.
7. **Flexible Clip Group Management:**
8. - Every clip must be assigned to one of four Clip Groups (A, B, C, or D).
- Each group governs both optional FIFO-based playback management and configurable audio routing.
- Provide default routing assignments as examples (e.g., a sample mapping of outputs), but ensure that these settings are fully customizable and can change dynamically.
9. **Intuitive, Responsive User Interface:**
10. - Develop a modern UI where the waveform editor is an integrated bottom-half panel, leaving the main tab view available for launching or stopping clips.
- Implement dedicated mouse controls in the waveform editor (left-click for IN point, right-click for OUT point, middle-click to jump the playhead, and control+click to set up to 4 cue points).
- Ensure the editor’s audio player is fully routable, with flexible output settings.
11. **Direct Recording with Embedded Metadata:**
12. - Record audio directly into designated clip buttons, eliminating extra assignment steps.
- Embed metadata (e.g., name, timestamp, duration, routing details) within each recorded clip.
13. **Seamless Remote Control Integration:**
14. - Enable an iOS companion app that can function as a standalone soundboard or as a remote controller via WiFi, Bluetooth, or WebSockets.
- Maintain real-time monitoring and MIDI/OSC passthrough without degrading desktop performance.
15. **Robust Logging and Session Management:**
16. - Log every clip event (start time, duration, stop time) along with relevant metadata.
- Support session management features that allow users to save, load, and duplicate complete session states.
- Offer automated log export in multiple formats (CSV, JSON, XML) with options for cloud syncing.
17. **Error Handling, Diagnostics, and Code Quality:**
18. - Implement comprehensive error handling and diagnostic logging across all modules.
- Ensure code is clean, well-documented, and follows best practices for maintainability and scalability.
19. **Flexibility and Customizability:**
20. - Treat default configurations—including output routing—as examples that serve as starting points.
- Allow users to dynamically adjust routing assignments, clip group behavior, and other settings to suit varying environments and needs.

By following these rules, Cursor AI’s Composer Agent will be able to generate an architecture that is both robust and adaptable, meeting the demanding requirements of a modern professional soundboard application.
