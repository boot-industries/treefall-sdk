# ORP098 - Orpheus UI Component Library (Future Web Strategy)

**Status:** Planning
**Created:** 2025-10-31
**Context:** Post-shmui removal (PR #129), defining future web UI strategy

## Purpose

Define a purpose-built web UI component library for Orpheus SDK that integrates natively with our TypeScript packages (`@orpheus/client`, `@orpheus/contract`, `@orpheus/react`) and serves actual Orpheus use cases, replacing the removed ElevenLabs shmui fork.

---

## What Made Shmui Useless

The removed `packages/shmui/` had fundamental misalignment:

1. **Wrong domain** - Voice AI conversational components (ElevenLabs)
2. **Zero integration** - Never connected to `@orpheus/client` or core SDK
3. **Wrong stack** - Standalone Next.js app, not a library
4. **Third-party branding** - Contributors/docs tied to ElevenLabs
5. **Bloat** - 316 files, 50k+ lines for components we'll never use

**Backup preserved:** `~/Archives/orpheus-backups/shmui-backup-20251031`

---

## What Would Be Useful: Orpheus-Specific UI

### Core Requirements

1. **Domain-specific components** for professional audio workflows
2. **Native SDK integration** with `@orpheus/client` and `@orpheus/react`
3. **Reusable library** (not a demo app)
4. **Zero third-party branding** - pure Orpheus identity
5. **Minimal footprint** - only what we need

### Target Use Cases

| Use Case               | Components Needed                                          | Integration Point                    |
| ---------------------- | ---------------------------------------------------------- | ------------------------------------ |
| **Session browser**    | SessionList, SessionCard, SessionMetadata                  | `@orpheus/client` SessionGraph       |
| **Transport controls** | PlayButton, TransportBar, TimecodeDisplay                  | `@orpheus/client` Transport          |
| **Clip management**    | ClipTimeline, ClipCard, ClipWaveform                       | `@orpheus/client` Clip API           |
| **Routing matrix**     | RoutingGraph, BusMatrix, AudioIOGrid                       | `@orpheus/client` Routing            |
| **Realtime meters**    | LevelMeter, PeakMeter, SpectrumAnalyzer                    | `@orpheus/client` AudioDriver events |
| **Settings panels**    | AudioDeviceSelector, BufferSizeControl, SampleRateSelector | `@orpheus/client` Config             |

### Architecture

```
packages/orpheus-ui/
├── package.json              # @orpheus/ui
├── src/
│   ├── components/
│   │   ├── session/          # Session browsing/management
│   │   ├── transport/        # Playback controls
│   │   ├── clip/             # Clip editing/display
│   │   ├── routing/          # Audio routing UI
│   │   ├── meters/           # Realtime audio visualization
│   │   └── settings/         # Device/config management
│   ├── hooks/                # useOrpheusSession, useTransport, useClip, etc.
│   ├── contexts/             # OrpheusProvider (wraps @orpheus/react)
│   ├── lib/                  # Utilities, formatters, converters
│   └── index.ts              # Public API exports
├── examples/
│   ├── session-browser/      # Vite demo app
│   ├── clip-editor/          # Vite demo app
│   └── routing-console/      # Vite demo app
└── docs/
    └── components/           # Storybook or MDX component docs
```

**Dependencies:**

```json
{
  "dependencies": {
    "@orpheus/client": "workspace:*",
    "@orpheus/react": "workspace:*",
    "@orpheus/contract": "workspace:*"
  },
  "peerDependencies": {
    "react": "^18.0.0",
    "react-dom": "^18.0.0"
  }
}
```

---

## Component Inventory (from shmui backup - reusable primitives)

**Generic UI primitives to adapt:**

- `button.tsx`, `card.tsx`, `dialog.tsx`, `dropdown-menu.tsx`
- `input.tsx`, `label.tsx`, `select.tsx`, `slider.tsx`, `switch.tsx`
- `tabs.tsx`, `tooltip.tsx`, `separator.tsx`

**Audio-specific to rebuild for Orpheus:**

- `audio-player.tsx` → **OrpheusPlayer** (uses `@orpheus/client` Transport)
- `bar-visualizer.tsx` → **LevelMeter** (uses `@orpheus/client` AudioDriver)
- `waveform.tsx` → **ClipWaveform** (uses `@orpheus/client` Clip API)

**Voice/conversational components to SKIP:**

- ~~conversation.tsx~~, ~~message.tsx~~, ~~response.tsx~~, ~~voice-button.tsx~~ (ElevenLabs-specific)

---

## Implementation Phases

### Phase 1: Foundation (Week 1-2)

- [ ] Create `packages/orpheus-ui/` structure
- [ ] Copy/adapt generic primitives from shmui backup (buttons, cards, inputs)
- [ ] Set up Vite + React dev environment
- [ ] Define component API contracts (props/types)

### Phase 2: Core Audio Components (Week 3-4)

- [ ] **OrpheusProvider** - Context wrapper for `@orpheus/react`
- [ ] **SessionBrowser** - List/search sessions via `@orpheus/client`
- [ ] **TransportBar** - Play/pause/stop with timecode display
- [ ] **ClipCard** - Display clip metadata (name, duration, color)

### Phase 3: Advanced Features (Week 5-6)

- [ ] **ClipTimeline** - Visual timeline with multi-clip arrangement
- [ ] **LevelMeter** - Realtime peak/RMS metering
- [ ] **RoutingMatrix** - Interactive bus/track routing grid
- [ ] **AudioDeviceSelector** - CoreAudio/WASAPI device picker

### Phase 4: Documentation & Examples (Week 7-8)

- [ ] Component documentation (Storybook or MDX)
- [ ] Example app: Session browser
- [ ] Example app: Simple DAW UI
- [ ] Integration guide for web apps

---

## Design Principles

1. **Offline-first** - Components work with local SDK, no cloud dependencies
2. **Deterministic rendering** - UI reflects SDK state accurately (no drift)
3. **Broadcast-safe patterns** - Never block audio thread from UI events
4. **Host-neutral** - Components work in any React environment (Vite, Next.js, Electron)
5. **Typed contracts** - Leverage `@orpheus/contract` for type safety
6. **Lightweight** - Target <500KB bundle (vs shmui's 1.5MB budget)

---

## Technology Stack

**Core:**

- React 18 (already used in `@orpheus/react`)
- TypeScript (strict mode)
- CSS Modules or Tailwind (TBD based on app needs)

**Build:**

- Vite (fast dev server, optimized bundling)
- tsup or Rollup (library build)

**Visualization:**

- Canvas API (waveforms, meters)
- Web Audio Visualizer (spectrum analyzer)
- D3.js or Recharts (routing graphs) - evaluate bundle cost

**Documentation:**

- Storybook (interactive component demos)
- MDX (API reference)

---

## Bundle Budget

| Package              | Max Size | Warn At | Notes                         |
| -------------------- | -------- | ------- | ----------------------------- |
| `@orpheus/ui` (core) | 400 KB   | 350 KB  | All components tree-shakeable |
| `@orpheus/ui` (full) | 800 KB   | 700 KB  | Including visualization deps  |
| Example apps         | 1.5 MB   | 1.2 MB  | With SDK + UI + frameworks    |

---

## Migration from Shmui Backup

**Reuse strategy:**

1. **Copy generic primitives** - Button, Card, Dialog, Input, Select, Slider (adapt styling)
2. **Adapt audio components** - Strip ElevenLabs branding, rewire to `@orpheus/client`
3. **Skip voice components** - Conversation, Message, Voice UI (wrong domain)
4. **Preserve component patterns** - shadcn/ui-style composition, Radix primitives

**File mapping:**

```bash
# Generic primitives (copy + rebrand)
shmui-backup/ui/button.tsx → packages/orpheus-ui/src/components/button.tsx
shmui-backup/ui/card.tsx → packages/orpheus-ui/src/components/card.tsx
shmui-backup/ui/dialog.tsx → packages/orpheus-ui/src/components/dialog.tsx

# Audio components (rebuild + integrate)
shmui-backup/ui/audio-player.tsx → packages/orpheus-ui/src/components/transport/orpheus-player.tsx
shmui-backup/ui/bar-visualizer.tsx → packages/orpheus-ui/src/components/meters/level-meter.tsx
shmui-backup/ui/waveform.tsx → packages/orpheus-ui/src/components/clip/clip-waveform.tsx
```

---

## Success Criteria

- [ ] `@orpheus/ui` package published to npm (or private registry)
- [ ] All components integrate with `@orpheus/client` (no mock data)
- [ ] Example app demonstrates session load → transport control → waveform display
- [ ] Bundle size ≤ 400 KB (core components)
- [ ] Zero ElevenLabs references in codebase
- [ ] Component documentation with live demos

---

## Future Extensions

**After MVP:**

- Plugin UI components (reverb, compressor, EQ visualizers)
- FX parameter automation curves
- MIDI piano roll / event editor
- Collaborative session UI (multi-user indicators)
- Mobile-responsive variants (touch-friendly transport)

**Deployment targets:**

- Vite SPA (standalone web app)
- Next.js (server-rendered docs/marketing)
- Electron (desktop app shell)
- Tauri (native desktop, like Clip Composer web view)

---

## Decision: When to Build This

**Not now** - Clip Composer (OCC) uses JUCE for UI, not web tech. Core SDK and adapters take priority.

**Build when:**

1. We need a web-based session browser/editor
2. Third-party developers request React components for Orpheus
3. We want a marketing/demo site with interactive SDK examples
4. Electron or Tauri apps require cross-platform UI

**Trigger conditions:**

- User request for web UI
- OCC reaches feature parity with JUCE and we explore web tech
- SDK stabilizes (v1.0+) and we focus on ecosystem/tooling

---

## References

[1] Removed shmui package - PR #129: https://github.com/chrislyons/orpheus-sdk/pull/129
[2] shadcn/ui (upstream of ElevenLabs fork): https://ui.shadcn.com/
[3] Radix UI primitives: https://www.radix-ui.com/
[4] `@orpheus/react` package: `/packages/react/`
[5] `@orpheus/client` package: `/packages/client/`

---

## Next Actions

- [ ] Archive this doc until web UI development is triggered
- [ ] Revisit when user requests React component library
- [ ] Evaluate shadcn/ui vs custom components when ready to build
- [ ] Prototype single component (SessionBrowser) as proof-of-concept
