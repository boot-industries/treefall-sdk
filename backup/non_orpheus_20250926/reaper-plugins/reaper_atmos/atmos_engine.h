#pragma once

#include "../../sdk/reaper_atmos.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

struct AtmosChannelDestination
{
  bool assigned = false;
  bool is_object = false;
  int index = -1; // bed channel index or object id
};

class AtmosEngine
{
public:
  AtmosEngine();

  void registerSpeakerFormat(const reaper_atmos_speaker_format *fmt);
  bool unregisterSpeakerFormat(const char *name);
  int getSpeakerFormatCount() const;
  const reaper_atmos_speaker_format *getSpeakerFormat(int idx) const;

  void mapChannelToBed(int channel, int bedChannelIndex);
  void mapChannelToObject(int channel, int objectId);
  void clearRouting();

  bool beginFrame(const reaper_atmos_render_frame_t &frame, std::string *error);
  void endFrame();
  bool processBlock(const PCM_source_transfer_t &block, std::string *error);

  bool getRoutingState(reaper_atmos_routing_state_t *state) const;
  int getActiveObjectCount() const;

  void assignTrackObject(MediaTrack *track, int object_id);
  void unassignTrackObject(MediaTrack *track);
  int getTrackObject(MediaTrack *track) const;

  bool exportADM(const std::string &path) const;
  bool exportBWF(const std::string &path) const;

private:
  struct SpeakerFormatStorage
  {
    reaper_atmos_speaker_format view{};
    std::string name;
    std::vector<std::string> channelNames;
    std::vector<const char *> channelNamePtrs;
  };

  struct BedSlot
  {
    int channel_index = -1;
    std::string channel_name;
    reaper_atmos_buffer_t buffer{};
  };

  struct ObjectSlot
  {
    int object_id = -1;
    reaper_atmos_buffer_t buffer{};
  };

  struct FrameState
  {
    bool hasFrame = false;
    double samplerate = 0.0;
    int block_length = 0;
    std::vector<BedSlot> beds;
    std::vector<ObjectSlot> objects;
    std::map<int, size_t> bedLookup;
    std::map<int, size_t> objectLookup;
  };

  struct FrameCapture
  {
    bool valid = false;
    double samplerate = 0.0;
    int frames = 0;
    std::vector<int> bedChannelIndices;
    std::vector<std::string> bedChannelNames;
    std::vector<std::vector<ReaSample>> bedAudio;
    std::vector<int> objectIds;
    std::vector<std::vector<ReaSample>> objectAudio;
  };

  void ensureBuiltinFormatsLocked();
  void addFormatLocked(const reaper_atmos_speaker_format &fmt);
  void ensureChannelMapSize(int nch);
  static bool writeBWFFile(const std::string &path, const FrameCapture &capture);
  static bool writeADMFile(const std::string &path, const FrameCapture &capture);

  mutable std::mutex mutex_;
  bool builtinFormatsLoaded_ = false;
  std::vector<SpeakerFormatStorage> speakerFormats_;
  std::vector<AtmosChannelDestination> channelMap_;
  FrameState frame_;
  FrameCapture capture_;
  std::map<MediaTrack *, int> trackAssignments_;
};

