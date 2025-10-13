# REAPER Adapter (Experimental)

**Status:** ⚠️  Experimental - Not recommended for production use

This adapter provides a modern ABI-based integration between Orpheus SDK and REAPER. It is built entirely on the public Orpheus ABI and does not use legacy WDL/SWELL/WALTER frameworks.

## Features

- **Session Import:** Load Orpheus session JSON files into REAPER panel
- **Click Track Rendering:** Generate metronome click tracks from session tempo
- **Panel Visualization:** Display session metadata, tracks, clips, markers, and playlist lanes
- **ABI-Based:** Uses `orpheus_session_abi_v1`, `orpheus_clipgrid_abi_v1`, and `orpheus_render_abi_v1`

## Building

The REAPER adapter is **disabled by default**. To enable it:

```bash
cmake -S . -B build -DORPHEUS_ENABLE_ADAPTER_REAPER=ON
cmake --build build --target reaper_orpheus
```

This produces a shared library: `build/adapters/reaper/libreaper_orpheus.{so|dylib|dll}`

## Installation

1. Build the adapter as shown above
2. Copy the shared library to your REAPER UserPlugins directory:
   - **macOS:** `~/Library/Application Support/REAPER/UserPlugins/`
   - **Windows:** `%APPDATA%\REAPER\UserPlugins\`
   - **Linux:** `~/.config/REAPER/UserPlugins/`
3. Restart REAPER
4. Access via Extensions → Orpheus Panel

## API Surface

### Exported Functions

- `ReaperExtensionName()` - Returns "Orpheus Panel"
- `ReaperExtensionVersion()` - Returns ABI version string
- `ReaperExtensionPanelText()` - Returns formatted panel content
- `OrpheusTogglePanel()` - Show/hide panel
- `OrpheusImportSession(const char* json_path)` - Load session from JSON
- `OrpheusRenderClickToFile(const char* output_path)` - Render click track

## Architecture

```
REAPER Extension Entry Points
         ↓
  reaper_entry.cpp
         ↓
  Orpheus ABI Functions
         ↓
  Orpheus C++ Core SDK
```

The adapter uses RAII wrappers (`SessionGuard`) for ABI handle management and provides thread-safe access to session state via `std::mutex`.

## Experimental Status

This adapter is marked **experimental** because:

- Limited testing across REAPER versions (tested primarily on REAPER 7.x)
- No UI components beyond text-based panel display
- No integration with REAPER's track/item structure
- No real-time playback integration
- API may change without notice

## Legacy Code

Previous REAPER plugins using WDL/SWELL/WALTER frameworks have been quarantined to `backup/non_orpheus_20250926/` and are no longer maintained. The current adapter:

- Does **not** use WDL/SWELL/WALTER
- Does **not** depend on private Orpheus headers
- Uses only the public ABI interface
- Compiles independently from core implementation details

## Future Work

Potential enhancements (not currently planned):

- GUI panel with clickable elements (via SWELL or Qt)
- Import session as REAPER project structure
- Real-time transport synchronization
- Clip triggering from REAPER actions
- Integration with REAPER's render API

## Contributing

Due to experimental status, contributions should be coordinated with maintainers before significant work. See `CONTRIBUTING.md` in the repository root.

## License

SPDX-License-Identifier: MIT

See `LICENSE` in the repository root.
