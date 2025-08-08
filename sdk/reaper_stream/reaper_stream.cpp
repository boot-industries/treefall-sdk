#include "reaper_stream.h"

#include <unordered_map>
#include <queue>
#include <mutex>
#include <vector>
#include <string>
#include <cstring>

struct BlockData {
  PCM_source_transfer_t meta;
  std::vector<ReaSample> samples;
};

struct StreamConnection {
  enum Type { WS, SRT } type;
  std::mutex mtx;
  std::queue<BlockData> incoming; // simple loopback queue
};

static std::unordered_map<int, StreamConnection> g_streams;
static int g_nextHandle = 1;

static StreamConnection::Type parseType(const std::string &url)
{
  if (url.rfind("srt://",0) == 0) return StreamConnection::SRT;
  return StreamConnection::WS;
}

int stream_open(const char *url)
{
  if (!url) return 0;
  StreamConnection conn;
  conn.type = parseType(url);
  int handle = g_nextHandle++;
  g_streams.emplace(handle, std::move(conn));
  return handle;
}

int stream_send(int handle, const PCM_source_transfer_t *block)
{
  auto it = g_streams.find(handle);
  if (it == g_streams.end() || !block || !block->samples) return 0;

  BlockData bd;
  bd.meta = *block;
  size_t n = (size_t)block->length * (size_t)block->nch;
  bd.samples.assign(block->samples, block->samples + n);
  bd.meta.samples = bd.samples.data();

  {
    std::lock_guard<std::mutex> lock(it->second.mtx);
    it->second.incoming.push(std::move(bd));
  }
  return 1;
}

int stream_receive(int handle, PCM_source_transfer_t *block)
{
  auto it = g_streams.find(handle);
  if (it == g_streams.end() || !block || !block->samples) return 0;
  std::lock_guard<std::mutex> lock(it->second.mtx);
  if (it->second.incoming.empty()) return 0;

  BlockData bd = std::move(it->second.incoming.front());
  it->second.incoming.pop();

  size_t n = (size_t)bd.meta.length * (size_t)bd.meta.nch;
  size_t requested = (size_t)block->length * (size_t)block->nch;
  if (requested < n) n = requested;
  std::memcpy(block->samples, bd.samples.data(), n * sizeof(ReaSample));
  *block = bd.meta;
  block->samples = block->samples; // keep caller buffer
  block->samples_out = (int)(n / block->nch);
  return block->samples_out;
}
