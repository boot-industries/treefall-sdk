# Network audio streaming

The `reaper_stream` module provides a minimal API for sending and receiving
`PCM_source_transfer_t` audio blocks over a network transport.
Connections are specified using a URL with either a WebSocket (`ws://` or
`wss://`) or SRT (`srt://`) scheme.

## API

```c
int stream_open(const char *url);
int stream_send(int handle, const PCM_source_transfer_t *block);
int stream_receive(int handle, PCM_source_transfer_t *block);
```

1. Call `stream_open()` with the desired URL to create a connection. The
   return value is a handle used for subsequent calls.
2. Use `stream_send()` to transmit an audio block to the remote peer.
3. Use `stream_receive()` to fetch the next available audio block from the
   connection. The caller must allocate a buffer for `block->samples`.

The provided implementation performs a simple loopback of sent audio blocks
and is intended as a reference for building more sophisticated transports.

See `sdk/example_stream` for a basic example extension.
