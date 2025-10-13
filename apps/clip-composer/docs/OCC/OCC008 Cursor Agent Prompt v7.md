# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**

#### **Objective:** Develop a high-performance, professional-grade soundboard application for use in broadcasting, live performances, theatre productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real-time, ultra-low-latency audio playback, advanced DSP processing, multi-channel routing, and intelligent clip management—all built on a modern, scalable, and crash-proof architecture optimized for multi-core processing.




## **Core Functional Requirements**

### **1. Real-Time Audio Engine & Performance**

- **Ultra-low-latency playback** optimized for real-time triggering.
- **Multi-format audio support:** MP3, WAV, FLAC, AIFF, OGG, WMA, and AAC (m4a).
- **Multi-threaded audio engine** with real-time priority execution.
- **Sample-accurate playback** featuring independent pitch and time processing:
    - **Time-stretching** (without altering pitch).
    - **Pitch-shifting** (independent of playback speed).
    - Implement using libraries such as Rubber Band, SoundTouch, or ZTX DSP.
- **Multi-channel audio routing:**
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
        - Each group has an optional togglable FIFO-based “choke” feature. When enabled, triggering a new clip in a group will stop (or fade out) any previous clip within the same group, ensuring clean transitions.
    - **Note:** These routing assignments serve as examples; the system must allow dynamic reconfiguration of routing settings.




### **3. Clip Editing & User Interface**

- **Responsive, modern UI** developed using JUCE for the desktop and React Native for the iOS companion app.
- **Advanced waveform editor** with integrated mouse and touchscreen controls:
    - **Mouse Controls:**
        - **Left-click** sets the **IN point**.
        - **Right-click** sets the **OUT point**.
        - **Middle-click** jumps the playhead.
        - **Control+click** sets up to **4 cue points per clip** on a FIFO basis.
    - **Touchscreen Controls:**
        - Intuitive gestures mirror mouse actions, enabling tap-to-set IN/OUT points, drag-to-scrub the playhead, and multi-touch for cue point management.
    - The waveform editor is integrated into a panel that appears from the **bottom half of the screen** (instead of as a pop-up), ensuring that the main tab view remains visible for launching or stopping clips while editing.
    - The editor includes its own audio player with basic transport controls that is fully **routable** (using outputs 9–10) for auditioning edits.
- **Clip Button Grid:**
    - User-configurable grid layout for clip buttons, with a maximum size of **10 rows x 12 columns** per tab.
    - Supports **8 full-screen tabs by default**.
    - Option to display two tabs simultaneously (using FIFO logic to govern which two tabs are active/highlighted).
    - **Clip Button Stretching:**
        - Individual clip buttons can be configured to “stretch” and occupy up to 4 grid cells, allowing for larger buttons when desired.
    - Primary page elements remain fixed (aside from adjustable pane widths in dual-tab view); the editor panel itself remains fixed in size.
- **Bottom Panels (Persistent Panels):**
    - **Clip Labelling Panel:**
        - Contains two tabs:
            - **Label & Appearance Tab:** For editing individual clip labels and button appearance.
            - **Metadata Tab:** For editing clip metadata, with support for schema-specific copy and paste (e.g., copying only “playgroup” and “clip colour” data between clips).
    - **Application Preferences Panel:**
        - Provides persistent playback control and general settings.
    - **Visibility Constraint:**
        - Only one bottom panel (either the Clip Labelling Panel or the Application Preferences Panel) may be visible at any given time, ensuring the main grid view remains fully displayed.




### **4. Audio Recording & Live Capture**

- **Direct audio recording into buttons:**
    - Recorded sounds are captured directly into the designated button, eliminating the need for later assignment.
    - Supports simultaneous recording and playback. If no clip is playing, the recording is audible; if a clip is already playing, the recording is captured silently.
- **Metadata embedding** for each recorded clip (including name, timestamp, duration, and routing info).
- **Auto-save and session-based management** for recordings.




### **5. Remote Control (iOS Companion App)**

- The iOS app can operate in two modes:
    - As a standalone soundboard.
    - Or as a remote controller for the desktop application.
- Supports triggering clips via WiFi, Bluetooth, or WebSockets.
- Provides live monitoring of active clips with waveform previews and countdown timers.
- Facilitates MIDI/OSC passthrough for seamless external control.
- Fully optimized for both iPhone and iPad (including iPadOS multitasking).




### **6. Logging, Exporting, Session Management, Intelligent File Recovery & Project Media Management**

- **Real-time Logging:**
    - Capture timestamped logs for every clip played (recording start, duration, and stop times).
    - Logs include clip metadata such as name, category, audio properties, and assigned hotkeys.
- **Automated Log Export:**
    - Export logs in CSV, JSON, or XML formats.
    - Option to auto-sync logs to cloud storage services (Google Drive, Dropbox, AWS S3).
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
        - When a clip file is found in an alternate location, the system maintains all session-relevant clip metadata, DSP settings, and edit points.
        - The user is prompted to confirm the reassignment of the clip file to the new location, ensuring that session integrity is preserved while providing flexibility in managing media file storage.




## **Tech Stack Recommendations (Optimized for 2025 Standards)**

|  |  |  |
| --- | --- | --- |
| **Audio Engine** | JUCE (C++) | PortAudio |
| **DSP Processing** | Rubber Band, ZTX, SoundTouch | Eigen (for matrix math) |
| **UI Framework** | JUCE (Desktop), React Native (iOS Companion App) | Qt, SwiftUI |
| **Networking** | WebSockets, MIDI, OSC | liblo (OSC) |
| **Database & Logs** | SQLite, JSON | LevelDB |
| **Remote Control** | WebSockets, Bluetooth MIDI | OpenStageControl |
| **Deployment** | GitHub Actions, Fastlane (iOS), Docker (for container builds) | Jenkins |




## **Development Priorities & Workflow**

1. **Core Audio Engine Development:**
    - Develop a low-latency, multi-threaded DSP engine using C++/JUCE.
    - Implement multi-channel audio routing with output assignments tied to Clip Groups A/B/C/D.
    - Integrate the optional FIFO-based playback (choke) functionality per group.
2. **UI & Clip Editor Development:**
    - Build a responsive waveform editor with dedicated mouse and touchscreen controls for setting IN/OUT points, playhead jumps, and cue points.
    - Integrate the editor as a bottom-half screen extension while keeping the main tab view accessible.
    - Ensure the editor’s audio player is routable (outputs 9–10).
    - Implement a user-configurable clip grid (up to 10x12 per tab) with support for 8 full-screen tabs and dual-tab view.
    - **Implement clip button stretching:** Allow individual clip buttons to occupy up to 4 grid cells for larger button displays.
    - **Implement Bottom Panels:**
        - Develop a Clip Labelling Panel with two tabs (Label & Appearance, Metadata) supporting schema-specific metadata copy/paste.
        - Develop an Application Preferences Panel for persistent playback control.
        - Enforce that only one bottom panel is visible at any time.
3. **Remote Control Implementation:**
    - Develop the iOS companion app with support for WebSockets, Bluetooth, MIDI, and OSC.
4. **Performance Optimization:**
    - Optimize real-time memory management and multi-core processing.
5. **Session Management, Logging & Intelligent File Recovery:**
    - Implement robust auto-save, session duplication, and automated log export features.
    - Integrate the intelligent search mechanism to locate missing files based on clip metadata when sessions are loaded.
6. **Beta Testing & Deployment:**
    - Conduct extensive stress testing under professional workloads.
    - Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.




## **Expected Outcome**

A robust, crash-proof, professional soundboard application delivering real-time, ultra-low-latency audio performance, intelligent clip management, and flexible routing. Key features include:

- An integrated bottom-half waveform editor that supports both touchscreen and mouse controls, with dedicated controls for IN/OUT points, playhead jumps, and cue points.
- Clip Groups (A/B/C/D) that manage both audio output routing and optional FIFO-based playback control.
- A user-configurable grid interface for managing clips across 8 full-screen tabs (with an optional dual-tab view), including support for clip button stretching to occupy up to 4 grid cells.
- Three persistent bottom panels:
    - A Clip Labelling Panel with two tabs for editing label/button appearance and clip metadata (with schema-specific copy/paste).
    - An Application Preferences Panel for persistent playback control.
    - Only one bottom panel is visible at any given time.
- Seamless remote control capabilities via an iOS app.
- Comprehensive logging, session management, and an intelligent missing file search feature that recovers absent clip files using metadata.
- Robust project media management with support for media folder selection and clip reconciliation.




## **Additional Considerations**

- **Future Expansion:**
    - Integration of a web-based controller (using React + WebSockets) for remote access.
    - Implementation of AI-powered auto-tagging to streamline clip organization.
- **Docker Deployment:**
    - Utilize Docker Desktop for containerized builds, ensuring cross-platform consistency and simplified deployment.
    - Set up automated pipelines for efficient updates and maintenance.




This updated comprehensive prompt now includes the new bottom panels for clip labelling and application preferences, ensuring that our UI design supports persistent, non-pop-up controls while maintaining the main grid view visibility. Let me know if any further adjustments are needed!

##### You said:

Superb. Now let's move on to database design to give Cursor AI an idea of what we need in this area. Use the provided schema below, ordered into three sections, to articulate a separate manifest document for the metadata database that we will need to support our soundboard application. [ I. *Performer Details (Front‑of‑House):* **Track Title** – The track’s name is your primary identifier. **Artist** – Quickly tells you who is performing. **Album** – Helps place the track in context. **Cover Art** – A visual cue that can speed recognition. **Track Length** – Essential for timing and set‑flow decisions. **BPM** – Crucial for beat‑matching and energy management. **Key** – For harmonic mixing and compatibility. **Genre** – A quick tag to know the style or mood of the track. **Playgroup** – User-customized grouping by mood or feel of clip. **Release Date** – Informs you if the track is fresh or a classic. **Format** – The file type, important for playback quality/compatibility. **Label/Catalogue** – Record label or music catalogue information. **Catalogue Number** – A unique identifier assigned by the label for the release. **Comments** – Quick notes for on‑the‑fly reminders. II. *Legal/Rights Details (For Business & Distribution):* **Copyright Owner** – Who legally owns the recording. **Distribution Rights** – Who holds the rights to distribute the track. **Licensing** – The terms under which the track may be used. **Publishing – Rights** – Who owns the publishing rights. **Synchronisation Rights** – Permissions for audiovisual use. **Territorial Restrictions** – Regions where the track can’t be distributed. **Sample Clearance** – Status of any samples used in the track. **Streaming Platforms** – Platforms where the track is available. **UPC** – Universal Product Code for the album. **Revenue – Share** – Percentage of revenue allocated to rights holders. **RIAA Certification** – Sales certification (Gold, Platinum, etc.). **ISRC** – A unique identifier for cataloguing. III. *Production/Industry Details (Back‑of‑House):* **Chart – Performance** – Indicates hit status or popularity. **Recording Date** – When the track was recorded. **Recording Studio** – Where the track was recorded. **Producer** – The production credit. **Composer** – Useful for rights or deeper dive info. **Lyricist** – Similar to the composer; secondary during live selection. **Mix Engineer** – Part of the production details. **Mastering Engineer** – Also important for technical records. **Remix Details** – Indicates if this is a remix or alternate version. **Outtakes** – Unused recordings; more for archival purposes. **Bonus Material** – Extra content that might affect how you use the track. **Derivative Works** – Information about adaptations or remixes derived from the track. ]

###### ChatGPT said:

Reasoned for 10 seconds

Below is a manifest document that defines the metadata database schema for our soundboard application. This document is organized into three sections—Performer Details, Legal/Rights Details, and Production/Industry Details—to support all aspects of clip and session management.




# Metadata Database Manifest for Soundboard Application

## Introduction

This document outlines the schema for the metadata database supporting our soundboard application. The database will store comprehensive information for each audio clip to enable robust session management, intelligent file recovery, and efficient live performance operations. The schema is divided into three primary sections:

- **Performer Details (Front‑of‑House):** Information critical for performance and on‑the‑fly decision-making.
- **Legal/Rights Details (For Business & Distribution):** Data necessary for handling licensing, distribution, and copyright compliance.
- **Production/Industry Details (Back‑of‑House):** Technical and production-related metadata for archival and industry purposes.




## I. Performer Details (Front‑of‑House)

- **Track Title:**
    - Primary identifier for the clip.
- **Artist:**
    - Name of the performer.
- **Album:**
    - The collection or album to which the clip belongs.
- **Cover Art:**
    - Visual asset that can be displayed to speed recognition.
- **Track Length:**
    - Duration of the clip (in seconds or formatted time).
- **BPM:**
    - Beats per minute, crucial for beat‑matching and energy management.
- **Key:**
    - Musical key for harmonic compatibility.
- **Genre:**
    - Tag that indicates the style or mood of the track.
- **Playgroup:**
    - User-defined grouping to organize clips by mood or feel.
- **Release Date:**
    - The date the clip was released; useful for set‑flow decisions.
- **Format:**
    - File type (e.g., WAV, MP3, AAC) relevant for playback quality and compatibility.
- **Label/Catalogue:**
    - Record label or catalogue information to place the clip in context.
- **Catalogue Number:**
    - A unique identifier assigned by the label.
- **Comments:**
    - Free-form notes for quick on‑the‑fly reminders.




## II. Legal/Rights Details (For Business & Distribution)

- **Copyright Owner:**
    - The legal owner of the recording.
- **Distribution Rights:**
    - Parties authorized to distribute the clip.
- **Licensing:**
    - Terms under which the clip may be used.
- **Publishing Rights:**
    - Ownership details regarding publishing.
- **Synchronisation Rights:**
    - Permissions required for audiovisual use.
- **Territorial Restrictions:**
    - Geographic regions where the clip cannot be distributed.
- **Sample Clearance:**
    - Status of any samples used in the track.
- **Streaming Platforms:**
    - Platforms on which the clip is available for streaming.
- **UPC:**
    - Universal Product Code associated with the album or clip.
- **Revenue Share:**
    - Percentage allocation of revenue to rights holders.
- **RIAA Certification:**
    - Sales certification status (e.g., Gold, Platinum).
- **ISRC:**
    - International Standard Recording Code for unique cataloguing.




## III. Production/Industry Details (Back‑of‑House)

- **Chart Performance:**
    - Indicator of hit status or popularity.
- **Recording Date:**
    - The date when the clip was recorded.
- **Recording Studio:**
    - The studio where the recording took place.
- **Producer:**
    - The production credit.
- **Composer:**
    - Composer details for deeper musical analysis.
- **Lyricist:**
    - Information on the lyricist (secondary to the composer in live contexts).
- **Mix Engineer:**
    - Engineer responsible for mixing.
- **Mastering Engineer:**
    - Engineer responsible for final mastering.
- **Remix Details:**
    - Information if the clip is a remix or alternate version.
- **Outtakes:**
    - Archive data for unused recordings.
- **Bonus Material:**
    - Additional content that might influence clip usage.
- **Derivative Works:**
    - Details about adaptations or remixes derived from the original clip.




## Database Considerations

- **Implementation:**
    - Use a lightweight, file-based database engine such as SQLite for local storage.
    - JSON can be used within the database to handle complex or nested metadata.
- **Data Storage:**
    - Each clip’s metadata is stored in a structured table with columns corresponding to the fields above.
- **Query Performance:**
    - Design indices on frequently queried fields (e.g., Track Title, Artist, BPM) to support rapid lookups during live performance.
- **Extensibility:**
    - The schema is designed to be extensible, allowing for additional fields (such as AI auto-tagging results) to be incorporated as needed.




## Future Expansion

- **Cloud Integration:**
    - Consider integrating with a cloud-based metadata service for collaborative environments or large-scale performances.
- **Enhanced Metadata:**
    - Additional fields can be added for emerging needs, such as detailed cue annotations or real-time performance metrics.
- **Interoperability:**
    - Ensure that the database schema can be easily mapped to other industry standards for metadata, facilitating future integrations.




This manifest provides a clear blueprint for the metadata database that will underpin the soundboard application, ensuring robust support for clip and session management throughout live and broadcast environments.