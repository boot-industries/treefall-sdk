---
source: standard-notes
sn_filename: "OCC010 Wireframes v1-5b21ab1c.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

Below are several wireframe illustrations (presented as annotated sketches) that describe the key UI components and interactions of our soundboard application. These wireframes help visualize the layout and user flow:


---




### Wireframe 1: Main Screen Overview


```


















```

**Description:**

- The top area displays the Global Tempo (when enabled) and indicators for each Clip Group.
- Below, full-screen tab buttons allow the user to switch between up to 8 pages (each containing a 10×12 grid).
- The main grid area shows clip buttons; these buttons can be standard or can “stretch” to occupy up to 4 cells.
- The grid remains visible at all times.


---




### Wireframe 2: Waveform Editor (Bottom-Half Panel)


```














```

**Description:**

- The waveform editor panel slides up from the bottom of the screen without covering the main grid completely.
- It includes its own audio player with basic transport controls.
- The display shows the audio waveform with visual markers (IN/OUT points, cue points) set via mouse or touchscreen.


---




### Wireframe 3: Persistent Bottom Panels


```





















```

**Description:**

- Two types of bottom panels are available:
- - The **Clip Labelling Panel** (with two tabs for label/appearance and metadata editing, supporting schema-specific copy/paste).
- The **Application Preferences Panel** for general settings and persistent playback controls.
- Only one bottom panel is open at a time, ensuring the main grid remains in view.
- The panels are persistent (non-pop-up) and docked at the bottom.


---



These wireframes provide a high-level visual guide to our interface, illustrating how the main grid, waveform editor, and bottom panels work together to create an efficient and customizable performance environment. They serve as a starting point for further UI design and refinement. Let me know if you need additional details or adjustments!


##### You said:

Show a wireframe for dual-tab view.


###### ChatGPT said:

Reasoned for a few seconds

Below is a wireframe illustration for the dual-tab view, showing two grid pages displayed side by side while maintaining visibility of the persistent bottom panel (if active):


```



















```

**Description:**

- The top area shows the Global Tempo and tab navigation; in dual-tab view, two tabs are active/highlighted.
- The main area is split into two panels—each showing a 10×12 grid for clip buttons (with flexible stretching).
- The persistent bottom panel remains visible at the bottom, ensuring continuous access to clip labelling or preferences without covering the main grid.
