#ifndef REAPER_STREAM_H
#define REAPER_STREAM_H

#include "../reaper_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

// Open a WebSocket or SRT stream. The URL scheme (ws://, wss://, srt://)
// selects the transport. Returns a non-zero handle on success.
int stream_open(const char *url);

// Send an audio block over the stream. Returns non-zero on success.
int stream_send(int handle, const PCM_source_transfer_t *block);

// Receive an audio block from the stream. The caller must allocate a buffer
// large enough to hold the requested number of samples. Returns the number of
// sample pairs received.
int stream_receive(int handle, PCM_source_transfer_t *block);

#ifdef __cplusplus
}
#endif

#endif
