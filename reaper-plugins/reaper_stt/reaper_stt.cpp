#include <cstring>
#include "stt_stubs.h"
#include "../../sdk/reaper_plugin.h"
#include "../../sdk/reaper_plugin_functions.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

/*
  Simple speech-to-text helper using PCM_source_transfer_t blocks.
  This example feeds blocks into a local STT engine (stubbed) and
  inserts project markers for each recognized word while maintaining
  a parallel text lane for basic word-level search/editing.
*/

// ----------------------------------------------------------------------
// STT engine stub
// ----------------------------------------------------------------------
static std::string run_local_stt(const ReaSample* samples,
                                 int nch, int length, double samplerate)
{
  // In real use, connect to an actual STT engine.
  // Here we return a placeholder string for demonstration.
  (void)samples; (void)nch; (void)length; (void)samplerate;
  return "hello world"; // dummy transcription
}

// ----------------------------------------------------------------------
// Text lane structure
// ----------------------------------------------------------------------
struct WordEntry {
  std::string word;
  double position; // seconds
};

static std::vector<WordEntry> g_lane;

static void lane_add_word(const std::string& word, double position)
{
  g_lane.push_back({word, position});
}

int lane_find_word(const std::string& word)
{
  for (size_t i = 0; i < g_lane.size(); ++i)
    if (g_lane[i].word == word) return (int)i;
  return -1;
}

void lane_replace_word(const std::string& oldWord, const std::string& newWord)
{
  for (auto &w : g_lane)
    if (w.word == oldWord) w.word = newWord;
}

void lane_clear()
{
  g_lane.clear();
}

// ----------------------------------------------------------------------
// Feed a PCM block to STT and insert markers
// ----------------------------------------------------------------------
static void feed_block_to_stt(PCM_source_transfer_t *block, double start_time)
{
  std::string text = run_local_stt(block->samples, block->nch, block->length, block->samplerate);
  std::istringstream iss(text);
  std::string word;
  double word_dur = (double)block->length / block->samplerate; // simple duration
  int word_index = 0;
  while (iss >> word)
  {
    double pos = start_time + word_index * word_dur;
    AddProjectMarker(nullptr, false, pos, pos, word.c_str(), -1);
    lane_add_word(word, pos);
    ++word_index;
  }
}

// ----------------------------------------------------------------------
// Public helper to process a PCM_source
// ----------------------------------------------------------------------
void TranscribeSource(PCM_source *src)
{
  if (!src) return;
  lane_clear();
  const int block_len = 4096;
  std::vector<ReaSample> buffer(block_len * src->GetNumChannels());
  PCM_source_transfer_t block{};
  block.samples = buffer.data();
  block.length = block_len;
  block.nch = src->GetNumChannels();
  block.samplerate = src->GetSampleRate();
  block.time_s = 0.0;
  double t = 0.0;
  while (true)
  {
    block.time_s = t;
    src->GetSamples(&block);
    if (block.samples_out <= 0) break;
    feed_block_to_stt(&block, t);
    t += (double)block.samples_out / block.samplerate;
  }
}

// ----------------------------------------------------------------------
// Search/edit API
// ----------------------------------------------------------------------
int STT_FindWord(const char *word)
{
  return lane_find_word(word ? word : "");
}

void STT_ReplaceWord(const char *oldWord, const char *newWord)
{
  if (!oldWord || !newWord) return;
  lane_replace_word(oldWord, newWord);
}

