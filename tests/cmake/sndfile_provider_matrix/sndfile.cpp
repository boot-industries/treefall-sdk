#include <sndfile.h>
#include <cstdio>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

struct SNDFILE_tag {
  SF_INFO info{};
  sf_count_t position{0};
  bool writing{false};
  std::vector<float> samples;
};

extern "C" SNDFILE* sf_open(const char*, int mode, SF_INFO* info) {
  if (info == nullptr || info->samplerate <= 0 || info->channels <= 0) {
    return nullptr;
  }
  auto* file = new SNDFILE_tag;
  file->info = *info;
  file->writing = mode == SFM_WRITE;
  if (!file->writing) {
    file->info.frames = 4;
    file->samples.assign(static_cast<size_t>(file->info.frames) * file->info.channels, 0.25f);
  }
  *info = file->info;
  return file;
}

extern "C" int sf_close(SNDFILE* file) {
  delete file;
  return 0;
}

extern "C" const char* sf_strerror(const SNDFILE*) { return "fake sndfile error"; }

extern "C" sf_count_t sf_readf_float(SNDFILE* file, float* buffer, sf_count_t frames) {
  if (file == nullptr || buffer == nullptr || frames < 0 || file->writing) {
    return -1;
  }
  const sf_count_t available = std::max<sf_count_t>(0, file->info.frames - file->position);
  const sf_count_t count = std::min(frames, available);
  const size_t samples = static_cast<size_t>(count) * static_cast<size_t>(file->info.channels);
  if (samples > 0) {
    std::memcpy(buffer,
                file->samples.data() + static_cast<size_t>(file->position) * file->info.channels,
                samples * sizeof(float));
  }
  file->position += count;
  return count;
}

extern "C" sf_count_t sf_writef_float(SNDFILE* file, const float* buffer, sf_count_t frames) {
  if (file == nullptr || buffer == nullptr || frames < 0 || !file->writing) {
    return -1;
  }
  const size_t samples = static_cast<size_t>(frames) * static_cast<size_t>(file->info.channels);
  file->samples.insert(file->samples.end(), buffer, buffer + samples);
  file->position += frames;
  file->info.frames = std::max(file->info.frames, file->position);
  return frames;
}

extern "C" sf_count_t sf_seek(SNDFILE* file, sf_count_t frames, int whence) {
  if (file == nullptr || whence != SEEK_SET || frames < 0) {
    return -1;
  }
  file->position = std::min(frames, file->info.frames);
  return file->position;
}

extern "C" int sf_format_check(const SF_INFO* info) {
  return info != nullptr && info->samplerate > 0 && info->channels > 0 ? SF_TRUE : SF_FALSE;
}

extern "C" int sf_command(SNDFILE*, int, void*, int) { return 0; }
