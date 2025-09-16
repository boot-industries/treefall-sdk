# Network audio streaming

The `reaper_stream` module provides a minimal API for sending and receiving
`PCM_source_transfer_t` audio blocks over real network transports.  The initial
implementation focuses on WebSocket clients built on top of
[`ixwebsocket`](https://github.com/machinezone/IXWebSocket); additional
transports can be added behind the same API in the future.

## Supported transports

* **WebSocket (`ws://`)** – Enabled by default and implemented with
  `ixwebsocket`.  Connections are opened asynchronously and data frames are
  transferred using a compact binary container that preserves the fields of
  `PCM_source_transfer_t`.
* **Secure WebSocket (`wss://`)** – Available when the SDK is configured with
  TLS support for `ixwebsocket` (see the build notes below).

SRT (`srt://`) and other schemes are reserved for later expansion and currently
return an error from `stream_open`.

## Building the SDK with networking support

The top-level CMake project now fetches `ixwebsocket` automatically via
`FetchContent` and builds the `reaper_stream` static library.  No manual setup
is required for plain `ws://` support—simply configure and build the project as
usual:

```bash
cmake -S . -B build
cmake --build build
```

Secure WebSocket connections rely on TLS.  To enable them, pass the
`IXWEBSOCKET_USE_TLS=ON` option when configuring CMake.  If OpenSSL is available
on your system you can also enable the helper routines with
`IXWEBSOCKET_ENABLE_OPEN_SSL=ON`:

```bash
cmake -S . -B build \
  -DIXWEBSOCKET_USE_TLS=ON \
  -DIXWEBSOCKET_ENABLE_OPEN_SSL=ON
```

## API reference

```c
int stream_open(const char *url);
int stream_send(int handle, const PCM_source_transfer_t *block);
int stream_receive(int handle, PCM_source_transfer_t *block);
```

1. **`stream_open`** – Creates a client connection for the supplied URL.  A
   non-zero handle indicates success.  The function blocks briefly while the
   WebSocket handshake completes (up to ~5 seconds by default).
2. **`stream_send`** – Serialises an audio block and queues it for transmission.
   The call is non-blocking with respect to network I/O.  A return value of `0`
   indicates an error such as a disconnected transport or an invalid block.
3. **`stream_receive`** – Retrieves the next decoded block, if one is available.
   The caller must pre-allocate `block->samples` with enough storage for
   `block->length * block->nch` `ReaSample` values.  The function returns the
   number of sample frames copied (or `0` when no data is ready).

Incoming frames are buffered on a per-connection queue with a modest depth
(currently 32 blocks) to absorb jitter.  When the queue is full, the oldest
block is dropped to keep the stream moving forward.

## Example: basic echo round-trip

The `sdk/example_stream/basic_stream.cpp` extension demonstrates a minimal
end-to-end exchange.  To try it out you will need a WebSocket server that echoes
binary payloads.  One option is to use
[`websocat`](https://github.com/vi/websocat):

```bash
websocat --binary ws-l:0.0.0.0:9000 reuseport:1 mirror:
```

Alternatively, any WebSocket implementation that relays binary frames unchanged
will work.

After starting the echo server, build the extension and load it into REAPER.
The example opens `ws://127.0.0.1:9000`, sends a short mono buffer and attempts
to read the echoed data back into the same structure.  Inspect the source for
additional details on buffer sizing and error handling.

## Troubleshooting

* `stream_open` returns `0`: verify the URL scheme and confirm that the remote
  server is reachable.  When using `wss://`, ensure that TLS support was enabled
  at configure time.
* `stream_send`/`stream_receive` return `0`: check that the handle is valid and
  that you provided non-null sample buffers.  Network errors are latched inside
  the connection and will cause subsequent calls to fail until the handle is
  reopened.

With these primitives in place you can build higher-level streaming workflows on
 top of the REAPER SDK while relying on well-tested networking infrastructure.
