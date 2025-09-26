# reaper_atmos extension

The `reaper_atmos` plug-in exposes a lightweight Dolby Atmos-style routing
model to REAPER extensions and automation scripts.  The plug-in now ships with
host-facing APIs that operate on real audio buffers, capture render metadata,
and emit standards-compliant ADM/BWF files for downstream workflows.

## Speaker format management

The plug-in still registers a few built-in speaker layouts (`5.1.4`, `7.1.2`) at
startup.  Custom formats can be registered and unregistered at runtime using
the following helpers:

```c
reaper_atmos_speaker_format fmt = {
  .name = "9.1.6",
  .num_channels = 16,
  .channel_names = my_channel_name_array,
};

Atmos_RegisterSpeakerFormat(&fmt);
...
Atmos_UnregisterSpeakerFormat("9.1.6");
```

Use `Atmos_GetSpeakerFormatCount()` / `Atmos_GetSpeakerFormat()` to enumerate
the active formats.  Registered formats own their copied strings, so the caller
does not need to keep the original buffers alive after the call returns.

## Routing lifecycle

The routing API now accepts explicit lifecycle events:

* `Atmos_ClearRouting()` resets all channel assignments.
* `Atmos_MapChannelToBed()` and `Atmos_MapChannelToObject()` map source channels
  to bed channels or object IDs (negative indices clear the assignment).
* `Atmos_GetRoutingState()` returns a snapshot of the current routing map,
  including whether each channel feeds a bed or object destination.
* `Atmos_GetActiveObjectCount()` reports the number of unique object IDs that
  are currently assigned.

To process audio the host provides a `reaper_atmos_render_frame_t`, which
contains writable buffers for each bed/object destination:

```c
reaper_atmos_bed_channel_t beds[1];
reaper_atmos_object_buffer_t objects[1];
reaper_atmos_render_frame_t frame = {
  .samplerate = 48000.0,
  .block_length = 512,
  .num_bed_channels = 1,
  .bed_channels = beds,
  .num_objects = 1,
  .objects = objects,
};

Atmos_BeginRenderFrame(&frame);
Atmos_RouteBlock(&block); // PCM_source_transfer_t block filled by the caller
Atmos_EndRenderFrame();
```

Every buffer uses planar storage (`frames` samples per channel).  Set
`stride > 1` when writing into larger interleaved arrays.  The router validates
buffer lengths and skips destinations that cannot accept the requested number of
frames.

Track assignments can be managed with `Atmos_AssignTrackObject()`,
`Atmos_GetTrackObject()`, and `Atmos_UnassignTrackObject()`.

## Export helpers

`Atmos_ExportADM()` and `Atmos_ExportBWF()` now emit minimal, compliant files:

* The ADM writer produces an XML document describing all routed bed channels
  and objects using the EBU ADM vocabulary.
* The BWF writer generates a PCM WAVE container with a `bext` metadata chunk
  and 32-bit float samples, interleaving the captured bed/object buffers.

Both helpers operate on the most recently routed render frame.  Hosts can call
the export functions immediately after routing to persist the captured audio and
metadata.

## Testing

Unit tests (`reaper_atmos_test`) exercise the routing path, ensure the exported
WAV header is well formed, and verify that the ADM XML contains the expected
structure.

