#ifndef REAPER_STREAM_PCM_TYPES_H
#define REAPER_STREAM_PCM_TYPES_H

#ifndef _REAPER_PLUGIN_H_

#ifndef REASAMPLE_SIZE
#define REASAMPLE_SIZE 8
#endif

#if REASAMPLE_SIZE == 4
typedef float ReaSample;
#else
typedef double ReaSample;
#endif

struct MIDI_eventlist;

typedef struct _PCM_source_transfer_t {
  double time_s;
  double samplerate;
  int nch;
  int length;
  ReaSample* samples;
  int samples_out;
  MIDI_eventlist* midi_events;
  double approximate_playback_latency;
  double roundtrip_latency;
  double absolute_time_s;
  double force_bpm;
} PCM_source_transfer_t;

#endif // !_REAPER_PLUGIN_H_

#endif // REAPER_STREAM_PCM_TYPES_H
