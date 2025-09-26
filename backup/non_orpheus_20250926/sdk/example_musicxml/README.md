# MusicXML Round-trip Example

This example plug-in shows how to export MIDI data from a REAPER track to a
MusicXML file and import a MusicXML file back into a track.

## Building

Ensure the WDL submodule is checked out next to the `sdk` directory:

```bash
git submodule update --init --recursive
```

Then compile `musicxml_util.cpp` as a REAPER extension and copy the resulting
plug-in to your REAPER extensions directory.

## Usage

1. Load the plug-in into REAPER.
2. Select a track that contains MIDI items.
3. Call the `ExportTrackMIDIToMusicXML` API function to create `output.musicxml`.
4. Open the file in notation software such as Sibelius and make edits.
5. Export the changes as MusicXML from the notation software.
6. Call `ImportTrackMIDIFromMusicXML` to read the edited file back onto the track.

The accompanying `sample_project.rpp` and `sample.musicxml` demonstrate a
simple round-trip workflow.
