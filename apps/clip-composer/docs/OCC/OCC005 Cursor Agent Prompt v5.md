---
source: standard-notes
sn_filename: "OCC005 Cursor Agent Prompt v5-4bc58990.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

# **Project Title: Orpheus Clip Composer**

**Cross-Platform, Low-Latency Professional Soundboard Application**


#### **Objective**: Develop a high-performance, professional-grade soundboard application for use in broadcasting, live performances, theatre productions, and other professional audio workflows. The software must support Windows and macOS, with an iOS companion app for remote control. It should deliver real-time, ultra-low-latency audio playback, advanced DSP processing, multi-channel routing, and intelligent clip management—all built on a modern, scalable, and crash-proof architecture optimized for multi-core processing.


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
- 1. **Example Output Routing:**
2. - **Group A:** Routes audio of all Group A clips to outputs 1–2
- **Group B:** Routes audio of all Group B clips to outputs 3–4
- **Group C:** Routes audio of all Group C clips to outputs 5–6
- **Group D:** Routes audio of all Group D clips to outputs 7–8
- **Editor/Audition Mode:** Routes audio from the editor player to outputs 9–10
3. **Playback Management:**
4. - Each group has an optional togglable FIFO-based “choke” feature. When enabled, triggering a new clip in a group will stop (or fade out) any previous clip within the same group, ensuring clean transitions.
- - **Note:** These routing assignments serve as examples; the system must allow dynamic reconfiguration of routing settings.


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


### **6. Logging, Exporting, Session Management & Intelligent File Recovery**

- **Real-time logging:**
- - Capture timestamped logs for every clip played (recording start, duration, and stop times).
- Log includes clip metadata such as name, category, audio properties, and assigned hotkeys.
- **Automated log export:**
- - Export logs in CSV, JSON, or XML formats.
- Option to auto-sync logs to cloud storage services (Google Drive, Dropbox, AWS S3).
- **Comprehensive session management:**
- - Users can save, load, and duplicate complete session states.
- Sessions include all clips, settings, cue sequences, banks, and configuration data.
- **Intelligent Missing File Search:**
- - When a session is loaded and one or more clip files are missing, the system automatically performs an intelligent search using clip metadata to locate relevant or nearby files.
- If multiple candidate files are found, the user is prompted to confirm the correct file, ensuring seamless recovery of missing assets.


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
11. **Beta Testing & Deployment:**
12. - Conduct extensive stress testing under professional workloads.
- Set up automated CI/CD pipelines using GitHub Actions, Fastlane, and Docker for containerized builds.


---




## **Expected Outcome**

A robust, crash-proof, professional soundboard application delivering real-time, ultra-low-latency audio performance, intelligent clip management, and flexible routing. Key features include:

- An integrated bottom-half waveform editor that supports both touchscreen and mouse controls, with dedicated controls for IN/OUT points, playhead jumps, and cue points.
- Clip Groups (A/B/C/D) that manage both audio output routing and optional FIFO-based playback control.
- A user-configurable grid interface for managing clips across 8 full-screen tabs (with an optional dual-tab view).
- Seamless remote control capabilities via an iOS app.
- Comprehensive logging, session management, and an intelligent missing file search feature that recovers absent clip files using metadata.


---




## **Additional Considerations**

- **Future Expansion:**
- - Integration of a web-based controller (using React + WebSockets) for remote access.
- Implementation of AI-powered auto-tagging to streamline clip organization.
- **Docker Deployment:**
- - Utilize Docker Desktop for containerized builds, ensuring cross-platform consistency and simplified deployment.
- Set up automated pipelines for efficient updates and maintenance.


---



This updated comprehensive prompt now includes the intelligent missing file search feature and the expanded touchscreen support for the waveform editor, ensuring Cursor AI’s Composer Agent has clear and flexible instructions for developing the application. Let me know if any further adjustments are needed!


##### You said:

Augment this section [ 6. Logging, Exporting, Session Management & Intelligent File Recovery Real-time logging: Capture timestamped logs for every clip played (recording start, duration, and stop times). Log includes clip metadata such as name, category, audio properties, and assigned hotkeys. Automated log export: Export logs in CSV, JSON, or XML formats. Option to auto-sync logs to cloud storage services (Google Drive, Dropbox, AWS S3). Comprehensive session management: Users can save, load, and duplicate complete session states. Sessions include all clips, settings, cue sequences, banks, and configuration data. Intelligent Missing File Search: When a session is loaded and one or more clip files are missing, the system automatically performs an intelligent search using clip metadata to locate relevant or nearby files. If multiple candidate files are found, the user is prompted to confirm the correct file, ensuring seamless recovery of missing assets. ] to include a dialogue to select a project media folder for all clips within any newly created project. An existing project should also be able to be pointed to a different folder if requred. Clips should also be able to be reconciled to identical files in different directories if necessary, maintaining all session-relevant clip metadata, DSP, and edit points.


###### ChatGPT said:

Reasoned for a few seconds


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

This augmented section ensures not only robust logging and session management but also flexible, user-friendly media folder selection and clip reconciliation for a seamless editing and playback experience.


##### You said:

Great. Thanks. Now let's scan our horizons a little bit. We've got a good working prompt for Cursor AI's Composer Agent, we have a good start on a set of Cursor "rules," I have set up a GitHub account and installed Cursor desktop. What other services or applications should I be registering with or installing in order to facilitate this build? What will Cursor AI expect me to be prepared for based on what we are asking here? Provide instructions.


###### ChatGPT said:

Reasoned for 8 seconds

Below is a checklist of services, tools, and accounts you should set up to facilitate this build. Cursor AI’s Composer Agent expects you to have a modern, integrated development and deployment environment that spans local development, CI/CD, and cloud services. Here’s what to get ready:

1. **Integrated Development Environment (IDE) & SDKs:**
2. - Install a robust IDE for C++ development (e.g., Visual Studio on Windows, Xcode on macOS, or CLion).
- Set up the JUCE framework, including the Projucer, and ensure you have any necessary licenses.
- Install any additional SDKs needed for DSP libraries like Rubber Band, SoundTouch, or ZTX DSP.
3. **Version Control & Code Hosting:**
4. - You’ve already set up a GitHub account—ensure your repositories are well-organized and that you follow Git best practices.
5. **Continuous Integration/Deployment (CI/CD):**
6. - Configure GitHub Actions for automated builds, testing, and deployments.
- Install and set up Fastlane if you plan to deploy your iOS companion app. (Note: This will require an active Apple Developer account.)
7. **Containerization & Deployment:**
8. - Ensure Docker Desktop is installed (as you’ve done) and register for a Docker Hub account if you plan to host container images.
- Prepare Dockerfiles for cross-platform builds and test containerized builds locally.
9. **Cloud Storage & Logging:**
10. - If you intend to auto-sync logs or session data, sign up for a cloud storage service such as AWS (for S3), Google Drive, or Dropbox.
- Set up the necessary API keys and buckets/folders to integrate automated log export into your pipeline.
11. **Remote Control & MIDI/OSC Tools:**
12. - Verify that you have the necessary drivers or tools for MIDI/OSC support.
- If using external hardware (e.g., MIDI controllers), ensure they’re properly configured in your development environment.
13. **Error Reporting & Project Management (Optional):**
14. - Consider integrating error monitoring services like Sentry for real-time diagnostics.
- Use project management tools such as Jira, Trello, or Asana to track tasks, bugs, and feature progress.
15. **Documentation & Backup:**
16. - Set up a system for maintaining comprehensive project documentation (e.g., a dedicated Wiki or README files).
- Implement a backup strategy for your code and configuration files.
17. **Future AI Integration (Optional):**
18. - If planning future AI-powered features (like auto-tagging), register with relevant platforms (e.g., OpenAI) and review their integration guides.

By setting up these services and tools, you’ll be well-prepared to develop, test, and deploy the professional soundboard application as described in our comprehensive prompt. Cursor AI’s Composer Agent will rely on this modern ecosystem to generate code and system designs that integrate smoothly with your development pipeline.


##### You said:

What would you suggest for documentation workflows? I like the idea of maintaining documentation as part of the build to not only manage the Cursor AI Composer Agent but also to help my own visualization of the project overview. I am a Notion subscriber and I am aware that their wiki tool is quite powerful.... what tools or best practices would you suggest?


###### ChatGPT said:

Reasoned for a couple of seconds

For a project with the scope and complexity we’re discussing, having a robust, integrated documentation workflow is essential. Here are some best practices and tool suggestions:

1. **Centralized Documentation Repository:**
2. - **Notion as the Central Hub:**
Use Notion as your main project wiki. Notion’s flexibility lets you organize pages for everything—from high-level project overviews, technical specifications, and meeting notes to detailed API documentation.
- - **Linking and Embedding:** Embed code snippets, diagrams, and even GitHub issues or commits directly into Notion pages for a live view of project progress.
- **Templates:** Create reusable templates for new features, bug reports, or meeting summaries so that documentation is consistent and thorough.
3. **Docs-as-Code:**
4. - **Version-Controlled Documentation:**
Maintain Markdown-based documentation within your GitHub repository (or a dedicated docs repository). Tools like Docusaurus or MkDocs can automatically generate a static website from these files.
- - **Automated Updates:** Integrate your documentation build into your CI/CD pipeline. This way, changes to documentation trigger automatic rebuilds, ensuring that your docs are always in sync with the latest code.
- **Consistency:** Keeping documentation close to the code helps ensure that documentation evolves with your application, reducing the “bit rot” of manual documentation.
5. **Integration Between Notion and GitHub:**
6. - **Sync and Linking:**
While Notion is excellent for a high-level overview, consider periodically syncing key documents or summaries to your GitHub wiki. Tools like Notion2GitHub or custom scripts can help automate this.
- - **Dual Documentation Strategy:** Use Notion for planning, brainstorming, and executive overviews, and maintain detailed technical documentation in your repository for version control.
7. **Collaboration and Accessibility:**
8. - **Real-Time Collaboration:**
Leverage Notion’s real-time collaboration features for team meetings, brainstorming sessions, and quick updates. Use comment threads and inline discussions to keep feedback in context.
- **Searchability and Tagging:**
Organize your Notion pages with tags and linked databases (for instance, a project task tracker or bug database) to make it easy to find relevant documentation quickly.
9. **Automated Documentation Generation:**
10. - **Code Documentation:**
Use tools like Doxygen, Sphinx, or Javadoc (depending on your codebase) to auto-generate documentation from code comments. This can be integrated into your CI/CD process, with links to these docs available from your central Notion hub.
- **API Documentation:**
If your project exposes APIs (or even internal ones for integration), consider using Swagger or Postman’s documentation features to keep API docs up-to-date.
11. **Visual Documentation and Diagrams:**
12. - **Diagram Tools:**
Incorporate tools like Lucidchart, Draw.io, or Notion’s built-in drawing tools to create flowcharts, architecture diagrams, and wireframes. Embed these diagrams in your Notion pages and Markdown docs to visually represent system components and workflows.
- **Versioned Diagrams:**
Keep diagrams versioned along with your code changes so that you can track the evolution of your system architecture over time.
13. **Documentation Culture and Maintenance:**
14. - **Regular Reviews:**
Schedule periodic documentation reviews as part of your sprint or release cycle to ensure that documentation stays relevant and accurate.
- **Documentation as a Deliverable:**
Treat documentation updates as a required part of feature development or bug fixes. This means that pull requests should include updates to the relevant documentation sections, whether in Notion or your GitHub repo.

By combining Notion’s powerful wiki features for high-level and collaborative documentation with a docs-as-code approach for technical and API documentation, you can maintain a comprehensive, integrated, and easily accessible documentation workflow that evolves alongside your project. This dual strategy not only helps manage the Cursor AI Composer Agent but also provides you with a clear visualization of the overall project architecture and progress.
