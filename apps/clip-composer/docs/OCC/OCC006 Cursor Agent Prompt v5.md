---
source: standard-notes
sn_filename: "OCC006 Cursor Agent Prompt v5-d51e6878.txt"
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
- - Intuitive touch gestures mirror the mouse actions—tapping to set IN/OUT points, dragging to scrub the playhead, and multi-touch for cue point management.
- The waveform editor is embedded in a panel that slides up from the **bottom half of the screen** so that the main tab view remains visible for launching or stopping clips while editing.
- The editor includes its own audio player with basic transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
- - User-configurable grid layout for clip buttons, with a maximum size of **10 rows x 12 columns** per tab.
- Supports **8 full-screen tabs by default**.
- Option to display two tabs simultaneously (using FIFO logic for active/highlighted tabs).
- Primary page elements remain fixed (aside from adjustable pane widths in dual-tab view); the editor panel itself remains fixed in size.


---




### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
- - Recorded sounds are captured directly into the designated button, eliminating the need for later assignment.
- Supports simultaneous recording and playback. If no clip is playing, the recording is audible; if a clip is already playing, the recording is captured silently.
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
- - **Media Folder Selection:**
- - When creating a new project, prompt the user to select a default project media folder to serve as the central repository for all clip files.
- For existing projects, allow reassigning the project media folder if files have been relocated.
- **Clip Reconciliation:**
- - Support reconciling clip files to identical assets in different directories while preserving all session-relevant metadata, DSP settings, and edit points.
- Prompt the user to confirm reassignment when a clip is found in an alternate location.


---




### **7. Global Tempo Mode & AI Beat-Sync**

- **Global Tempo Mode:**
- - An optional mode where the entire session is assigned a specific BPM.
- When enabled, all clips played are automatically warped to sync with the global tempo.
- **Clip-Specific Warp Options:**
- - Provide options on each clip to warp by pitch, by time, or by both.
- **User Controls for Global Tempo:**
- - Display the global BPM on the main window when enabled; hide it when disabled.
- Include an on-screen tap tempo button as well as support for external control inputs.
- Provide up/down arrow controls to adjust the BPM in increments of 0.5 BPM.
- **Behavior on Mode Exit:**
- - If a clip is already playing when Global Tempo mode is exited, that clip continues at its current tempo.
- Subsequent clips revert to playing at their normal speed and pitch, independent of the previously active Global Tempo mode.
- **AI Beat-Detection:**
- - Integrate AI beat-detection to automatically match and warp clips to the global BPM—similar to the “SYNC” function found in virtual DJ software.


---




### **8. Clip Group Filters & Intelligent Crossfade**

- **Per-Group Filters:**
- - Each Clip Group (A, B, C, and D) is equipped with two filters—a High-Pass Filter (HPF) and a Low-Pass Filter (LPF)—totaling 8 filters across the groups.
- **Filter Parameters:**
- - **HPF:** Frequency range from 10 Hz to 1000 Hz.
- **LPF:** Frequency range from 1000 Hz to 22000 Hz.
- Both filters should support adjustable slopes ranging from 6 dB to 18 dB.
- **Clip-Specific & External Control:**
- - Filter settings can be defined as static values stored within each clip and/or modified in real-time via external control data.
- **Intelligent Crossfade of Filters:**
- - When transitioning between clips within a Clip Group, if the filter settings differ, the group's filters should intelligently ramp—synchronized with the crossfade—to match the new clip’s settings.
- Even if FIFO “choke” is not enabled for the group, the second clip’s filter settings take precedence.


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
5. **Remote Control Implementation:**
6. - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
7. **Performance Optimization:**
8. - Optimize real-time memory management and multi-core processing.
9. **Session Management, Logging & Intelligent File Recovery:**
10. - Implement robust auto-save, session duplication, and automated log export features.
- Integrate the intelligent search mechanism to locate missing files based on clip metadata when sessions are loaded.
11. **Global Tempo Mode & Filter Integration:**
12. - Develop the Global Tempo mode with AI beat-detection, tap tempo, up/down arrow BPM adjustments, and per-clip warp options.
- Implement per-Clip Group filter controls with intelligent crossfade ramping for both HPF and LPF settings.
13. **Beta Testing & Deployment:**
14. - Conduct extensive stress testing under professional workloads.
- Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.


---




## **Expected Outcome**

A robust, crash-proof, professional soundboard application delivering:

- Real-time, ultra-low-latency audio performance.
- Intelligent clip management with flexible routing and dynamic media reconciliation.
- An integrated bottom-half waveform editor with both touchscreen and mouse controls.
- Optional Global Tempo mode with AI beat-detection and per-clip warp options.
- Advanced per-Clip Group filters (HPF and LPF) that intelligently ramp during crossfades.
- Comprehensive logging, session management, and an intelligent missing file search feature.
- Seamless remote control capabilities via an iOS companion app.


---




## **Additional Considerations**

- **Future Expansion:**
- - Integration of a web-based controller (using React + WebSockets) for remote access.
- Implementation of AI-powered auto-tagging to streamline clip organization.
- **Docker Deployment:**
- - Utilize Docker Desktop for containerized builds, ensuring cross-platform consistency and simplified deployment.
- Set up automated pipelines for efficient updates and maintenance.


---



This updated prompt now comprehensively includes the new Global Tempo mode and Clip Group filter functionalities along with all existing features, providing Cursor AI’s Composer Agent with clear and detailed instructions for developing the application. Let me know if any further adjustments are needed!


##### You said:

Now help me prepare a visual guide for Cursor AI. What tools can we use to collect images that should inspire the visual design of the user interface?


###### ChatGPT said:

Reasoned for a couple of seconds

Here are several tools and approaches you can use to create a compelling visual guide for Cursor AI’s UI design:

- **Pinterest & Milanote:**
Use Pinterest to curate inspirational images from various design styles. Milanote is excellent for mood boarding—it lets you organize images, notes, and sketches in a flexible, visual canvas that can evolve as your design direction develops.
- **Figma & Miro:**
Figma isn’t just for high-fidelity designs—it also supports collaborative mood boards. Miro provides a whiteboard space where you can drag-and-drop images, annotate them, and build a visual narrative for the UI’s look and feel.
- **Dribbble & Behance:**
Browse Dribbble and Behance for UI design inspiration. You can save or bookmark your favorite designs to help inform your own style, ensuring the soundboard interface is modern, sleek, and user-friendly.
- **Unsplash & Adobe Stock:**
For high-quality imagery and textures that can serve as background inspiration or to inform your color palettes and design elements, check out Unsplash for free photos or Adobe Stock for curated collections.
- **Notion:**
As a Notion subscriber, you can create a dedicated page to embed these images, link to inspiring designs, and annotate your ideas. This keeps your visual inspiration centralized and easily shareable with your team.

Using these tools together can help you assemble a comprehensive visual guide that captures the desired aesthetic, functionality, and user experience for Cursor AI’s interface.


##### You said:

[Ovation_10.1_User_Manual.pdf

PDF

]()

[orp_ConductorApp_prompt-v3.md

File

]()

Ok. Moving along now... Run a deep analysis of the attached Merging Ovation 10 user manual, and report back with features that we might be missing in our build. We won't want all of the features offered in Ovation, but this is a way for us to make sure we haven't missed anything important.


###### ChatGPT said:



Reasoned for 8 seconds

Based on a deep analysis of the Ovation 10 user manual , here are several advanced features and design elements that you might consider to ensure our build hasn’t overlooked important functionality. While we don’t necessarily need to replicate every Ovation feature, these items could inspire enhancements or options to better serve professional live and broadcast environments:

1. Advanced Cue List Management
• Ovation offers multiple types of cue lists (standard, timed, hot keys, custom keys, and browser-based) that allow flexible scheduling and triggering of media.
• Consider adding an optional timed cue list mode or scheduled playback for situations where clips need to be triggered at predetermined times.
• Support for customized cue layouts (akin to hot keys or custom keys) can provide alternative control schemes for different performance contexts.
2. Automation & Snapshot Capabilities
• Ovation supports snapshot automation and dynamic mixer automation, letting users capture and recall specific mixer or effect settings on cue fire.
• You might explore integrating automation features that allow snapshots of filter settings, volume levels, or even global tempo adjustments that can be recalled during a performance.
3. Enhanced Hardware Controller Integration
• While our design includes an iOS companion app for remote control, Ovation is built to interface with various hardware control surfaces (via MIDI, OSC, GPI, etc.).
• Consider broadening support to include external hardware controllers, allowing tactile cue triggering and mixer adjustments—which can be crucial in live performance environments.
4. Ambisonic & Immersive Audio Support
• Ovation’s capability to handle ambisonic audio cues (up to high orders) provides immersive sound experiences.
• Depending on your target audience, you may want to evaluate whether adding optional ambisonic processing or routing (beyond standard multi-channel routing) could set your product apart in niche applications.
5. Multi-Sequencer Synchronization
• Ovation can synchronize multiple sequencers for redundancy or multi-zone installations.
• Although our primary focus is on a single robust soundboard, incorporating an optional mode for synchronizing multiple instances could be beneficial for large-scale events or backup scenarios.
6. Detailed Mixer & Audio Control Features
• Ovation includes advanced mixer configuration, such as adjustable channel gain, panning, and output slot management.
• Even though our design covers multi-channel routing and global tempo mode, you might consider enhancing the mixer controls—such as adding more granular gain, pan, or even integration of a virtual mixer interface—to offer finer control over live audio.
7. Rich Cue Feedback & Visual Indicators
• The Ovation manual describes extensive visual feedback for cues, including states (Asleep, Ready, Playing, Paused), progress bars, and timers.
• Ensuring that our grid of clip buttons provides detailed real-time status (e.g., visual progress indicators, state changes) could further improve usability during live shows.
8. Robust File Management & Media Consolidation
• Ovation’s workflow includes consolidating media files from various sources into a single repository and ensuring that all media assets are packaged with a show.
• While we have intelligent file recovery and media folder management, consider adding an option for media consolidation—especially useful when a project is migrated between systems.
9. Extensive Remote Control Options
• Beyond the iOS companion app, Ovation supports control via various protocols (OSC, MIDI, IP, etc.), enabling diverse remote and integrated control scenarios.
• Expanding remote control options (or providing flexible configuration for external controllers) can enhance the application’s versatility across different production setups.

By comparing our current feature set with the robust capabilities detailed in the Ovation 10 manual, you can identify opportunities to incorporate optional advanced modes (like timed cue lists or multi-sequencer sync) and enhance control interfaces (both visual and hardware-based). This not only ensures that our product meets professional standards but also offers additional flexibility to adapt to various live performance scenarios.

Let me know if you’d like to dive deeper into any specific area or adjust our roadmap based on these insights!

Sources


##### You said:

1) I don't want to get into advanced types of cue lists, scheduled clips and the simple cue lists that we are currently provisioning for (?) are sufficient for my current needs. I would however like to explore the support for customized cue layouts to make sure our design isn't overlooking anything there. 2) I'm not sure this is desirable. 3) Mouse, keyboard (mappable shortcuts), and touchscreen control of the native app, as well as external MIDI/OSC and iOS control are sufficient. No additional hardware control needed. 4) Not required. 5) Not required. 6) Let's explore what might be missing in our design here. 7) This is important, we will need to emulate this behaviour. 8) This is even more important. 9) Already discussed above. Where does that leave us? Provide an updated report on desirable functions that we should glean from the provided Ovation 10 manual.


###### ChatGPT said:

**Reasoned About User Interface Flexibility and Feature Support** for 8 seconds

Based on your feedback and the key takeaways from the Ovation 10 manual, here’s an updated report on the desirable functions we should incorporate into our build:

1. Customized Cue Layouts
• Although our current simple cue lists meet basic needs, we should explore options for customized cue layouts. This means allowing users to tailor the appearance of cue buttons—adjusting colors, fonts, sizes, and positions—to create a more personalized and intuitive interface.
2. Enhanced Mixer & Audio Control Features
• Review our mixer controls to ensure we’re not missing essential functions found in professional systems. Consider:
3. - Detailed channel gain controls and precise panning adjustments.
- Intuitive output routing with clear visual feedback.
- Optional support for mixer snapshots or automation recall (if it doesn’t complicate the core workflow).
4. Rich Cue Feedback & Visual Indicators
• Emulate Ovation’s behavior by providing clear, real-time visual feedback for cues:
5. - Display distinct statuses such as Asleep, Ready, Playing, and Paused.
- Incorporate progress bars, elapsed/remaining time counters, and color-coded highlights (or even flashing borders) to indicate cue focus and state changes. • These indicators should be seamlessly integrated into our cue list interface for easy monitoring during live shows.
6. Robust File Management & Media Consolidation
• This is a crucial area:
7. - Implement a system for consolidating all media files into a designated project folder during project creation or consolidation.
- Include an intelligent missing file search mechanism that uses clip metadata to locate missing assets.
- Allow clip reconciliation—so that if files are found in alternate directories, session-specific metadata (such as DSP settings and edit points) is preserved while reassigning the file location.

In summary, while our current design meets our core needs, we should refine it further by: • Allowing customized cue layouts, • Enhancing mixer controls for finer audio management, • Emulating rich, real-time visual feedback for cues, • And ensuring robust file management and media consolidation.
