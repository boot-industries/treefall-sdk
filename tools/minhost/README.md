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

* `--json` – return structured error output (helpful for CI parsing).

Run `orpheus_minhost --help` to see the list of available subcommands:

```text
Usage: orpheus_minhost [--json] <command> [options]
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
orpheus_minhost load \
  --session tools/fixtures/sessions/basic.json
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
orpheus_minhost load \
  --session tools/fixtures/sessions/basic.json \
  --tracks drums,bass \
  --range 16:32
```

### Render a metronome click

```sh
orpheus_minhost render-click \
  --session tools/fixtures/sessions/basic.json \
  --out /tmp/minhost-click.wav \
  --range 0:16 \
  --sr 48000 \
  --bd 24
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
orpheus_minhost render-tracks \
  --session tools/fixtures/sessions/basic.json \
  --out /tmp/minhost-stems \
  --tracks drums,bass \
  --sr 96000 \
  --bd 24
```

This command writes per-track stems using the negotiated render spec (or the
provided overrides) and lists the generated filenames.

### Simulate the transport

```sh
orpheus_minhost simulate-transport \
  --session tools/fixtures/sessions/basic.json \
  --range 0:8
```

The simulation honours the session tempo and prints beat callbacks over the
requested beat range. Without `--range`, it defaults to four bars.

## JSON diagnostics

Add `--json` to any invocation to receive structured error output suitable for
machine parsing:

```sh
orpheus_minhost render-tracks --json --session missing.json --out /tmp/out
```

```json
{
  "error": {
    "code": "session.load",
    "message": "Failed to load session JSON",
    "details": [
      "No such file or directory"
    ]
  }
}
```
