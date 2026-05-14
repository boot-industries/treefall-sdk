---
source: standard-notes
sn_filename: "OCC011 Wireframes v2-37e59874.txt"
prefix: occ
original_format: lexical
imported: 2026-05-01
status: archive
---

In this design, the main screen can be displayed in Single Tab View or Dual Tab View, and a single bottom panel region is togglable between four modes—Waveform Editor, Clip Labelling, Routing, and Preferences—ensuring only one is visible at any time.


---



**1. Main Screen – Single Tab View**


```



















```

**Description:**

- The top bar displays clip group indicators and the Global Tempo.
- Full-screen tab buttons (up to 8) allow switching between grid pages.
- The main grid shows clip buttons arranged in a 10×12 layout, with some buttons able to stretch to occupy up to 4 cells.
- At the bottom, a togglable panel offers four modes—Labelling, Editor, Routing, or Preferences—with only one panel visible at any time.


---



**2. Main Screen – Dual Tab View**


```



















```

**Description:**

- In dual-tab view, the main screen shows two grid panels side by side, with two tabs active.
- Each panel displays a 10×12 grid (with some buttons stretching up to 4 cells).
- The togglable bottom panel (offering Labelling, Editor, Routing, or Preferences) remains available below, with only one panel visible at any time.






---


---


---


---


---

Below are the updated unified wireframes for the bottom panel. Now, there are four modes available (ordered as: Labelling, Editor, Routing, Preferences), with only one visible at any time.


---



**1. Unified Bottom Panel – Active Mode: Labelling**


```

































```

**Description:**
This view shows the Labelling mode active. The bottom panel has two tabs (Label & Appearance and Metadata) for editing clip appearance and metadata. Only the Labelling panel is visible, leaving the main grid fully accessible.


---



**2. Unified Bottom Panel – Active Mode: Editor**


```































```

**Description:**
In Editor mode, the bottom panel shows a waveform editor with transport controls. It now explicitly includes a Loop toggle, DSP controls, and IN/OUT time entry fields for precise editing. Only the Editor panel is visible.


---



**3. Unified Bottom Panel – Active Mode: Routing**


```

































```

**Description:**
Routing mode provides a routing matrix within the bottom panel. Two tabs ("Outputs" and "Inputs") allow the user to configure routing between the application endpoints and available audio hardware. The default view is Outputs. Only the Routing panel is visible in this mode.


---



**4. Unified Bottom Panel – Active Mode: Preferences**


```






























```

**Description:**
In Preferences mode, the bottom panel displays application-wide settings. This includes general preferences, persistent playback controls, and remote control configurations. Only the Preferences panel is visible in this mode.


---



These four wireframes illustrate the unified bottom panel with four distinct toggleable modes—Labelling, Editor, Routing, and Preferences—with only one panel visible at a time. Each mode provides specific controls while maintaining the main grid view above. Let me know if you require any further adjustments!
