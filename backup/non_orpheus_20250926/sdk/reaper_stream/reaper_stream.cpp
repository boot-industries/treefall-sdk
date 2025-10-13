#include "reaper_stream.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace {
struct NetSystemInitializer {
  NetSystemInitializer() {
    ix::initNetSystem();
  }
  ~NetSystemInitializer() {
    ix::uninitNetSystem();
  }
};

NetSystemInitializer g_netSystemInitializer;

constexpr uint32_t makeMagic() {
  return static_cast<uint32_t>('R') | (static_cast<uint32_t>('S') << 8) |
         (static_cast<uint32_t>('T') << 16) | (static_cast<uint32_t>('M') << 24);
}

constexpr uint32_t kPacketMagic = makeMagic();
constexpr uint16_t kPacketVersion = 1;
constexpr uint32_t kHeaderSizeBytes = 80;
constexpr size_t kMaxQueuedBlocks = 32;
const std::chrono::milliseconds kConnectTimeout{5000};

template <typename T> void appendLE(std::string& buffer, T value) {
  if constexpr (std::is_integral<T>::value) {
    using UnsignedT = typename std::make_unsigned<T>::type;
    UnsignedT uvalue = static_cast<UnsignedT>(value);
    for (size_t i = 0; i < sizeof(T); ++i) {
      buffer.push_back(static_cast<char>(uvalue & 0xFF));
      uvalue >>= 8;
    }
  } else if constexpr (std::is_floating_point<T>::value) {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8, "unexpected floating point size");
    using UnsignedT = typename std::conditional<sizeof(T) == 8, uint64_t, uint32_t>::type;
    UnsignedT bits = 0;
    std::memcpy(&bits, &value, sizeof(T));
    appendLE(buffer, bits);
  } else {
    static_assert(std::is_arithmetic<T>::value, "unsupported type");
  }
}

template <typename T> bool readLE(const uint8_t* data, size_t size, size_t& offset, T& out) {
  if (offset + sizeof(T) > size)
    return false;

  if constexpr (std::is_integral<T>::value) {
    using UnsignedT = typename std::make_unsigned<T>::type;
    UnsignedT value = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
      value |= static_cast<UnsignedT>(data[offset + i]) << (8 * i);
    }
    out = static_cast<T>(value);
  } else if constexpr (std::is_floating_point<T>::value) {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8, "unexpected floating point size");
    using UnsignedT = typename std::conditional<sizeof(T) == 8, uint64_t, uint32_t>::type;
    UnsignedT bits = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
      bits |= static_cast<UnsignedT>(data[offset + i]) << (8 * i);
    }
    std::memcpy(&out, &bits, sizeof(T));
  } else {
    static_assert(std::is_arithmetic<T>::value, "unsupported type");
  }

  offset += sizeof(T);
  return true;
}

struct BlockData {
  PCM_source_transfer_t meta{};
  std::vector<ReaSample> samples;
};

enum class StreamType {
  kWebSocket,
  kUnsupported,
};

StreamType parseType(const std::string& url) {
  if (url.rfind("ws://", 0) == 0 || url.rfind("wss://", 0) == 0)
    return StreamType::kWebSocket;
  return StreamType::kUnsupported;
}

class StreamConnection : public std::enable_shared_from_this<StreamConnection> {
public:
  StreamConnection(StreamType type, std::string url) : type_(type), url_(std::move(url)) {}

  ~StreamConnection() {
    if (websocket_) {
      websocket_->stop();
    }
  }

  StreamType type() const {
    return type_;
  }
  const std::string& url() const {
    return url_;
  }

  bool waitForOpen(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(stateMutex_);
    if (open_)
      return true;
    stateCv_.wait_for(lock, timeout, [this]() { return open_ || failed_; });
    return open_;
  }

  bool isOpen() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return open_;
  }

  void enqueue(BlockData&& data) {
    std::lock_guard<std::mutex> lock(incomingMutex_);
    if (incoming_.size() >= kMaxQueuedBlocks) {
      incoming_.pop();
    }
    incoming_.push(std::move(data));
  }

  bool pop(BlockData& out) {
    std::lock_guard<std::mutex> lock(incomingMutex_);
    if (incoming_.empty())
      return false;
    out = std::move(incoming_.front());
    incoming_.pop();
    return true;
  }

  void reportError(const std::string& reason) {
    markFailed(reason);
  }

  std::string error() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return lastError_;
  }

  std::unique_ptr<ix::WebSocket> websocket_;

private:
  void markOpen() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    open_ = true;
    failed_ = false;
    lastError_.clear();
    stateCv_.notify_all();
  }

  void markClosed(const std::string& reason) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    open_ = false;
    failed_ = true;
    if (!reason.empty())
      lastError_ = reason;
    stateCv_.notify_all();
  }

  void markFailed(const std::string& reason) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    failed_ = true;
    open_ = false;
    if (!reason.empty())
      lastError_ = reason;
    stateCv_.notify_all();
  }

  friend void setupWebSocket(const std::shared_ptr<StreamConnection>&);

  StreamType type_;
  std::string url_;

  mutable std::mutex stateMutex_;
  std::condition_variable stateCv_;
  bool open_ = false;
  bool failed_ = false;
  std::string lastError_;

  mutable std::mutex incomingMutex_;
  std::queue<BlockData> incoming_;
};

std::mutex g_streamMutex;
std::unordered_map<int, std::shared_ptr<StreamConnection>> g_streams;
std::atomic<int> g_nextHandle{1};

std::shared_ptr<StreamConnection> lookupConnection(int handle) {
  std::lock_guard<std::mutex> lock(g_streamMutex);
  auto it = g_streams.find(handle);
  if (it == g_streams.end())
    return nullptr;
  return it->second;
}

bool encodeBlock(const PCM_source_transfer_t* block, std::string& payload, std::string& error) {
  if (!block) {
    error = "null block";
    return false;
  }
  if (!block->samples) {
    error = "null sample buffer";
    return false;
  }
  if (block->nch <= 0) {
    error = "channel count must be positive";
    return false;
  }

  const int frames = block->samples_out > 0 ? block->samples_out : block->length;
  if (frames <= 0) {
    error = "no samples to send";
    return false;
  }

  const size_t channels = static_cast<size_t>(block->nch);
  const size_t frameCount = static_cast<size_t>(frames);
  if (frameCount > std::numeric_limits<size_t>::max() / channels) {
    error = "sample count overflow";
    return false;
  }

  const size_t totalSamples = frameCount * channels;
  if (totalSamples > std::numeric_limits<size_t>::max() / sizeof(ReaSample)) {
    error = "payload too large";
    return false;
  }

  const size_t payloadBytes = totalSamples * sizeof(ReaSample);
  if (payloadBytes > std::numeric_limits<uint32_t>::max()) {
    error = "payload exceeds protocol limit";
    return false;
  }

  payload.clear();
  payload.reserve(kHeaderSizeBytes + payloadBytes);

  appendLE<uint32_t>(payload, kPacketMagic);
  appendLE<uint16_t>(payload, kPacketVersion);
  appendLE<uint16_t>(payload, 0); // reserved
  appendLE<uint32_t>(payload, kHeaderSizeBytes);
  appendLE<uint32_t>(payload, 0); // flags
  appendLE<uint32_t>(payload, static_cast<uint32_t>(block->length));
  appendLE<uint32_t>(payload, static_cast<uint32_t>(frames));
  appendLE<uint32_t>(payload, static_cast<uint32_t>(block->nch));
  appendLE<uint32_t>(payload, static_cast<uint32_t>(payloadBytes));
  appendLE<double>(payload, block->time_s);
  appendLE<double>(payload, block->samplerate);
  appendLE<double>(payload, block->approximate_playback_latency);
  appendLE<double>(payload, block->roundtrip_latency);
  appendLE<double>(payload, block->absolute_time_s);
  appendLE<double>(payload, block->force_bpm);

  payload.append(reinterpret_cast<const char*>(block->samples), payloadBytes);
  return true;
}

bool decodeBlock(const std::string& payload, BlockData& out, std::string& error) {
  const auto* data = reinterpret_cast<const uint8_t*>(payload.data());
  const size_t size = payload.size();
  size_t offset = 0;

  uint32_t magic = 0;
  if (!readLE(data, size, offset, magic) || magic != kPacketMagic) {
    error = "invalid packet magic";
    return false;
  }

  uint16_t version = 0;
  if (!readLE(data, size, offset, version) || version != kPacketVersion) {
    error = "unsupported packet version";
    return false;
  }

  uint16_t reserved = 0;
  if (!readLE(data, size, offset, reserved)) {
    error = "truncated header";
    return false;
  }

  (void)reserved;

  uint32_t headerBytes = 0;
  if (!readLE(data, size, offset, headerBytes) || headerBytes != kHeaderSizeBytes) {
    error = "unexpected header size";
    return false;
  }

  uint32_t flags = 0;
  if (!readLE(data, size, offset, flags)) {
    error = "truncated flags";
    return false;
  }
  (void)flags;

  uint32_t lengthFrames = 0;
  uint32_t framesOut = 0;
  uint32_t channels = 0;
  uint32_t payloadBytes = 0;
  if (!readLE(data, size, offset, lengthFrames) || !readLE(data, size, offset, framesOut) ||
      !readLE(data, size, offset, channels) || !readLE(data, size, offset, payloadBytes)) {
    error = "truncated header fields";
    return false;
  }

  double time_s = 0.0;
  double samplerate = 0.0;
  double approx_latency = 0.0;
  double roundtrip_latency = 0.0;
  double absolute_time = 0.0;
  double force_bpm = 0.0;

  if (!readLE(data, size, offset, time_s) || !readLE(data, size, offset, samplerate) ||
      !readLE(data, size, offset, approx_latency) ||
      !readLE(data, size, offset, roundtrip_latency) ||
      !readLE(data, size, offset, absolute_time) || !readLE(data, size, offset, force_bpm)) {
    error = "truncated metadata";
    return false;
  }

  if (offset + payloadBytes > size) {
    error = "payload size mismatch";
    return false;
  }

  if (channels == 0) {
    error = "invalid channel count";
    return false;
  }

  if (payloadBytes % sizeof(ReaSample) != 0) {
    error = "payload not aligned to sample size";
    return false;
  }

  const size_t sampleCount = payloadBytes / sizeof(ReaSample);
  const size_t framesAvailable = sampleCount / channels;

  out.samples.resize(sampleCount);
  std::memcpy(out.samples.data(), data + offset, payloadBytes);

  out.meta.time_s = time_s;
  out.meta.samplerate = samplerate;
  out.meta.nch = static_cast<int>(std::min<uint32_t>(channels, static_cast<uint32_t>(INT_MAX)));
  out.meta.length =
      static_cast<int>(std::min<uint32_t>(lengthFrames, static_cast<uint32_t>(INT_MAX)));
  out.meta.samples_out = static_cast<int>(
      std::min<uint32_t>(framesOut ? framesOut : static_cast<uint32_t>(framesAvailable),
                         static_cast<uint32_t>(INT_MAX)));
  out.meta.samples = out.samples.data();
  out.meta.midi_events = nullptr;
  out.meta.approximate_playback_latency = approx_latency;
  out.meta.roundtrip_latency = roundtrip_latency;
  out.meta.absolute_time_s = absolute_time;
  out.meta.force_bpm = force_bpm;

  return true;
}

void setupWebSocket(const std::shared_ptr<StreamConnection>& conn) {
  if (!conn || !conn->websocket_)
    return;

  std::weak_ptr<StreamConnection> weakConn = conn;
  conn->websocket_->disableAutomaticReconnection();

  auto handshakeSeconds = std::max<int>(
      1,
      static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(kConnectTimeout).count()));
  conn->websocket_->setHandshakeTimeout(handshakeSeconds);

  conn->websocket_->setOnMessageCallback([weakConn](const ix::WebSocketMessagePtr& msg) {
    if (auto locked = weakConn.lock()) {
      switch (msg->type) {
      case ix::WebSocketMessageType::Open:
        locked->markOpen();
        break;
      case ix::WebSocketMessageType::Close: {
        std::ostringstream oss;
        oss << "connection closed";
        if (!msg->closeInfo.reason.empty())
          oss << ": " << msg->closeInfo.reason;
        locked->markClosed(oss.str());
        break;
      }
      case ix::WebSocketMessageType::Error: {
        std::ostringstream oss;
        if (!msg->errorInfo.reason.empty()) {
          oss << msg->errorInfo.reason;
        } else {
          oss << "websocket error";
        }
        if (msg->errorInfo.http_status) {
          oss << " (HTTP " << msg->errorInfo.http_status << ")";
        }
        locked->markFailed(oss.str());
        break;
      }
      case ix::WebSocketMessageType::Message: {
        if (!msg->binary) {
          locked->markFailed("received non-binary frame");
          break;
        }
        BlockData data;
        std::string error;
        if (!decodeBlock(msg->str, data, error)) {
          locked->markFailed("decode error: " + error);
          break;
        }
        data.meta.samples = data.samples.data();
        locked->enqueue(std::move(data));
        break;
      }
      default:
        break;
      }
    }
  });
}

} // namespace

int stream_open(const char* url) {
  if (!url || !*url)
    return 0;

  std::string urlStr(url);
  StreamType type = parseType(urlStr);
  if (type != StreamType::kWebSocket)
    return 0;

  auto conn = std::make_shared<StreamConnection>(type, urlStr);
  conn->websocket_ = std::make_unique<ix::WebSocket>();
  conn->websocket_->setUrl(urlStr);
  conn->websocket_->setAutoThreadName(false);

  setupWebSocket(conn);

  const int handle = g_nextHandle.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(g_streamMutex);
    g_streams.emplace(handle, conn);
  }

  conn->websocket_->start();

  if (!conn->waitForOpen(kConnectTimeout)) {
    conn->reportError(conn->error().empty() ? std::string("connection timeout") : conn->error());
    conn->websocket_->stop();
    std::lock_guard<std::mutex> lock(g_streamMutex);
    g_streams.erase(handle);
    return 0;
  }

  return handle;
}

int stream_send(int handle, const PCM_source_transfer_t* block) {
  if (!block || !block->samples)
    return 0;

  auto conn = lookupConnection(handle);
  if (!conn || conn->type() != StreamType::kWebSocket)
    return 0;

  if (!conn->isOpen())
    return 0;

  std::string payload;
  std::string error;
  if (!encodeBlock(block, payload, error)) {
    conn->reportError("encode error: " + error);
    return 0;
  }

  ix::WebSocketSendInfo info = conn->websocket_->sendBinary(payload);
  if (!info.success) {
    conn->reportError("websocket send failed");
    return 0;
  }

  return 1;
}

int stream_receive(int handle, PCM_source_transfer_t* block) {
  if (!block || !block->samples)
    return 0;

  auto conn = lookupConnection(handle);
  if (!conn || conn->type() != StreamType::kWebSocket)
    return 0;

  BlockData data;
  if (!conn->pop(data))
    return 0;

  if (data.meta.nch <= 0)
    return 0;

  if (block->nch != data.meta.nch) {
    if (block->nch == 0) {
      block->nch = data.meta.nch;
    } else if (block->nch != data.meta.nch) {
      conn->reportError("channel count mismatch");
      return 0;
    }
  }

  const size_t availableSamples = data.samples.size();
  if (availableSamples == 0)
    return 0;

  const size_t requestedSamples =
      static_cast<size_t>(block->length) * static_cast<size_t>(block->nch);
  if (requestedSamples == 0)
    return 0;

  const size_t copySamples = std::min(availableSamples, requestedSamples);
  std::memcpy(block->samples, data.samples.data(), copySamples * sizeof(ReaSample));

  block->time_s = data.meta.time_s;
  block->samplerate = data.meta.samplerate;
  block->nch = data.meta.nch;
  block->length = data.meta.length;
  const int framesAvailable =
      data.meta.nch > 0 ? static_cast<int>(availableSamples / data.meta.nch) : 0;
  const int framesCopied = data.meta.nch > 0 ? static_cast<int>(copySamples / data.meta.nch) : 0;
  const int reportedFrames =
      data.meta.samples_out > 0 ? std::min(data.meta.samples_out, framesCopied) : framesCopied;
  block->samples_out = reportedFrames;
  block->midi_events = nullptr;
  block->approximate_playback_latency = data.meta.approximate_playback_latency;
  block->roundtrip_latency = data.meta.roundtrip_latency;
  block->absolute_time_s = data.meta.absolute_time_s;
  block->force_bpm = data.meta.force_bpm;

  (void)framesAvailable;

  return block->samples_out;
}
