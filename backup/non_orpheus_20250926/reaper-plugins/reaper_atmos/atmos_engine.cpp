#include "atmos_engine.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr const char* kDefaultBedFormats[][12] = {
    {"L", "R", "C", "LFE", "Ls", "Rs", "Ltf", "Rtf", "Ltr", "Rtr", nullptr, nullptr},
    {"L", "R", "C", "LFE", "Lss", "Rss", "Lrs", "Rrs", "Ltf", "Rtf", nullptr, nullptr}};

constexpr const char* kDefaultFormatNames[] = {"5.1.4", "7.1.2"};

struct ScopedError {
  std::string* out;
  void assign(const std::string& msg) {
    if (out)
      *out = msg;
  }
};

inline int safeStride(const reaper_atmos_buffer_t& buffer) {
  return buffer.stride <= 0 ? 1 : buffer.stride;
}

void copyToBuffer(const ReaSample* src, int frames, const reaper_atmos_buffer_t& buffer) {
  if (!buffer.data)
    return;
  const int stride = safeStride(buffer);
  ReaSample* dst = buffer.data;
  for (int i = 0; i < frames; ++i) {
    dst[i * stride] = src[i];
  }
}

template <typename T> T clampTo(const T& value, const T& minV, const T& maxV) {
  return std::max(minV, std::min(maxV, value));
}

std::string makeId(const char* prefix, int index) {
  std::ostringstream oss;
  oss << prefix << '_' << std::setw(4) << std::setfill('0') << index;
  return oss.str();
}

void writeU16(std::ostream& os, uint16_t value) {
  char bytes[2];
  bytes[0] = static_cast<char>(value & 0xff);
  bytes[1] = static_cast<char>((value >> 8) & 0xff);
  os.write(bytes, sizeof(bytes));
}

void writeU32(std::ostream& os, uint32_t value) {
  char bytes[4];
  bytes[0] = static_cast<char>(value & 0xff);
  bytes[1] = static_cast<char>((value >> 8) & 0xff);
  bytes[2] = static_cast<char>((value >> 16) & 0xff);
  bytes[3] = static_cast<char>((value >> 24) & 0xff);
  os.write(bytes, sizeof(bytes));
}
} // namespace

AtmosEngine::AtmosEngine() {
  std::lock_guard<std::mutex> lock(mutex_);
  ensureBuiltinFormatsLocked();
}

void AtmosEngine::ensureBuiltinFormatsLocked() {
  if (builtinFormatsLoaded_)
    return;

  for (size_t i = 0; i < std::size(kDefaultFormatNames); ++i) {
    reaper_atmos_speaker_format fmt{};
    fmt.name = kDefaultFormatNames[i];
    int channelCount = 0;
    while (kDefaultBedFormats[i][channelCount])
      ++channelCount;
    fmt.num_channels = channelCount;
    fmt.channel_names = const_cast<const char**>(kDefaultBedFormats[i]);
    addFormatLocked(fmt);
  }
  builtinFormatsLoaded_ = true;
}

void AtmosEngine::addFormatLocked(const reaper_atmos_speaker_format& fmt) {
  SpeakerFormatStorage storage;
  storage.name = fmt.name ? fmt.name : "";
  const int channelCount = fmt.num_channels > 0 ? fmt.num_channels : 0;
  storage.channelNames.reserve(static_cast<size_t>(channelCount));
  for (int i = 0; i < channelCount; ++i) {
    const char* ch = (fmt.channel_names && fmt.channel_names[i]) ? fmt.channel_names[i] : "";
    storage.channelNames.emplace_back(ch);
  }
  storage.channelNamePtrs.resize(storage.channelNames.size());
  for (size_t i = 0; i < storage.channelNames.size(); ++i) {
    storage.channelNamePtrs[i] = storage.channelNames[i].c_str();
  }
  storage.view.name = storage.name.c_str();
  storage.view.num_channels = static_cast<int>(storage.channelNamePtrs.size());
  storage.view.channel_names =
      storage.channelNamePtrs.empty() ? nullptr : storage.channelNamePtrs.data();
  speakerFormats_.push_back(std::move(storage));
}

void AtmosEngine::registerSpeakerFormat(const reaper_atmos_speaker_format* fmt) {
  if (!fmt)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  ensureBuiltinFormatsLocked();
  addFormatLocked(*fmt);
}

bool AtmosEngine::unregisterSpeakerFormat(const char* name) {
  if (!name)
    return false;
  std::lock_guard<std::mutex> lock(mutex_);
  ensureBuiltinFormatsLocked();
  for (auto it = speakerFormats_.begin(); it != speakerFormats_.end(); ++it) {
    if (it->name == name) {
      speakerFormats_.erase(it);
      return true;
    }
  }
  return false;
}

int AtmosEngine::getSpeakerFormatCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int>(speakerFormats_.size());
}

const reaper_atmos_speaker_format* AtmosEngine::getSpeakerFormat(int idx) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (idx < 0 || idx >= static_cast<int>(speakerFormats_.size()))
    return nullptr;
  return &speakerFormats_[static_cast<size_t>(idx)].view;
}

void AtmosEngine::ensureChannelMapSize(int nch) {
  if (nch <= 0)
    return;
  if (static_cast<int>(channelMap_.size()) < nch) {
    channelMap_.resize(static_cast<size_t>(nch));
  }
}

void AtmosEngine::mapChannelToBed(int channel, int bedChannelIndex) {
  if (channel < 0)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  ensureChannelMapSize(channel + 1);
  AtmosChannelDestination dest;
  dest.assigned = bedChannelIndex >= 0;
  dest.is_object = false;
  dest.index = bedChannelIndex;
  channelMap_[static_cast<size_t>(channel)] = dest;
}

void AtmosEngine::mapChannelToObject(int channel, int objectId) {
  if (channel < 0)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  ensureChannelMapSize(channel + 1);
  AtmosChannelDestination dest;
  dest.assigned = objectId >= 0;
  dest.is_object = true;
  dest.index = objectId;
  channelMap_[static_cast<size_t>(channel)] = dest;
}

void AtmosEngine::clearRouting() {
  std::lock_guard<std::mutex> lock(mutex_);
  channelMap_.clear();
}

bool AtmosEngine::beginFrame(const reaper_atmos_render_frame_t& frame, std::string* error) {
  ScopedError err{error};
  if (frame.block_length <= 0) {
    err.assign("block_length must be positive");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  frame_.hasFrame = true;
  frame_.samplerate = frame.samplerate;
  frame_.block_length = frame.block_length;
  frame_.beds.clear();
  frame_.objects.clear();
  frame_.bedLookup.clear();
  frame_.objectLookup.clear();

  if (frame.num_bed_channels > 0 && frame.bed_channels) {
    frame_.beds.reserve(static_cast<size_t>(frame.num_bed_channels));
    for (int i = 0; i < frame.num_bed_channels; ++i) {
      const reaper_atmos_bed_channel_t& bed = frame.bed_channels[i];
      BedSlot slot;
      slot.channel_index = bed.channel_index;
      slot.channel_name = bed.channel_name ? bed.channel_name : "";
      slot.buffer = bed.buffer;
      frame_.beds.push_back(slot);
      frame_.bedLookup[slot.channel_index] = frame_.beds.size() - 1;
    }
  }

  if (frame.num_objects > 0 && frame.objects) {
    frame_.objects.reserve(static_cast<size_t>(frame.num_objects));
    for (int i = 0; i < frame.num_objects; ++i) {
      const reaper_atmos_object_buffer_t& obj = frame.objects[i];
      ObjectSlot slot;
      slot.object_id = obj.object_id;
      slot.buffer = obj.buffer;
      frame_.objects.push_back(slot);
      frame_.objectLookup[slot.object_id] = frame_.objects.size() - 1;
    }
  }

  capture_.valid = false;
  capture_.samplerate = frame_.samplerate;
  capture_.frames = 0;
  capture_.bedChannelIndices.clear();
  capture_.bedChannelNames.clear();
  capture_.bedAudio.assign(frame_.beds.size(), {});
  for (const auto& bed : frame_.beds) {
    capture_.bedChannelIndices.push_back(bed.channel_index);
    capture_.bedChannelNames.push_back(bed.channel_name);
  }
  capture_.objectIds.clear();
  capture_.objectAudio.assign(frame_.objects.size(), {});
  for (const auto& obj : frame_.objects) {
    capture_.objectIds.push_back(obj.object_id);
  }

  return true;
}

void AtmosEngine::endFrame() {
  std::lock_guard<std::mutex> lock(mutex_);
  frame_.hasFrame = false;
}

bool AtmosEngine::processBlock(const PCM_source_transfer_t& block, std::string* error) {
  ScopedError err{error};
  if (!block.samples) {
    err.assign("PCM block missing samples pointer");
    return false;
  }
  if (block.nch <= 0 || block.length <= 0) {
    err.assign("PCM block has no channels or length");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!frame_.hasFrame) {
    err.assign("no active render frame");
    return false;
  }

  ensureChannelMapSize(block.nch);
  int frames = block.length;
  if (block.samples_out > 0 && block.samples_out < frames)
    frames = block.samples_out;
  if (frames > frame_.block_length) {
    err.assign("PCM block longer than active frame");
    return false;
  }
  if (frames <= 0)
    return true;

  for (int ch = 0; ch < block.nch; ++ch) {
    const AtmosChannelDestination& dest = channelMap_[static_cast<size_t>(ch)];
    if (!dest.assigned || dest.index < 0)
      continue;

    const ReaSample* src =
        block.samples + static_cast<size_t>(ch) * static_cast<size_t>(block.length);

    if (dest.is_object) {
      auto it = frame_.objectLookup.find(dest.index);
      if (it == frame_.objectLookup.end())
        continue;
      ObjectSlot& slot = frame_.objects[it->second];
      if (frames > slot.buffer.frames)
        continue;
      copyToBuffer(src, frames, slot.buffer);
      capture_.objectAudio[it->second].assign(src, src + frames);
    } else {
      auto it = frame_.bedLookup.find(dest.index);
      if (it == frame_.bedLookup.end())
        continue;
      BedSlot& slot = frame_.beds[it->second];
      if (frames > slot.buffer.frames)
        continue;
      copyToBuffer(src, frames, slot.buffer);
      capture_.bedAudio[it->second].assign(src, src + frames);
    }
  }

  capture_.frames = frames;
  capture_.valid = true;
  return true;
}

bool AtmosEngine::getRoutingState(reaper_atmos_routing_state_t* state) const {
  if (!state)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  state->samplerate = frame_.samplerate;
  state->block_length = frame_.block_length;
  const int total = static_cast<int>(channelMap_.size());
  state->destinations_count = total;
  int written = 0;
  if (state->destinations && state->destinations_capacity > 0) {
    written = std::min(state->destinations_capacity, total);
    for (int i = 0; i < written; ++i) {
      const AtmosChannelDestination& dest = channelMap_[static_cast<size_t>(i)];
      reaper_atmos_routing_dest_t entry{};
      entry.source_channel = i;
      entry.is_object = dest.assigned && dest.is_object ? 1 : 0;
      entry.destination_index = dest.assigned ? dest.index : -1;
      entry.object_id = (dest.assigned && dest.is_object) ? dest.index : -1;
      state->destinations[i] = entry;
    }
  }
  state->destinations_written = written;
  return true;
}

int AtmosEngine::getActiveObjectCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::unordered_set<int> ids;
  for (const auto& dest : channelMap_) {
    if (dest.assigned && dest.is_object && dest.index >= 0)
      ids.insert(dest.index);
  }
  return static_cast<int>(ids.size());
}

void AtmosEngine::assignTrackObject(MediaTrack* track, int object_id) {
  if (!track)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  trackAssignments_[track] = object_id;
}

void AtmosEngine::unassignTrackObject(MediaTrack* track) {
  if (!track)
    return;
  std::lock_guard<std::mutex> lock(mutex_);
  trackAssignments_.erase(track);
}

int AtmosEngine::getTrackObject(MediaTrack* track) const {
  if (!track)
    return -1;
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = trackAssignments_.find(track);
  if (it == trackAssignments_.end())
    return -1;
  return it->second;
}

bool AtmosEngine::exportBWF(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!capture_.valid)
    return false;
  return writeBWFFile(path, capture_);
}

bool AtmosEngine::exportADM(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!capture_.valid)
    return false;
  return writeADMFile(path, capture_);
}

bool AtmosEngine::writeBWFFile(const std::string& path, const FrameCapture& capture) {
  const int bedCount = static_cast<int>(capture.bedAudio.size());
  const int objectCount = static_cast<int>(capture.objectAudio.size());
  const int totalChannels = bedCount + objectCount;
  const int frames = capture.frames;

  std::ofstream ofs(path, std::ios::binary);
  if (!ofs)
    return false;

  const uint32_t sampleRate = static_cast<uint32_t>(
      clampTo<double>(capture.samplerate, 1.0, std::numeric_limits<uint32_t>::max()));
  const uint16_t bitsPerSample = 32;
  const uint16_t bytesPerSample = bitsPerSample / 8;
  const uint32_t dataSize =
      static_cast<uint32_t>(frames) * static_cast<uint32_t>(totalChannels) * bytesPerSample;
  const uint32_t fmtChunkSize = 16;
  const uint32_t bextChunkSize = 602;
  const uint32_t riffSize = 4 + (8 + fmtChunkSize) + (8 + bextChunkSize) + (8 + dataSize);

  ofs.write("RIFF", 4);
  writeU32(ofs, riffSize);
  ofs.write("WAVE", 4);

  // fmt chunk
  ofs.write("fmt ", 4);
  writeU32(ofs, fmtChunkSize);
  writeU16(ofs, 3); // WAVE_FORMAT_IEEE_FLOAT
  writeU16(ofs, static_cast<uint16_t>(totalChannels));
  writeU32(ofs, sampleRate);
  const uint32_t byteRate = sampleRate * static_cast<uint32_t>(totalChannels) * bytesPerSample;
  writeU32(ofs, byteRate);
  writeU16(ofs, static_cast<uint16_t>(totalChannels * bytesPerSample));
  writeU16(ofs, bitsPerSample);

  // bext chunk
  ofs.write("bext", 4);
  writeU32(ofs, bextChunkSize);
  std::array<char, 602> bext{};
  const char* description = "REAPER Atmos Export";
  std::strncpy(bext.data(), description,
               std::min<size_t>(description ? std::strlen(description) : 0, 256));
  // version
  bext[256 + 32 + 32 + 10 + 8 + 4 + 4] = 0x01;
  ofs.write(bext.data(), bext.size());

  // data chunk
  ofs.write("data", 4);
  writeU32(ofs, dataSize);

  std::vector<float> interleaved(static_cast<size_t>(frames) * static_cast<size_t>(totalChannels),
                                 0.0f);
  for (int frame = 0; frame < frames; ++frame) {
    int channel = 0;
    for (int i = 0; i < bedCount; ++i) {
      if (frame < static_cast<int>(capture.bedAudio[i].size()))
        interleaved[static_cast<size_t>(frame) * static_cast<size_t>(totalChannels) + channel] =
            static_cast<float>(capture.bedAudio[i][frame]);
      ++channel;
    }
    for (int i = 0; i < objectCount; ++i) {
      if (frame < static_cast<int>(capture.objectAudio[i].size()))
        interleaved[static_cast<size_t>(frame) * static_cast<size_t>(totalChannels) + channel] =
            static_cast<float>(capture.objectAudio[i][frame]);
      ++channel;
    }
  }

  for (float sample : interleaved) {
    uint32_t raw = 0;
    std::memcpy(&raw, &sample, sizeof(float));
    writeU32(ofs, raw);
  }

  return ofs.good();
}

bool AtmosEngine::writeADMFile(const std::string& path, const FrameCapture& capture) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs)
    return false;

  const int totalChannels = static_cast<int>(capture.bedAudio.size() + capture.objectAudio.size());
  if (totalChannels == 0) {
    // Still output a minimal document
  }

  ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ofs << "<adm:adm xmlns:adm=\"urn:ebu:metadata-schema:ebuCore_2014\">\n";
  ofs << "  <adm:audioProgramme adm:id=\"APR_0001\" adm:audioProgrammeName=\"REAPER Atmos "
         "Programme\">\n";
  ofs << "    <adm:audioContentIDRef>ACO_0001</adm:audioContentIDRef>\n";
  ofs << "  </adm:audioProgramme>\n";
  ofs << "  <adm:audioContent adm:id=\"ACO_0001\" adm:audioContentName=\"REAPER Atmos Content\">\n";

  struct ChannelMeta {
    std::string name;
    bool isObject = false;
  };

  std::vector<ChannelMeta> channels;
  channels.reserve(static_cast<size_t>(totalChannels));
  int counter = 1;
  for (size_t i = 0; i < capture.bedAudio.size(); ++i) {
    ChannelMeta meta;
    meta.name = !capture.bedChannelNames.empty() ? capture.bedChannelNames[i]
                                                 : ("Bed " + std::to_string(i));
    meta.isObject = false;
    channels.push_back(std::move(meta));
  }
  for (size_t i = 0; i < capture.objectAudio.size(); ++i) {
    ChannelMeta meta;
    meta.name = "Object " + std::to_string(capture.objectIds[i]);
    meta.isObject = true;
    channels.push_back(std::move(meta));
  }

  counter = 1;
  for (const ChannelMeta& meta : channels) {
    std::string objectId = makeId("AO", counter);
    ofs << "    <adm:audioObjectIDRef>" << objectId << "</adm:audioObjectIDRef>\n";
    ++counter;
  }
  ofs << "  </adm:audioContent>\n";

  counter = 1;
  for (const ChannelMeta& meta : channels) {
    const std::string objectId = makeId("AO", counter);
    const std::string trackUid = makeId("ATU", counter);
    ofs << "  <adm:audioObject adm:id=\"" << objectId << "\" adm:audioObjectName=\"" << meta.name
        << "\">\n";
    ofs << "    <adm:audioTrackUIDRef>" << trackUid << "</adm:audioTrackUIDRef>\n";
    ofs << "  </adm:audioObject>\n";
    ++counter;
  }

  counter = 1;
  for (const ChannelMeta& meta : channels) {
    const std::string trackUid = makeId("ATU", counter);
    const std::string packId = makeId("APF", counter);
    const std::string trackFormat = makeId("AT", counter);
    const std::string streamFormat = makeId("ASF", counter);
    const std::string channelFormat = makeId("ACF", counter);
    ofs << "  <adm:audioTrackUID adm:id=\"" << trackUid << "\" adm:trackIndex=\"" << counter
        << "\">\n";
    ofs << "    <adm:audioPackFormatIDRef>" << packId << "</adm:audioPackFormatIDRef>\n";
    ofs << "    <adm:audioTrackFormatIDRef>" << trackFormat << "</adm:audioTrackFormatIDRef>\n";
    ofs << "  </adm:audioTrackUID>\n";

    ofs << "  <adm:audioPackFormat adm:id=\"" << packId << "\" adm:audioPackFormatName=\""
        << meta.name << "\" adm:type=\"" << (meta.isObject ? "Objects" : "DirectSpeakers")
        << "\">\n";
    ofs << "    <adm:audioChannelFormatIDRef>" << channelFormat
        << "</adm:audioChannelFormatIDRef>\n";
    ofs << "  </adm:audioPackFormat>\n";

    ofs << "  <adm:audioTrackFormat adm:id=\"" << trackFormat << "\" adm:audioTrackFormatName=\""
        << meta.name << "\">\n";
    ofs << "    <adm:audioStreamFormatIDRef>" << streamFormat << "</adm:audioStreamFormatIDRef>\n";
    ofs << "  </adm:audioTrackFormat>\n";

    ofs << "  <adm:audioStreamFormat adm:id=\"" << streamFormat << "\" adm:audioStreamFormatName=\""
        << meta.name << "\">\n";
    ofs << "    <adm:audioChannelFormatIDRef>" << channelFormat
        << "</adm:audioChannelFormatIDRef>\n";
    ofs << "  </adm:audioStreamFormat>\n";

    ofs << "  <adm:audioChannelFormat adm:id=\"" << channelFormat
        << "\" adm:audioChannelFormatName=\"" << meta.name << "\">\n";
    ofs << "    <adm:audioTypeDefinition>" << (meta.isObject ? "Objects" : "DirectSpeakers")
        << "</adm:audioTypeDefinition>\n";
    ofs << "  </adm:audioChannelFormat>\n";
    ++counter;
  }

  ofs << "</adm:adm>\n";
  return ofs.good();
}
