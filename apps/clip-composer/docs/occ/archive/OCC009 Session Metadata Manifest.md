# Session Metadata Database Manifest for Soundboard Application

## Introduction

This document defines the schema for storing session-level metadata in our soundboard application. Each session record encapsulates all configuration, layout, and state information necessary to recreate a live performance environment. This includes not only the order and arrangement of clips but also user interface settings, global performance modes, and file management details.

## I. Session Identification & Basic Details

- **Session ID:**
  - A unique identifier (UUID) for each session.
- **Session Name:**
  - The title or name of the session, as entered by the user.
- **Description:**
  - Optional text to provide a summary or notes about the session.
- **Date Created:**
  - Timestamp for when the session was initially created.
- **Date Modified:**
  - Timestamp for the last update made to the session.
- **User ID (Optional):**
  - Identifier for the user who created or is managing the session (if multi-user support is required).

## II. Layout and Display Configuration

- **Grid Layout Settings:**
  - Number of rows and columns configured for the clip button grid.
  - Active tab count and dual-tab view flag.
- **Clip Button Properties:**
  - Configuration for individual clip button stretching (e.g., a clip can span up to 4 grid cells).
  - Positions and dimensions for custom button layouts.
- **Bottom Panel State:**
  - Indicator of which bottom panel is active (Clip Labelling Panel or Application Preferences Panel) or if none is open.
- **Waveform Editor Settings:**
  - Default zoom level, visible properties, and other editor configuration parameters.

## III. Session Audio & Playback Settings

- **Global Tempo Mode:**
  - Flag indicating whether Global Tempo Mode is enabled.
  - Global BPM value (if enabled).
- **Global Filter Settings:**
  - Default filter values for each Clip Group (if applicable).
- **Audio Routing Configuration:**
  - Current mapping of Clip Groups to output channels.
- **Default Playback Settings:**
  - Overall volume levels and any default audio behavior settings.

## IV. Cue Sequence & Clip References

- **Clip Order List:**
  - An ordered array of clip IDs that defines the sequence in which clips appear in the session.
- **Cue List Configuration:**
  - Details about the active cue list(s), including type (e.g., simple cue list) and order.
- **Clip Position Mapping:**
  - JSON or structured data representing the grid positions and any customized layout for clips.
- **Additional Cue Settings:**
  - Any session-specific cues or settings (e.g., timing offsets) stored as a JSON blob.

## V. Project Media & File Reconciliation

- **Project Media Folder Location:**
  - File path or URI of the designated project media folder.
- **Media Consolidation Flag:**
  - Indicates whether media files have been consolidated into the project folder.
- **File Reconciliation Mappings:**
  - Structured mapping of original file locations to reconciled file paths (if files have been moved or relocated).

## VI. Session Preferences & User Settings

- **User Interface Preferences:**
  - Settings related to the appearance and behavior of the UI (theme, layout preferences, etc.).
- **Custom Key & Shortcut Mappings:**
  - Mappings for keyboard shortcuts and custom key configurations used within the session.
- **Remote Control Settings:**
  - Preferences for remote control behavior (e.g., configuration for the iOS companion app).

## VII. Logs & Session History

- **Session Event Logs:**
  - Records of significant session events (e.g., initialization, state changes) for audit or review purposes.
- **Last Backup Timestamp:**
  - Timestamp indicating the last time the session was backed up or exported.
- **Change History:**
  - Optional versioning or change log details to track modifications over time.

## VIII. Network Configuration

- **Remote Control Connectivity:**
  - **Network Protocols:**
    - Configuration for protocols such as WebSockets, MIDI over IP, and OSC used to interface with the iOS companion app and external devices.
  - **IP Settings:**
    - Static or dynamic IP configuration, including subnet masks, gateways, and DNS settings, to ensure reliable communication between devices.
  - **Port Assignments:**
    - Defined port numbers for remote control commands, log automation, and any other network-based services.
  - **Security Settings:**
    - Options for encryption (e.g., SSL/TLS) and authentication for secure remote control and data transmission.
  - **Log Automation:**
    - Network endpoints or cloud service URLs for auto-syncing logs (e.g., Google Drive, Dropbox, AWS S3).
  - **Connectivity Status:**
    - A status flag or timestamp indicating the last successful network connection, useful for troubleshooting remote control or logging issues.

## Database Considerations

- **Implementation:**
  - A lightweight, file-based database engine such as SQLite is recommended for local storage.
  - JSON can be used within the database to handle complex or nested metadata.
- **Data Storage:**
  - Each session's metadata is stored in a structured table with columns corresponding to the fields above.
- **Query Performance:**
  - Design indices on frequently queried fields (e.g., Session Name, Date Modified) to support rapid lookups during live performance.
- **Extensibility:**
  - The schema is designed to be extensible, allowing for additional fields (such as AI auto-tagging results) to be incorporated as needed.
- **Cloud Integration (Optional):**
  - Consider integrating with a cloud-based metadata service for collaborative environments or large-scale performances.

This manifest provides a comprehensive blueprint for the session metadata database, ensuring that every session’s state—including layout, playback settings, cue sequences, and file management details—is fully captured and easily retrievable. This will allow the soundboard application to restore sessions accurately and support advanced features like intelligent file recovery and dynamic UI configurations.
