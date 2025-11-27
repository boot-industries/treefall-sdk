# ORP119: Shmui Integration & Augmentation Strategy

**Status:** Draft
**Package:** `packages/shmui-juce`
**Version:** 1.0.0
**Target App:** Clip Composer (OCC)

## 1. Executive Summary

The `Shmui` package currently serves as a high-fidelity **visualization library**, providing ports of ElevenLabs' React components (`Orb`, `Waveform`, `Bar`, `Matrix`) to JUCE. While visually impressive, it fundamentally lacks the **interactive controls** (buttons, knobs, sliders) required to build a functional professional audio application like Clip Composer.

This document outlines the strategy to transform `Shmui` from a visualizer collection into a comprehensive **UI Kit**, ensuring visual consistency across the application while meeting the performance demands of the 960-button Clip Grid.

---

## 2. Component Fit Analysis

We assessed the existing `Shmui` components against Clip Composer's key requirements:

| Clip Composer Feature | Shmui Component | Fit | Action Required |
| :--- | :--- | :--- | :--- |
| **Clip Edit Dialog** | `AudioScrubberVisualizer` | ⭐ **High** | **Adopt immediately.** Supports scrubbing, seeking, and playback visualization. Fits the timeline editing workflow perfectly. |
| **Clip Grid Thumbnails** | `WaveformVisualizer` | ⚠️ **Medium** | **Optimization needed.** Rendering 960 individual `WaveformVisualizer` components will degrade performance. Needs a virtualization strategy (see Sec 4). |
| **Mixer Meters** | `MatrixDisplay` | ✅ **Good** | Use `createVUMeterFrame` helper. Provides a stylized "LED" aesthetic consistent with the hardware-inspired look. |
| **Master Output** | `BarVisualizer` | ⚠️ **Partial** | Good for frequency analysis, but less precise than standard meters. Use as a secondary "spectral" display. |
| **Idle State** | `OrbVisualizer` | ❌ **Low** | Too resource-intensive (OpenGL) for general use. Reserve for specific "AI Processing" or "Wait" states. |

---

## 3. Critical Gaps: The "Controls" Module

The application needs standard inputs styled to match the Shmui aesthetic. We will create a new module `packages/shmui-juce/Controls` containing:

### 3.1. `ShmuiKnob`
*   **Usage:** Gain, Pan, Trim adjustments.
*   **Style:** Minimalist vector knob using `ColorUtils` palette.
*   **Features:** Value display tooltip, fine-control mode (Shift+Drag).

### 3.2. `ShmuiFader`
*   **Usage:** Mixer volume faders.
*   **Style:** Flat, modern fader cap with integrated level metering background (optional).

### 3.3. `ShmuiButton` & `ShmuiToggle`
*   **Usage:** Mute, Solo, Loop, Transport controls.
*   **Style:** Glow effects on active state (using `MatrixDisplay` color logic), smooth transitions using `Utils/Interpolation`.

---

## 4. The Grid Challenge: 960 Interactive Buttons

Instantiating 960 separate `juce::Component` instances (one per clip) is a known performance bottleneck in JUCE, especially if each contains a complex `WaveformVisualizer`.

### Solution: `ShmuiVirtualGrid`
We will implement a monolithic component that renders the entire grid:

1.  **Single Paint Pass:** The grid iterates visible cells and paints them directly to the `Graphics` context.
2.  **Flyweight Pattern:** Clip data is stored in a lightweight struct, not in heavyweight Components.
3.  **Cached Waveforms:** Waveform paths are generated once and cached as `juce::Path` or `juce::Image` objects.
4.  **Virtual Input:** `mouseDown/Up` events are trapped by the Grid and mapped to logical cell coordinates (row/col).

---

## 5. Implementation Plan

### Phase 1: Integration (Immediate)
*   [ ] Update `AudioEngine` to expose `AudioMeter` data for `MatrixDisplay` (VU Meters).
*   [ ] Replace existing debug scrubber in Edit Dialog with `AudioScrubberVisualizer`.

### Phase 2: Augmentation (Controls)
*   [ ] Create `packages/shmui-juce/Controls`.
*   [ ] Implement `ShmuiButton` (the simplest primitive).
*   [ ] Implement `ShmuiKnob` (using `ColorUtils` for consistency).

### Phase 3: The Grid (Performance)
*   [ ] Prototype `ShmuiVirtualGrid` with simple rectangle rendering.
*   [ ] Integrate `WaveformVisualizer` drawing logic into the Grid's paint loop (using cached paths).

---

## 6. Style Consistency

All new controls MUST use `shmui::ColorUtils` to ensure theming support:
*   Use `fromHex` for defining palette constants.
*   Use `colorRamp` for value indicators (e.g., knob ring color changing with value).
*   Respect `Style` structs for defining dimensions and rounding.

