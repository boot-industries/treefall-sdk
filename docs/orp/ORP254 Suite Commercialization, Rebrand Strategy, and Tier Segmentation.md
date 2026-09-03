# ORP254 Suite Commercialization, Rebrand Strategy, and Tier Segmentation

**Status:** Approved Strategy
**Date:** 2026-09-03
**Scope:** Suite-wide commercialization, rebrand decoupling, packaging gates, and product tier boundaries
**Repos Affected:** `orpheus-sdk`, `fourtrack`, `clip-composer`, `freqfinder`, `shmui`

---

## 1. Executive Summary & Brand Architecture

The Orpheus Suite is transitioning from internal research and development into a commercial product family published by **Boot Industries** (colloquially **boot**).

To eliminate rebranding paralysis and prevent artificial coupling between the foundational audio engine and consumer-facing applications, the branding hierarchy is decoupled into four distinct tiers:

1. **Company / Publisher:** `Boot Industries` (corporate umbrella, store publisher entity, commercial support).
2. **Audio Engine / Infrastructure (Open Core / OSS):** `Treefall SDK` (rebranded from `Orpheus SDK` under permissive MIT license).
3. **Design System & UI Component Library:** `ShmUI` (internal design-token contract and JUCE component library).
4. **Independent Consumer Applications (Proprietary / Closed Source):**
   - **`TR-40` & `TR-80`**: Hardware-referenced portable multitrack tape consoles.
   - **`Clip Composer`**: Professional live broadcast and theatrical soundboard.
   - **`FreqFinder`**: Precision surgical audio inspection and spectral analysis plugin.

---

## 2. Open Core vs. Proprietary Model

### 2.1 Strategy: Open Source Engine, Closed Source Applications
- **Treefall SDK (formerly Orpheus SDK): Open Source (MIT)**
  - Retains public trust, deterministic realtime audits, and external academic/developer credibility.
  - Acts as a verifiable reference implementation for C++20 realtime audio, lock-free concurrency, and deterministic session modeling.
  - Does not cannibalize application value: commercial users buy the curated UX, hardware-modeled workflows, and platform shells, not raw C++ header libraries.
- **Applications (TR-40, TR-80, Clip Composer, FreqFinder): Closed Source (Commercial)**
  - All rights reserved on application logic, workflow design, view models, and presentation assets.
  - Direct sales, upfront purchase, and seat licenses. Zero mandatory subscriptions.

### 2.2 Third-Party Licensing Guardrails
- **JUCE Framework (Clip Composer, FreqFinder):** Commercial distribution requires paid JUCE licenses (Indie or Pro tier). Must not rely on JUCE GPL terms, which would force application open-sourcing.
- **Rubber Band Library:** Time-stretching integration in Clip Composer remains deferred to v1.0+; commercial integration requires commercial licensing to avoid AGPL contagion.
- **libsndfile:** Bound dynamically or audited strictly for LGPL 2.1 compliance.
- **SIL OFL Fonts (HK Grotesk, Space Grotesk, JetBrains Mono):** Fonts may be bundled within binary distributions; standalone sale of font files is prohibited. Legal attribution notices must be included in the application About/Attribution panels.

---

## 3. Product Tiering: Hardware-Scoped Boundaries (TR-40 vs. TR-80)

### 3.1 Hard-Scoped Platform Division
The previously contemplated in-app purchase / software unlock model ("buy FourTrack, upgrade to EightTrack via IAP within the same frame") is superseded. Instead, the products are hard-scoped by target form factor, physical surface area, and platform modality:

| Dimension | **TR-40** | **TR-80** |
|:---|:---|:---|
| **Target Platforms** | **iOS / Android** (Mobile / Pocket) | **iPadOS / macOS / Windows / Linux** (Desktop / Tablet) |
| **Track Count** | Exactly 4 Tracks | Up to 8 Tracks |
| **Bounce Architecture** | 4-to-1 Mono Bounce (Portastudio classic discipline) | 8-to-1 Mono and Stereo Bounce |
| **UI Surface Model** | Single-handed vertical thumb flow, compact meter field, pocket transport lock | Full 90-point Console grid, tactile horizontal fader bank, dedicated tape-aperture view |
| **Workflow Ethos** | By-ear pocket sketchpad; friction-free capture anywhere | Full session tracking, acoustic layering, stereo mastering stem bounce |
| **Monetization** | Upfront paid mobile app ($9.99–$14.99) | Standalone desktop/tablet workstation purchase ($29.99–$49.99) |

### 3.2 Strategic Advantages of Hard-Scoping
1. **Zero UI Compromise:** The mobile interface does not need to awkwardly accommodate 8 faders, horizontal panning clutter, or collapsible track banks.
2. **Platform Native Muscle Memory:** Mobile operators get a focused 4-track tape recorder optimized for touch and pocket use; desktop and iPad operators get a full console surface.
3. **App Store Simplicity:** Eliminates convoluted StoreKit receipt validation, restore-purchase state machines, and tier-gating logic in the audio core. A session bundle (`.trk`) remains cross-compatible: a TR-40 session opens seamlessly in TR-80 (populating tracks 1–4).

---

## 4. Rebrand Execution Policy (ORP177 Conformance)

Following `ORP177`, renaming the SDK from `Orpheus` to `Treefall` must follow a staged, non-breaking cutover:

1. **Stage 1: Repository & Display Rebrand (Immediate)**
   - Rename remote GitHub repository to `chrislyons/treefall-sdk`.
   - Update remote URLs in parent repos' `.gitmodules`.
   - Retain local submodule checkout path `third_party/orpheus-sdk` to prevent build churn.
   - Update READMEs, About panels, and marketing copy.
2. **Stage 2: Package Name Migration (Minor Milestone)**
   - Export CMake package `TreefallSDK` while retaining compatibility alias `OrpheusSDK`.
   - Export targets `Treefall::core`, etc., aliasing `Orpheus::core`.
3. **Stage 3: Full API & ABI Cutover (Major Version Milestone / v1.0)**
   - Cutover include directory (`include/treefall/`), C++ namespace (`namespace treefall`), and C ABI prefix (`treefall_*`).
   - Bump C ABI to v2.0.

---

## 5. Road to Market: 4-Gate Release Process

Each application must satisfy five sequential, observable verification gates before public release:

```
Gate 0: Feature Freeze ──► Gate 1: Deterministic QA ──► Gate 2: Air-Gapped Packaging ──► Gate 3: Docs & Legal ──► Gate 4: Operator Acceptance
```

1. **Gate 0: Feature Freeze & Zero-Stub Audit**
   - All P0 workflows complete and functional.
   - Zero UI placeholders, disabled non-functional buttons, or mock alert dialogs.
2. **Gate 1: Deterministic Realtime & Sanitizer Certification**
   - Full CTest suite clean under Debug (ASan + UBSan) and Release.
   - Static realtime audit (`realtime_static_audit`) clean; zero memory allocations, locks, or file I/O on the audio thread.
   - 1-hour continuous playback/record soak test with zero memory growth and zero audio dropouts.
3. **Gate 2: Cross-Platform Packaging & Air-Gapped Verification**
   - **macOS:** Signed with Developer ID Application certificate, hardened runtime enabled, audio/mic entitlements declared, notarized via Apple Notary Service (`notarytool`), distributed in a signed `.dmg`.
   - **Verification:** Installed on a clean, secondary machine (Whitebox) with no developer SDKs or Xcode tools. Verifies cold-start launch, CoreAudio HAL handshake, mic TCC prompt, and physical output routing.
4. **Gate 3: Documentation & Legal Parity**
   - Starlight user manual matches current UI menus, shortcut tables, and behavioral invariants.
   - Root `LICENSE` file committed.
   - Third-party dependency attribution panel populated (JUCE, SIL OFL fonts, libsndfile, nlohmann/json).
5. **Gate 4: Real-World Operator Acceptance**
   - **TR-40 / TR-80:** Complete tracking and bounce of a multi-take session without data loss.
   - **Clip Composer:** Full theatrical/broadcast cue list auditioned and fired under stress conditions.
   - **FreqFinder:** Real-time spectral analysis verified in DAWs (Logic Pro, REAPER) via AU and VST3 formats.

---

## 6. Marketing & Web Presence Architecture

Standardize all web properties on the lightweight, offline-first stack established in `apps/fourtrack-web` (Astro + Starlight on Cloudflare Pages):

1. **Corporate / Umbrella (`boot.industries`):**
   - Positioning: "Software and instruments built for operators."
   - Directory linking to TR-40, TR-80, Clip Composer, FreqFinder, and Treefall SDK.
2. **SDK Engineering Hub (`treefall.audio` or GitHub Pages):**
   - Technical documentation, C++ API references, architecture diagrams, realtime benchmarks, and contribution guidelines.
3. **Product Marketing & User Manual Sites:**
   - **TR-40 / TR-80:** Hand-authored hardware showcase, sound samples, and Starlight user manual.
   - **Clip Composer:** Broadcast feature tour, latency metrics, and operator cue sheet guide.
   - **FreqFinder:** Plugin format matrix, feature rundown, and technical manual.

---

## 7. Sequencing & Launch Waves

1. **Wave 1: TR-80 (macOS) & TR-40 (iOS)**
   - Leading the commercial launch. Headless C++20 core and SwiftUI Console shell are already mature (FTR071/FTR096).
2. **Wave 2: FreqFinder (AU/VST3 & Standalone)**
   - Compact utility footprint; straightforward package qualification and DAW validation.
3. **Wave 3: Clip Composer (macOS / Windows)**
   - Full theatrical soundboard release following JUCE commercial licensing qualification.
4. **Wave 4: Treefall SDK 1.0 Public Release**
   - Formal open-source release with versioned package tarballs and stabilized C ABI.
