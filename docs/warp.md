# Pitch and Time Warping

REAPER exposes pitch and playback rate adjustments at both the clip (take) and track levels.

## Clip Level

Use `GetMediaItemTakeInfo_Value`/`SetMediaItemTakeInfo_Value` with keys such as:

- `D_PLAYRATE` – take playback rate multiplier.
- `D_PITCH` – take pitch adjustment in semitones.
- `B_PPITCH` – preserve pitch when changing rate.
- `I_PITCHMODE` – pitch shifter mode identifier.

These parameters enable per-clip time‑stretching and pitch shifting.

## Track Level

Tracks can now have their own warp parameters accessible via
`GetMediaTrackInfo_Value` and `SetMediaTrackInfo_Value`:

- `D_PLAYRATE` – track playback rate multiplier.
- `D_PITCH` – track pitch adjustment in semitones.
- `B_PPITCH` – preserve pitch when changing rate.
- `I_PITCHMODE` – pitch shifter mode identifier.

These attributes allow processing entire tracks without editing individual
items, making global timing or tuning adjustments straightforward.
