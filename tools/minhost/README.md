# Orpheus Minhost CLI

`orpheus_minhost` is a lightweight host harness that exercises the public Orpheus
session ABI. It can load a session JSON file, render metronome clicks and track
stems, or drive a simple transport simulation without relying on a specific DAW.

## Building

```sh
cmake -S . -B build
cmake --build build --target orpheus_minhost
```

The resulting binary lives at `build/orpheus_minhost`.

## Global flags

All subcommands understand the following global options:

* `--session <file>` – default session JSON to load.
* `--spec <file>` – default click render spec overrides.
* `--tracks <a,b,c>` – restrict session loading/rendering to named tracks.
* `--range <start:end>` – limit the session beat range (start and/or end).
* `--sr <hz>` – override render/click sample rate.
* `--bd <bits>` – override render bit depth (16/24/32) or click bit-depth hint.
* `--json` – emit structured JSON summaries (success + error cases).

Run `orpheus_minhost --help` to see the list of available subcommands:

```text
Usage: orpheus_minhost [global options] <command> [options]
Commands:
  load                 Load a session and print metadata
  render-click         Render a metronome click track
  render-tracks        Render track stems to disk
  simulate-transport   Run a transport simulation
```

Each subcommand also supports `--help` for command-specific options.

## Examples

### Load a session and inspect negotiated capabilities

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json load
```

Example output:

```text
ABI negotiation
  session    v1.0 ✅
  clipgrid   v1.0 ✅
  render     v1.0 ✅
Session: 'demo session'
  tempo       : 120.00 bpm
  range       : 0.00 → 32.00 beats
  tracks      : 4 loaded
  clips       : 12
  render spec : 48000 Hz, 24-bit, dither on
```

Use `--tracks` and `--range` to scope the load:

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json \
  --tracks drums,bass \
  --range 16:32 \
  load
```

### Render a metronome click

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json \
  --range 0:16 \
  --sr 48000 \
  --bd 24 \
  render-click \
  --out /tmp/minhost-click.wav
```

The click renderer accepts optional JSON overrides via `--spec`:

```json
{
  "tempo_bpm": 90.0,
  "bars": 8,
  "gain": 0.25,
  "output_path": "/tmp/custom-click.wav"
}
```

When supplied, these overrides merge into the generated spec.

### Render track stems

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json \
  --tracks drums,bass \
  --sr 96000 \
  --bd 24 \
  render-tracks \
  --out /tmp/minhost-stems
```

This command writes per-track stems using the negotiated render spec (or the
provided overrides) and lists the generated filenames.

### Simulate the transport

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json \
  --range 0:8 \
  simulate-transport
```

The simulation honours the session tempo and prints beat callbacks over the
requested beat range. Without `--range`, it defaults to four bars.

## JSON diagnostics

Add `--json` to any invocation to receive structured summaries that are easy to
consume from automation:

```sh
orpheus_minhost --session tools/fixtures/sessions/basic.json \
  --range 0:8 \
  render-click --json
```

```json
{
  "command": "render-click",
  "abi": {
    "session": {"major": 1, "minor": 0, "ok": true},
    "clipgrid": {"major": 1, "minor": 0, "ok": true},
    "render": {"major": 1, "minor": 0, "ok": true}
  },
  "spec": {
    "tempo_bpm": 120.000000,
    "bars": 4,
    "sample_rate": 48000,
    "channels": 2,
    "gain": 0.300000,
    "click_frequency_hz": 1000.000000,
    "click_duration_seconds": 0.050000
  },
  "output_path": null,
  "suggested_path": "demo-session_click_48000hz_24bit.wav"
}
```

Errors are also emitted in the same JSON envelope with an `"error"` object.
