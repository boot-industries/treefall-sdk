#include "atmos_engine.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

namespace fs = std::filesystem;

namespace
{
reaper_atmos_render_frame_t makeFrame(int frames,
                                      std::vector<ReaSample> &bedBuffer,
                                      std::vector<ReaSample> &objectBuffer,
                                      reaper_atmos_bed_channel_t &bed,
                                      reaper_atmos_object_buffer_t &object)
{
  bedBuffer.assign(frames, 0.0);
  objectBuffer.assign(frames, 0.0);

  bed.channel_index = 0;
  bed.channel_name = "L";
  bed.buffer.data = bedBuffer.data();
  bed.buffer.frames = frames;
  bed.buffer.stride = 1;

  object.object_id = 42;
  object.buffer.data = objectBuffer.data();
  object.buffer.frames = frames;
  object.buffer.stride = 1;

  reaper_atmos_render_frame_t frame{};
  frame.samplerate = 48000.0;
  frame.block_length = frames;
  frame.num_bed_channels = 1;
  frame.bed_channels = &bed;
  frame.num_objects = 1;
  frame.objects = &object;
  return frame;
}

uint32_t readU32(std::istream &is)
{
  unsigned char b[4] = {};
  is.read(reinterpret_cast<char *>(b), 4);
  return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
         (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readU16(std::istream &is)
{
  unsigned char b[2] = {};
  is.read(reinterpret_cast<char *>(b), 2);
  return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

std::vector<ReaSample> makeSamples(int channels, int frames)
{
  std::vector<ReaSample> data(static_cast<size_t>(channels) * frames, 0.0);
  for (int ch = 0; ch < channels; ++ch)
  {
    for (int i = 0; i < frames; ++i)
    {
      data[static_cast<size_t>(ch) * frames + i] = ch * 10.0 + i + 0.25;
    }
  }
  return data;
}
} // namespace

TEST(AtmosEngineTest, RoutesBlockIntoHostBuffers)
{
  AtmosEngine engine;
  engine.clearRouting();
  engine.mapChannelToBed(0, 0);
  engine.mapChannelToObject(1, 42);

  std::vector<ReaSample> bedBuffer;
  std::vector<ReaSample> objectBuffer;
  reaper_atmos_bed_channel_t bed{};
  reaper_atmos_object_buffer_t object{};
  auto frame = makeFrame(8, bedBuffer, objectBuffer, bed, object);
  ASSERT_TRUE(engine.beginFrame(frame, nullptr));

  auto samples = makeSamples(2, frame.block_length);
  PCM_source_transfer_t block{};
  block.samples = samples.data();
  block.nch = 2;
  block.length = frame.block_length;
  block.samples_out = frame.block_length;

  ASSERT_TRUE(engine.processBlock(block, nullptr));

  EXPECT_DOUBLE_EQ(bedBuffer[0], 0.25);
  EXPECT_DOUBLE_EQ(bedBuffer[7], 7.25);
  EXPECT_DOUBLE_EQ(objectBuffer[0], 10.25);
  EXPECT_DOUBLE_EQ(objectBuffer[7], 17.25);

  reaper_atmos_routing_dest_t entries[2];
  reaper_atmos_routing_state_t state{};
  state.destinations = entries;
  state.destinations_capacity = 2;
  ASSERT_TRUE(engine.getRoutingState(&state));
  EXPECT_EQ(state.destinations_count, 2);
  EXPECT_EQ(state.destinations_written, 2);
  EXPECT_EQ(entries[0].destination_index, 0);
  EXPECT_EQ(entries[0].is_object, 0);
  EXPECT_EQ(entries[1].is_object, 1);
  EXPECT_EQ(entries[1].object_id, 42);
  EXPECT_EQ(engine.getActiveObjectCount(), 1);
}

TEST(AtmosEngineTest, ExportBWFProducesWaveHeader)
{
  AtmosEngine engine;
  engine.clearRouting();
  engine.mapChannelToBed(0, 0);
  engine.mapChannelToObject(1, 42);

  std::vector<ReaSample> bedBuffer;
  std::vector<ReaSample> objectBuffer;
  reaper_atmos_bed_channel_t bed{};
  reaper_atmos_object_buffer_t object{};
  auto frame = makeFrame(4, bedBuffer, objectBuffer, bed, object);
  ASSERT_TRUE(engine.beginFrame(frame, nullptr));

  auto samples = makeSamples(2, frame.block_length);
  PCM_source_transfer_t block{};
  block.samples = samples.data();
  block.nch = 2;
  block.length = frame.block_length;
  block.samples_out = frame.block_length;
  ASSERT_TRUE(engine.processBlock(block, nullptr));

  fs::path path = fs::temp_directory_path() / "reaper_atmos_test.wav";
  ASSERT_TRUE(engine.exportBWF(path.string()));

  std::ifstream in(path, std::ios::binary);
  ASSERT_TRUE(in.good());
  char id[4];
  in.read(id, 4);
  EXPECT_EQ(std::string(id, 4), "RIFF");
  uint32_t riffSize = readU32(in);
  EXPECT_GT(riffSize, 0u);
  in.read(id, 4);
  EXPECT_EQ(std::string(id, 4), "WAVE");

  in.read(id, 4);
  EXPECT_EQ(std::string(id, 4), "fmt ");
  EXPECT_EQ(readU32(in), 16u);
  EXPECT_EQ(readU16(in), 3u);
  EXPECT_EQ(readU16(in), 2u);
  EXPECT_EQ(readU32(in), 48000u);
  uint32_t byteRate = readU32(in);
  EXPECT_GT(byteRate, 0u);
  EXPECT_EQ(readU16(in), static_cast<uint16_t>(2 * sizeof(float)));
  EXPECT_EQ(readU16(in), 32u);

  in.read(id, 4);
  EXPECT_EQ(std::string(id, 4), "bext");
  uint32_t bextSize = readU32(in);
  EXPECT_EQ(bextSize, 602u);
  in.seekg(bextSize, std::ios::cur);

  in.read(id, 4);
  EXPECT_EQ(std::string(id, 4), "data");
  uint32_t dataSize = readU32(in);
  EXPECT_EQ(dataSize, static_cast<uint32_t>(frame.block_length * 2 * sizeof(float)));

  in.close();
  fs::remove(path);
}

TEST(AtmosEngineTest, ExportADMContainsObjects)
{
  AtmosEngine engine;
  engine.clearRouting();
  engine.mapChannelToBed(0, 0);
  engine.mapChannelToObject(1, 42);

  std::vector<ReaSample> bedBuffer;
  std::vector<ReaSample> objectBuffer;
  reaper_atmos_bed_channel_t bed{};
  reaper_atmos_object_buffer_t object{};
  auto frame = makeFrame(4, bedBuffer, objectBuffer, bed, object);
  ASSERT_TRUE(engine.beginFrame(frame, nullptr));

  auto samples = makeSamples(2, frame.block_length);
  PCM_source_transfer_t block{};
  block.samples = samples.data();
  block.nch = 2;
  block.length = frame.block_length;
  block.samples_out = frame.block_length;
  ASSERT_TRUE(engine.processBlock(block, nullptr));

  fs::path path = fs::temp_directory_path() / "reaper_atmos_test.adm";
  ASSERT_TRUE(engine.exportADM(path.string()));

  std::ifstream in(path);
  ASSERT_TRUE(in.good());
  std::stringstream contents;
  contents << in.rdbuf();
  std::string text = contents.str();
  EXPECT_THAT(text, testing::HasSubstr("<adm:adm"));
  EXPECT_THAT(text, testing::HasSubstr("Object 42"));
  EXPECT_THAT(text, testing::HasSubstr("DirectSpeakers"));
  EXPECT_THAT(text, testing::HasSubstr("Objects"));

  in.close();
  fs::remove(path);
}

TEST(AtmosEngineTest, SpeakerFormatLifecycle)
{
  AtmosEngine engine;
  int original = engine.getSpeakerFormatCount();
  ASSERT_TRUE(engine.unregisterSpeakerFormat("5.1.4"));
  EXPECT_EQ(engine.getSpeakerFormatCount(), original - 1);

  const char *channels[] = {"A", "B", nullptr};
  reaper_atmos_speaker_format fmt{};
  fmt.name = "Custom";
  fmt.num_channels = 2;
  fmt.channel_names = channels;
  engine.registerSpeakerFormat(&fmt);
  EXPECT_EQ(engine.getSpeakerFormatCount(), original);
}

