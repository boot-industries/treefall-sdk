# OCC120 Options Menu Backend Features - Sprint Plan

**Status:** Draft
**Created:** 2025-11-12
**Sprint:** Backend Architecture Analysis (Part of OCC114)
**Dependencies:** OCC114 (Backend Audit), OCC115-OCC119 (All previous menus)

---

## Executive Summary

Analysis of SpotOn Manual Section 07 (Options Menu) identifying backend requirements for application preferences, feature toggles, and global settings management.

**Complexity:** Low-Medium (primarily configuration management)
**Backend Components:** Settings/Preferences system
**Database Requirements:** Low (key-value settings storage)
**API Endpoints:** ~15-20 endpoints

---

## 1. Feature Overview

The Options Menu provides application-level settings and feature toggles:
- UI preferences (hints, fonts, button images)
- Feature enable/disable flags (GPI, MIDI, hotkeys, Master/Slave links, etc.)
- Display options (session startup, button highlighting, track history)
- Input settings (mouse button mapping, play-click debounce)
- Global defaults (fade times, gain settings, auto-trim)
- Advanced features (Step to Next, SMPTE, DTMF)

**Key Insight:** This menu is primarily about preference storage and feature flag management rather than complex business logic.

---

## 2. Backend Architecture Requirements

### 2.1 Settings Management System

**Core Requirements:**
- Hierarchical settings organization
- Type-safe value storage
- Default value management
- Settings validation
- Change notification
- Import/export capabilities

**Implementation:**
```typescript
interface SettingsCategory {
  display: DisplaySettings;
  input: InputSettings;
  audio: AudioSettings;
  features: FeatureFlags;
  midi: MidiSettings;
  advanced: AdvancedSettings;
}

interface DisplaySettings {
  popupHintsEnabled: boolean;
  startMaximized: boolean;
  startupMode: 'blank' | 'previous' | 'template';
  autoShowSessionNotes: boolean;
  bringToFrontWithScrollLock: boolean;
  individualButtonFonts: boolean;
  useFriendlyOutputNames: boolean;
  buttonImagesEnabled: boolean;
  disableButtonText: boolean;
  highlightButtonsPlaying: boolean;
  autoLoadButtonImages: boolean;
  showTracksPlayed: boolean;
}

interface InputSettings {
  mouseLeftFunction: 'play_toggle' | 'play';
  mouseRightFunction: 'menu' | 'stop';
  mouseCenterFunction: 'pause' | 'skip_playlist' | 'menu';
  playClickDebounce: number;          // 0, 50, 100, 150, 200, 250 ms
  quickSelectEnabled: boolean;
}

interface AudioSettings {
  gainOffset: 0 | -10;                // dB
  fadeDepth: 20 | 30 | 40 | 50 | 60;  // dB
  autoTrimThreshold: number;           // -20 to -50 dBFS
  autoTrimMargin: number;              // 0, 20, 40, 60, 80 ms
  defaultFadeOutTime: number;          // 0, 0.1-1.0, or custom seconds
}

interface FeatureFlags {
  advancedEditingEnabled: boolean;
  searchMenuEnabled: boolean;
  gpiEnabled: boolean;
  hotKeysEnabled: boolean;
  pbusControlEnabled: boolean;
  dtmfTriggerEnabled: boolean;
  masterSlaveLinksEnabled: boolean;
  smpteTimecodeEnabled: boolean;
  stepToNextEnabled: boolean;
}

interface MidiSettings {
  midiInEnabled: boolean;
  midiOutEnabled: boolean;
  actAsNetworkMaster: boolean;
  loopbackOutToIn: boolean;
  enableFastMidiMute: boolean;
  useVelocityAsInitialGain: boolean;
  convertVelocityZeroToNoteOff: boolean;
}

interface AdvancedSettings {
  stepToNextSkipMuted: boolean;
  stepToNextSkipPlayNext: boolean;
  highlightStepToNext: boolean;
  menuIconsEnabled: boolean;
  groupOutputSelectionPopup: boolean;
  smpteUseTriggerList: boolean;
}
```

### 2.2 Database Schema

```sql
CREATE TABLE user_settings (
  id INTEGER PRIMARY KEY DEFAULT 1,
  -- Display settings
  popup_hints_enabled BOOLEAN DEFAULT TRUE,
  start_maximized BOOLEAN DEFAULT FALSE,
  startup_mode TEXT DEFAULT 'previous' CHECK(startup_mode IN ('blank', 'previous', 'template')),
  auto_show_session_notes BOOLEAN DEFAULT FALSE,
  bring_to_front_scroll_lock BOOLEAN DEFAULT FALSE,
  individual_button_fonts BOOLEAN DEFAULT FALSE,
  friendly_output_names BOOLEAN DEFAULT FALSE,
  button_images_enabled BOOLEAN DEFAULT FALSE,
  disable_button_text BOOLEAN DEFAULT FALSE,
  highlight_buttons_playing BOOLEAN DEFAULT TRUE,
  auto_load_button_images BOOLEAN DEFAULT FALSE,
  show_tracks_played BOOLEAN DEFAULT FALSE,

  -- Input settings
  mouse_left_function TEXT DEFAULT 'play_toggle',
  mouse_right_function TEXT DEFAULT 'menu',
  mouse_center_function TEXT DEFAULT 'pause',
  play_click_debounce INTEGER DEFAULT 50,
  quick_select_enabled BOOLEAN DEFAULT FALSE,

  -- Audio settings
  gain_offset INTEGER DEFAULT 0 CHECK(gain_offset IN (0, -10)),
  fade_depth INTEGER DEFAULT 40 CHECK(fade_depth IN (20, 30, 40, 50, 60)),
  auto_trim_threshold INTEGER DEFAULT -20,
  auto_trim_margin INTEGER DEFAULT 40,
  default_fade_out_time REAL DEFAULT 0.1,

  -- Feature flags
  advanced_editing_enabled BOOLEAN DEFAULT FALSE,
  search_menu_enabled BOOLEAN DEFAULT TRUE,
  gpi_enabled BOOLEAN DEFAULT FALSE,
  hot_keys_enabled BOOLEAN DEFAULT TRUE,
  pbus_control_enabled BOOLEAN DEFAULT FALSE,
  dtmf_trigger_enabled BOOLEAN DEFAULT FALSE,
  master_slave_links_enabled BOOLEAN DEFAULT TRUE,
  smpte_timecode_enabled BOOLEAN DEFAULT FALSE,
  step_to_next_enabled BOOLEAN DEFAULT FALSE,

  -- MIDI settings
  midi_in_enabled BOOLEAN DEFAULT FALSE,
  midi_out_enabled BOOLEAN DEFAULT FALSE,
  act_as_network_master BOOLEAN DEFAULT FALSE,
  loopback_out_to_in BOOLEAN DEFAULT FALSE,
  enable_fast_midi_mute BOOLEAN DEFAULT FALSE,
  use_velocity_as_initial_gain BOOLEAN DEFAULT FALSE,
  convert_velocity_zero_to_note_off BOOLEAN DEFAULT TRUE,

  -- Advanced settings
  step_to_next_skip_muted BOOLEAN DEFAULT FALSE,
  step_to_next_skip_play_next BOOLEAN DEFAULT FALSE,
  highlight_step_to_next BOOLEAN DEFAULT TRUE,
  menu_icons_enabled BOOLEAN DEFAULT TRUE,
  group_output_selection_popup BOOLEAN DEFAULT FALSE,
  smpte_use_trigger_list BOOLEAN DEFAULT FALSE,

  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Ensure only one settings row exists
CREATE UNIQUE INDEX idx_single_settings ON user_settings(id);

-- Insert default settings
INSERT INTO user_settings (id) VALUES (1);
```

### 2.3 Settings Service

```typescript
class SettingsService {
  private settings: SettingsCategory;
  private listeners: Map<string, Function[]> = new Map();

  async loadSettings(): Promise<SettingsCategory> {
    // Load from database
    const row = await db.get('SELECT * FROM user_settings WHERE id = 1');
    this.settings = this.deserializeSettings(row);
    return this.settings;
  }

  async updateSetting<K extends keyof SettingsCategory>(
    category: K,
    key: keyof SettingsCategory[K],
    value: any
  ): Promise<void> {
    // Validate value
    this.validateSetting(category, key, value);

    // Update in memory
    (this.settings[category] as any)[key] = value;

    // Persist to database
    await this.persistSetting(category, key, value);

    // Notify listeners
    this.notifyChange(category, key, value);
  }

  onSettingChange(
    category: string,
    key: string,
    callback: Function
  ): () => void {
    const listenerId = `${category}.${key}`;
    if (!this.listeners.has(listenerId)) {
      this.listeners.set(listenerId, []);
    }
    this.listeners.get(listenerId)!.push(callback);

    // Return unsubscribe function
    return () => {
      const callbacks = this.listeners.get(listenerId);
      if (callbacks) {
        const index = callbacks.indexOf(callback);
        if (index > -1) callbacks.splice(index, 1);
      }
    };
  }

  async resetToDefaults(): Promise<void> {
    await db.run('DELETE FROM user_settings');
    await db.run('INSERT INTO user_settings (id) VALUES (1)');
    await this.loadSettings();
  }

  async exportSettings(): Promise<string> {
    return JSON.stringify(this.settings, null, 2);
  }

  async importSettings(json: string): Promise<void> {
    const imported = JSON.parse(json);
    // Validate and apply each setting
    for (const [category, settings] of Object.entries(imported)) {
      for (const [key, value] of Object.entries(settings as object)) {
        await this.updateSetting(category as any, key as any, value);
      }
    }
  }
}
```

---

## 3. API Contracts

```typescript
// GET /api/settings
// Get all settings
interface GetSettingsResponse {
  settings: SettingsCategory;
}

// PUT /api/settings/:category/:key
// Update a specific setting
interface UpdateSettingRequest {
  value: any;
}

interface UpdateSettingResponse {
  success: boolean;
  settings: SettingsCategory;
}

// POST /api/settings/reset
// Reset all settings to defaults
interface ResetSettingsResponse {
  success: boolean;
  settings: SettingsCategory;
}

// GET /api/settings/export
// Export settings as JSON
interface ExportSettingsResponse {
  json: string;
}

// POST /api/settings/import
// Import settings from JSON
interface ImportSettingsRequest {
  json: string;
}

interface ImportSettingsResponse {
  success: boolean;
  imported: number;
  errors: string[];
}

// POST /api/settings/tracks/reset-played
// Reset "tracks played" indicators (Ctrl+U)
interface ResetTracksPlayedResponse {
  buttonsReset: number;
}
```

---

## 4. Key Integration Points

### 4.1 Display Settings Integration
- Button rendering respects image/text/font settings
- Session startup behavior
- Window focus management
- Track history visualization

### 4.2 Input Settings Integration
- Mouse button event handlers
- Touchscreen debounce timing
- Quick select status bar toggle

### 4.3 Audio Settings Integration
- Global gain offset applied to all outputs
- Default fade times for new tracks
- Auto-trim threshold/margin for waveform analysis

### 4.4 Feature Flags Integration
- Enable/disable subsystems (GPI, MIDI, hotkeys, etc.)
- Master/Slave links global enable (from OCC119)
- Search menu visibility (from OCC118)
- Advanced editing warnings

### 4.5 MIDI Settings Integration
- MIDI input/output enable (from OCC116)
- Velocity-to-gain mapping
- Network master mode
- Loopback testing

### 4.6 Step to Next Integration
- Sequential playback mode
- Active button highlighting
- Skip logic for muted/PlayNext buttons
- Keyboard navigation (arrow keys, Home, End)

---

## 5. Implementation Priorities

### Phase 1: Core Settings Infrastructure (Sprint 1)
**Priority:** P0

**Tasks:**
1. Create settings database schema
2. Implement SettingsService
3. Add settings API endpoints
4. Implement change notification system

**Estimated Effort:** 12-16 hours

### Phase 2: Display & Input Settings (Sprint 2)
**Priority:** P0

**Tasks:**
1. Implement display setting handlers
2. Add input setting handlers
3. Integrate with button rendering
4. Add tracks played reset

**Estimated Effort:** 8-12 hours

### Phase 3: Audio & Feature Flags (Sprint 2)
**Priority:** P1

**Tasks:**
1. Implement audio settings handlers
2. Add feature flag system
3. Integrate with audio engine
4. Add default fade time application

**Estimated Effort:** 8-12 hours

### Phase 4: MIDI & Advanced Settings (Sprint 3)
**Priority:** P1

**Tasks:**
1. Implement MIDI settings handlers
2. Add advanced settings
3. Integrate Step to Next mode
4. Add settings import/export

**Estimated Effort:** 12-16 hours

---

## 6. Testing Strategy

```typescript
describe('SettingsService', () => {
  test('loads default settings', async () => {
    const settings = await settingsService.loadSettings();
    expect(settings.display.popupHintsEnabled).toBe(true);
    expect(settings.audio.gainOffset).toBe(0);
  });

  test('updates setting and persists', async () => {
    await settingsService.updateSetting('display', 'startMaximized', true);
    const settings = await settingsService.loadSettings();
    expect(settings.display.startMaximized).toBe(true);
  });

  test('notifies listeners on change', async () => {
    const callback = jest.fn();
    settingsService.onSettingChange('display', 'startMaximized', callback);
    await settingsService.updateSetting('display', 'startMaximized', false);
    expect(callback).toHaveBeenCalledWith(false);
  });

  test('resets to defaults', async () => {
    await settingsService.updateSetting('display', 'startMaximized', true);
    await settingsService.resetToDefaults();
    const settings = await settingsService.loadSettings();
    expect(settings.display.startMaximized).toBe(false);
  });
});
```

---

## 7. Sprint Summary

**Total Estimated Effort:** 40-56 hours

**Critical Path:**
1. Core settings infrastructure → Display/Input settings
2. Feature flags → Integration with existing systems
3. MIDI settings → Advanced features

**Key Deliverables:**
- Settings database schema
- Settings management service
- Settings API endpoints
- Feature flag system
- Import/export functionality

---

## Appendix: SpotOn Manual References

**Source:** SpotOn Manual - Section 07 - Options Menu

**Key Pages:**
- Page 1: Options menu overview
- Pages 2-3: Display settings
- Pages 4-5: Button images, tracks played
- Page 6: Hot keys, SMPTE, Step to Next
- Page 7: Play-click debounce, default fade time
- Pages 8-9: MIDI settings, gain settings, mouse functions

---

**Document Prepared By:** Backend Architecture Analysis Sprint
**Next Steps:** Analyze Section 08 (Info Menu) - OCC121
