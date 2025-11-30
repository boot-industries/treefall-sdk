# OCC126 Backend Master Plan: Architecture & Roadmap

**Version:** 2.0 (Consolidated)
**Date:** 2025-11-27
**Status:** Approved Master Plan
**References:** OCC114-OCC125
**Supersedes:** OCC126X

---

## 1. Executive Summary

This document consolidates the architectural audit (OCC126 v1) and the feature roadmap (OCC126X) into a single execution strategy for Clip Composer's backend.

We are transitioning Clip Composer from a frontend prototype to a professional broadcast system. This requires implementing **100+ features** while maintaining strict reliability standards.

**Key Strategic Decisions:**
1.  **Architecture First:** We will insert a **Sprint 0** to build shared backend services (Dependency Injection, SQLite, Paths) prevents "spaghetti code" as complexity grows.
2.  **Data Safety is P0:** Auto-backup, crash recovery, and event logging are the immediate priorities.
3.  **Modernization:** We are formally deprecating legacy SpotOn features (DTMF, CD Burning, PBus) in favor of modern standards (OSC, REST, JSON/SQLite).

---

## 2. Architectural Strategy (The "How")

Before building individual features, we must establish the application scaffolding to support them.

### 2.1 Sprint 0: Backend Foundation (1 Week)
**Goal:** Establish the shared infrastructure to support all subsequent sprints.

*   **Service Context Pattern:** Implement a Dependency Injection container (`ServiceContext`) to manage the lifecycle of 15+ new managers (`SessionManager`, `AudioEngine`, `EventLogger`, etc.) without passing pointers manually.
*   **Standardized Paths:** Implement `ApplicationPaths` to manage `~/.local/share/orpheus-clip-composer/` structure for sessions, logs, and backups.
*   **Persistence Layer:**
    *   **SQLite:** Initialize the engine for high-volume data (Logs, Search Index).
    *   **JSON:** Finalize the v1.0.0 schema for Sessions and Preferences.
*   **Universal Command Pattern:** Define the `Command` interface base class to support Undo/Redo across all application states (not just clip editing).

### 2.2 Threading & Reliability Policy
*   **Audio Thread:** Zero allocations, zero locks, zero I/O.
*   **Message Thread:** UI updates, command dispatch.
*   **Background Threads:** All file I/O (Auto-save, Logging) must occur here to prevent UI freezes.

---

## 3. Implementation Roadmap (The "When")

This roadmap integrates the detailed feature breakdown from the analysis into a sequential execution plan.

### Phase 1: Critical Data Safety (P0)
**Focus:** Prevent data loss and enable professional reliability.

| Sprint | Focus | Key Deliverables | Effort |
| :--- | :--- | :--- | :--- |
| **Sprint 0** | **Foundation** | ServiceContext, SQLite setup, Folder structure. | 1 wk |
| **Sprint 1** | **Data Safety** | **Auto-Backup:** Background thread, rotation (10 files).<br>**Crash Recovery:** Dirty shutdown detection, restore prompt.<br>**Templates:** "New from Template" workflow. | 2-3 wks |
| **Sprint 2** | **Observability** | **Event Logging:** Structured logging to SQLite.<br>**Status Logs:** Session state dump on exit.<br>**Log Viewer:** Basic UI for support diagnostics. | 1-2 wks |

### Phase 2: Professional Workflow (P1)
**Focus:** Enable complex session editing and management.

| Sprint | Focus | Key Deliverables | Effort |
| :--- | :--- | :--- | :--- |
| **Sprint 3** | **Visuals** | **Level Meters:** Real-time VU/PPM on buttons (Audio Thread safe).<br>**Clip Visuals:** Enhanced state indicators. | 3-4 wks |
| **Sprint 4** | **Editing** | **Undo/Redo:** Full command history for clip/tab edits.<br>**Paste Special:** Selective attribute copying (Metadata, Gain, etc.). | 2-3 wks |
| **Sprint 5** | **Session Mgmt** | **Missing File Resolution:** "Locate File" wizard with search paths.<br>**Recent Files (MRU):** Track last 10 sessions.<br>**File Timestamp Validation:** Detect external modifications. | 4 wks |

### Phase 3: Advanced Capabilities (P2)
**Focus:** Match and exceed SpotOn's advanced features.

| Sprint | Focus | Key Deliverables | Effort |
| :--- | :--- | :--- | :--- |
| **Sprint 6** | **Search** | **Advanced Search:** Filter by metadata, color, duration.<br>**Metadata Parsing:** Extract BWF/RIFF data from WAV files. | 1-2 wks |
| **Sprint 7** | **Automation** | **Clip Chains:** Modern replacement for Master/Slave links.<br>**Actions:** Play Next, Stop Other, Set Gain triggers. | 3-4 wks |
| **Sprint 8** | **External** | **HotKeys:** Global/Paged scope configuration.<br>**External Tools:** "Edit in Audition/Reaper" integration. | 2 wks |
| **Sprint 9** | **Hardware** | **GPI/GPO:** Hardware trigger inputs and tally outputs.<br>**Integration:** Support for legacy broadcast GPIO interfaces. | 2 wks |

---

## 4. Deprecation & Modernization Strategy

To maximize efficiency, we are strictly strictly deprecating legacy features that do not fit the modern ecosystem.

| Feature | Status | Modern Alternative | Saved Effort |
| :--- | :--- | :--- | :--- |
| **CD Burning** | ❌ Deprecated | OS-native burning / Cloud sharing | ~40 hrs |
| **DTMF Decoder** | ❌ Deprecated | SIP / WebRTC integration | ~40 hrs |
| **PBus Protocol** | ❌ Deprecated | REST API / OSC | ~20 hrs |
| **Master/Slave** | 🔄 Modernized | **Clip Chains** (Simplified Action List) | High Value |
| **Play Groups** | 🔄 Modernized | **Smart Groups** (User-defined tags) | High Value |

---

## 5. Risk Assessment

*   **Complexity Risk:** The interaction between `UndoManager`, `SessionManager`, and `AudioEngine` is high.
    *   *Mitigation:* Strict adherence to the `ServiceContext` pattern and Unit Tests for all `Command` classes.
*   **Performance Risk:** Level metering and logging could impact audio performance.
    *   *Mitigation:* Lock-free circular buffers for meters; asynchronous background queues for SQLite logging.

## 6. Next Steps

1.  **Approve Sprint 0:** Begin implementation of the architectural foundation immediately.
2.  **Update Project Board:** Create tickets for Sprint 0 tasks (`ServiceContext`, `SQLite`, `AppPaths`).
3.  **Archive Old Plans:** Delete `OCC126X` to avoid confusion.

---
**Document Status:** ✅ Final
**Maintainer:** OCC Development Team
